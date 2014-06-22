/* vim: ts=4:sw=4:et:sts=4:ai:
*/

/*
 * Virtual folder plugin for claws-mail
 *
 * Claws Mail is Copyright (C) 1999-2014 by Michael Rasmussen and
 * the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"

#include "gtkutils.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "alertpanel.h"

#include "vfolder_gtk.h"
#include "vfolder.h"
#include "vfolder_prop.h"

#define HEADERS	N_("Message _Headers")
#define BODY	N_("_Message body")
#define BOTH	N_("_Both")

typedef struct {
	GtkWidget* folder_name;
	GtkWidget* filter;
	GtkWidget* frozen;
	GtkWidget* source;
	GtkWidget* label_btn;
	GtkWidget* message_btn;
	GtkWidget* both_btn;
} PropsDialog;

static void add_current_config(VFolderItem* item, PropsDialog* props) {
	if (item->filter)
		gtk_entry_set_text(GTK_ENTRY(props->filter), item->filter);
	if (item->source) {
		gchar* id = folder_item_get_identifier(item->source);
		if (id) {
			gtk_entry_set_text(GTK_ENTRY(props->source), id);
			g_free(id);
		}
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->frozen), item->frozen);
	switch (item->search) {
		case SEARCH_BODY:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->message_btn), TRUE);
		case SEARCH_BOTH:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->both_btn), TRUE);
		case SEARCH_HEADERS:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->label_btn), TRUE);
	}
}

static void foldersel_cb(GtkWidget *widget, gpointer data) {
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	GtkWidget* entry = GTK_WIDGET(data);

	item = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL, FALSE);
	if (item && IS_VFOLDER_FOLDER_ITEM(item)) {
		/* Cannot create virtual folder from virtual folder */
		alertpanel_error(_("%s: Cannot create virtual folder from virtual folder"), item->name);
		return;
	}
	else {
		if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
			gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
			gtk_editable_insert_text(GTK_EDITABLE(entry),
						item_id, strlen(item_id), &newpos);
			g_free(item_id);
		}
		debug_print("Source Folder: %s\n", gtk_entry_get_text(GTK_ENTRY(entry)));
	}
}

static gboolean vfolder_source_widget(GtkWidget* widget) {
	const gchar* name = gtk_widget_get_name(widget);

	return (name && strcmp("source", name) == 0);
}

static GtkWidget* vfolder_prop_row(GtkWidget* widget,
								   const gchar* label_markup,
								   gint width, gboolean center) {
	GtkWidget* row = gtk_hbox_new(FALSE, 5);
	GtkWidget* label = NULL;
	GtkWidget* btn;

	label = gtk_label_new_with_mnemonic(label_markup);
	gtk_widget_set_size_request(label, width, -1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
	gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(row), widget, FALSE, FALSE, 5);

	if (vfolder_source_widget(widget)) {
		btn = gtk_button_new_from_stock(GTK_STOCK_OPEN);
		g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(foldersel_cb), widget);
		gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 5);
	}

	if (GTK_IS_ENTRY(widget))
		gtk_widget_set_size_request(widget, 300, -1);

	return row;
}

static gboolean vfolder_set_search_type(VFolderItem* item, GtkWidget* list, gboolean* update) {
	GSList *btn_list, *btns;
	gboolean active = FALSE;
	GtkToggleButton* btn = NULL;

	btn_list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(list));
	for (btns = btn_list; btns && !active; btns = g_slist_next(btns)) {
		btn = GTK_TOGGLE_BUTTON(btns->data);
		active = gtk_toggle_button_get_active(btn);
	}
	if (active) {
		const gchar* label = gtk_button_get_label(GTK_BUTTON(btn));
		if (label) {
			if (strcmp(BOTH, label) == 0) {
				if (item->search != SEARCH_BOTH) {
					item->search = SEARCH_BOTH;
					return TRUE;
				}
			}
			else if (strcmp(BODY, label) == 0) {
				if (item->search != SEARCH_BODY) {
					item->search = SEARCH_BODY;
					return TRUE;
				}
			}
			else {
				if (item->search != SEARCH_HEADERS) {
					item->search = SEARCH_HEADERS;
					return TRUE;
				}
			}
		}
	}

	return FALSE;
}

static gboolean vfolder_change_source(VFolderItem* vitem, const gchar* str) {
	if (! vitem->source_id && ! str)
		return FALSE;
	if (! vitem->source_id && str)
		return TRUE;
	if (vitem->source_id && ! str)
		return TRUE;
	return (strcmp(vitem->source_id, str)) ? TRUE : FALSE;
}

static gboolean vfolder_search_headers(MsgInfo* msg, GPatternSpec* pattern) {
	return ((msg->cc && g_pattern_match_string(pattern, msg->cc)) ||
			(msg->from && g_pattern_match_string(pattern, msg->from)) ||
			(msg->fromname && g_pattern_match_string(pattern, msg->fromname)) ||
			(msg->inreplyto && g_pattern_match_string(pattern, msg->inreplyto)) ||
			(msg->subject && g_pattern_match_string(pattern, msg->subject)) ||
			(msg->to && g_pattern_match_string(pattern, msg->to)));
}

static gboolean vfolder_search_body(MsgInfo* msg, GPatternSpec* pattern) {
	gchar* body;
	gboolean found = FALSE;

	body = procmsg_get_message_file(msg);
	if (body) {
		found = g_pattern_match_string(pattern, body);
		g_free(body);
	}

	return found;
}

static MsgInfoList* vfolder_filter_msgs_list(MsgInfoList* msgs, VFolderItem* item) {
	MsgInfoList *list = NULL, *tmp;
	GPatternSpec* pattern;
	MsgInfo* msg;

	if (!item || item->filter == NULL)
		return list;

	pattern = g_pattern_spec_new(item->filter);

	for (tmp = msgs; tmp; tmp = g_slist_next(tmp)) {
		msg = (MsgInfo *) tmp->data;
		switch (item->search) {
			case SEARCH_HEADERS:
				if (vfolder_search_headers(msg, pattern))
					list = g_slist_prepend(list, msg);
				break;
			case SEARCH_BODY:
				if (vfolder_search_body(msg, pattern))
					list = g_slist_prepend(list, msg);
				break;
			case SEARCH_BOTH:
				if (vfolder_search_headers(msg, pattern)) {
					list = g_slist_prepend(list, msg);
					continue;
				}
				if (vfolder_search_body(msg, pattern))
					list = g_slist_prepend(list, msg);
				break;
		}
	}

	if (list)
		list = g_slist_reverse(list);

	g_pattern_spec_free(pattern);

	return list;
}

static gboolean vfolder_create_msgs_list(VFolderItem* item) {
	MsgInfoList *msgs = NULL, *filtered = NULL;
	gboolean ok = FALSE;
	GSList* filelist = NULL;

	if (item->filter && item->msg_filter_func) {
		ok = TRUE;
		msgs = folder_item_get_msg_list(item->source);
		filtered = item->msg_filter_func(msgs, item);
		if (filtered) {
			filelist = procmsg_get_message_file_list(filtered);
			if (filelist) {
				gint n = folder_item_add_msgs(FOLDER_ITEM(item), filelist, FALSE);
				FOLDER_ITEM(item)->last_num = n;
				procmsg_message_file_list_free(filelist);
			}
			g_slist_free(filtered);
		}
		procmsg_msg_list_free(msgs);
	}
	return ok;
}

void vfolder_set_msgs_filter(VFolderItem* vfolder_item) {
	cm_return_if_fail(vfolder_item != NULL);

	vfolder_item->msg_filter_func = vfolder_filter_msgs_list;
}

gboolean vfolder_edit_item_dialog(VFolderItem** vitem_ptr, FolderItem* item) {
	gboolean ok = FALSE;
	PropsDialog* props_dialog;
	GtkWidget* dialog;
	GtkWidget* content;
	GtkWidget* row;
	GtkWidget* box;
	gint response;
	gchar *name = NULL, *id = NULL;
	const gchar* str;
	gboolean frozen;
	gchar* old_filter = NULL;
	FolderItem* old_source;
	gboolean update = TRUE;
	VFolderItem* vitem;

	cm_return_val_if_fail(vitem_ptr != NULL, ok);

	vitem = *vitem_ptr;

	MainWindow *mainwin = mainwindow_get_mainwindow();
	props_dialog = g_new0(PropsDialog, 1);
	props_dialog->folder_name = gtk_entry_new();
	CLAWS_SET_TIP(props_dialog->folder_name, _("Name for VFolder"));
	props_dialog->filter = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(props_dialog->filter), TRUE);
	CLAWS_SET_TIP(props_dialog->filter, _("Glob filter. '*' or '?' for wildcard"));
	props_dialog->frozen = gtk_check_button_new();
	CLAWS_SET_TIP(props_dialog->frozen, _("Disable automatic update. Manual refresh is not affected"));
	props_dialog->source = gtk_entry_new();
	gtk_entry_set_activates_default(GTK_ENTRY(props_dialog->source), TRUE);
	CLAWS_SET_TIP(props_dialog->source, _("Folder to watch"));
	props_dialog->label_btn =
		gtk_radio_button_new_with_mnemonic(NULL, HEADERS);
	CLAWS_SET_TIP(props_dialog->label_btn, _("Only scan message headers"));
	props_dialog->message_btn =
		gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(props_dialog->label_btn), BODY);
	CLAWS_SET_TIP(props_dialog->message_btn, _("Only scan message body"));
	props_dialog->both_btn =
		gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(props_dialog->label_btn), BOTH);
	CLAWS_SET_TIP(props_dialog->both_btn, _("Scan both message headers and message body"));
	gtk_widget_set_name(props_dialog->source, "source");

	if (vitem)
		add_current_config(vitem, props_dialog);

	dialog = gtk_dialog_new_with_buttons(
			N_("Edit VFolder Properties"),
			GTK_WINDOW(mainwin->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			NULL);
	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	GtkWidget* vbox = gtk_vbox_new(FALSE, 5);

	row = vfolder_prop_row(props_dialog->folder_name, _("VFolder _name"), 90, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 5);
	row = vfolder_prop_row(props_dialog->source, _("_Source folder"), 90, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 5);

	if (item || (vitem && vitem->source)) {
		if (!item)
			id = folder_item_get_identifier(vitem->source);
		else
			id = folder_item_get_identifier(item);
		gtk_entry_set_text(GTK_ENTRY(props_dialog->source), id);
		gtk_widget_set_sensitive(props_dialog->source, FALSE);
		g_free(id);
	}

	if (vitem) {
		name = folder_item_get_name(FOLDER_ITEM(vitem));
		if (name) {
			gtk_entry_set_text(GTK_ENTRY(props_dialog->folder_name), name);
			gtk_widget_set_sensitive(props_dialog->folder_name, FALSE);
			g_free(name);
		}
	}

	GtkWidget* frame1 = gtk_frame_new(_("Message filter"));
	GtkWidget* vbox1 = gtk_vbox_new(TRUE, 2);
	gtk_container_add(GTK_CONTAINER(frame1), vbox1);

	row = vfolder_prop_row(props_dialog->filter, _("Glob _filter"), 90, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), row, FALSE, FALSE, 5);

	box = gtk_hbox_new(FALSE, 20);
	gtk_box_pack_start(GTK_BOX(box), props_dialog->label_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), props_dialog->message_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box), props_dialog->both_btn, TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(vbox1), box, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), frame1, FALSE, FALSE, 5);

	row = vfolder_prop_row(props_dialog->frozen, _("F_reeze content"), 110, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 5);

	GtkWidget* frame = gtk_frame_new(_("settings"));
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	gtk_container_add(GTK_CONTAINER(content), frame);

	if (vitem)
		gtk_widget_grab_focus(props_dialog->filter);
	else
		gtk_widget_grab_focus(props_dialog->folder_name);
	GtkWidget* ok_btn = gtk_dialog_get_widget_for_response(
					   GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_widget_grab_default(ok_btn);

	gtk_widget_show_all(content);
	response = gtk_dialog_run(GTK_DIALOG(dialog));

	if (response == GTK_RESPONSE_ACCEPT) {
		str = gtk_entry_get_text(GTK_ENTRY(props_dialog->folder_name));
		debug_print("VFolder name from dialog: %s\n", str);
		if (! str) goto error;

		if (vitem)
			name = folder_item_get_name(FOLDER_ITEM(vitem));
		debug_print("VFolder name from VFolder: %s\n", name ? name : "NULL");

		if (! name || g_utf8_collate(name, str)) {
			Folder* folder = folder_find_from_name(VFOLDER_DEFAULT_MAILBOX, vfolder_folder_get_class());
			FolderItem* parent = FOLDER_ITEM(folder->node->data);
			/* find whether the directory already exists */
			if (folder_find_child_item_by_name(parent, str)) {
				alertpanel_error(_("The folder '%s' already exists."), str);
				goto error;
			}
			if (! vitem) {
				debug_print("Create VFolder: %s\n", str);
				vitem = VFOLDER_ITEM(folder_create_folder(parent, str));
				if (!vitem) {
					alertpanel_error(_("Can't create the folder '%s'."), str);
					goto error;
				}
				vfolder_set_msgs_filter(vitem);
				*vitem_ptr = vitem;
			} else {
				debug_print("Rename VFolder to: %s\n", str);
				folder_item_rename(FOLDER_ITEM(vitem), (gchar *) str);
			}
		}

		frozen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(props_dialog->frozen));

		str = gtk_entry_get_text(GTK_ENTRY(props_dialog->filter));
		if (str) {
			old_filter = g_strdup(vitem->filter);
			if (strlen(str) == 0) {
				if (vitem->filter) {
					g_free(vitem->filter);
					vitem->filter = NULL;
					ok = TRUE;
				}
			}
			else {
				if (vitem->filter && strcmp(vitem->filter, str) == 0)
					update = FALSE;
				else {
					g_free(vitem->filter);
					vitem->filter = g_strdup(str);
					ok = TRUE;
				}
			}
		}

		if (vfolder_set_search_type(vitem, props_dialog->label_btn, &update)) {
			update = TRUE;
			ok = TRUE;
		} else {
			update = FALSE;
		}


		str = gtk_entry_get_text(GTK_ENTRY(props_dialog->source));
		if (vfolder_change_source(vitem, str)) {
			if (item) {
				vitem->source = item;
			} else {
				/* remove msgvault */
				vfolder_msgvault_free(vitem->msgvault);
				vitem->msgvault = vfolder_msgvault_new();
			}

			if (strlen(str) == 0) {
				if (vitem->source) {
					vitem->source = NULL;
					folder_item_remove_all_msg(FOLDER_ITEM(vitem));
					ok = TRUE;
				}
			} else {
				old_source = NULL;
				id = (vitem->source) ? folder_item_get_identifier(vitem->source) : NULL;
				if (!id || strcmp(id, str) != 0) {
					old_source = vitem->source;
					vitem->source = folder_get_item_from_identifier(str);
				}
				g_free(id);
				if (vitem->source && (vitem->source->stype != F_NORMAL && vitem->source->stype != F_INBOX)) {
					alertpanel_error(_("%s: Not suitable for virtual folders\n"
									   "Use only folder type: Normal or Inbox\n"), str);
					g_free(vitem->filter);
					vitem->filter = g_strdup(old_filter);
					if (old_source)
						vitem->source = old_source;
					ok = FALSE;
					goto error;
				}
				if (vitem->source_id)
					g_free(vitem->source_id);
				vitem->source_id = folder_item_get_identifier(vitem->source);
				if (FOLDER_ITEM(vitem)->total_msgs > 0)
					folder_item_remove_all_msg(FOLDER_ITEM(vitem));
				ok =  vfolder_create_msgs_list(vitem);
				if (ok == FALSE) {
					g_free(vitem->filter);
					vitem->filter = g_strdup(old_filter);
					goto error;
				}
				vitem->changed = TRUE;
			}
		} else {
			ok = TRUE;
			update = FALSE;
		}

		if (vitem->frozen != frozen) {
			vitem->frozen = frozen;
			ok = TRUE;
			vitem->changed = TRUE;
		}

	}

error:
	gtk_widget_destroy(dialog);
	g_free(props_dialog);
	g_free(old_filter);
	g_free(name);

	return ok;
}

void vfolder_item_props_response(FolderPropsResponse resp) {
	GtkResponseType ret;

	switch (resp) {
		case FOLDER_ITEM_PROPS_OK: break;
		case FOLDER_ITEM_PROPS_BACKUP_FAIL:
			vfolder_msg_dialog(GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							   "Making backup of config failed");
			break;
		case FOLDER_ITEM_PROPS_READ_USING_DEFAULT:
			vfolder_msg_dialog(GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							   "Missing config file. Using defaults");
			break;
		case FOLDER_ITEM_PROPS_NO_ITEM:
			ret = vfolder_msg_dialog(GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
									 "Writing config failed\nContinue?");
			if (ret != GTK_RESPONSE_YES)
				vfolder_done();
			break;
		case FOLDER_ITEM_PROPS_READ_DATA_FAIL:
			vfolder_msg_dialog(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							   "Reading config failed. Unloading plugin");
			vfolder_done();
			break;
		case FOLDER_ITEM_PROPS_MAKE_RC_DIR_FAIL:
			vfolder_msg_dialog(GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
							   "Creating top folder failed. Unloading plugin");
			vfolder_done();
			break;
	}
}
