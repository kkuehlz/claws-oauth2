/*
 * $Id: $
 */
/* vim:et:ts=4:sw=4:et:sts=4:ai:set list listchars=tab\:»·,trail\:·: */

/*
 * Virtual folder plugin for claws-mail
 *
 * Claws Mail is Copyright (C) 1999-2012 by the Claws Mail Team
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
	GtkWidget* filter;
	GtkWidget* frozen;
	GtkWidget* deep_copy;
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
/*	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->deep_copy), item->deep_copy);
	switch (item->search) {
		case SEARCH_BODY:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->message_btn), TRUE);
		case SEARCH_BOTH:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->both_btn), TRUE);
		case SEARCH_HEADERS:
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props->label_btn), TRUE);
	}*/
}

static gboolean is_source_widget(GtkWidget* widget) {
	const gchar* name = gtk_widget_get_name(widget);

	return (name && strcmp("source", name) == 0);
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

static GtkWidget* vfolder_prop_row(GtkWidget* widget,
								   const gchar* label_markup,
								   gint width, gboolean center) {
	GtkWidget* row = gtk_hbox_new(FALSE, 5);
	GtkWidget* label = gtk_label_new(NULL);

	gtk_widget_set_size_request(label, width, -1);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), label_markup);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
	gtk_box_pack_start(GTK_BOX(row), label, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(row), widget, TRUE, FALSE, 5);

	if (is_source_widget(widget)) {
		GtkWidget* btn = gtk_button_new_from_stock(GTK_STOCK_OPEN);
		g_signal_connect(G_OBJECT(btn), "clicked", G_CALLBACK(foldersel_cb), widget);
		gtk_box_pack_start(GTK_BOX(row), btn, FALSE, FALSE, 5);
	}

	return row;
}

static gboolean vfolder_set_search_type(VFolderItem* item, GtkWidget* list) {
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
/*			if (strcmp(BOTH, label) == 0) {
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
			}*/
		}
	}

	return FALSE;
}
/*
static void vfolder_copy_msginfo_list(gpointer data, gpointer user_data) {
	MsgInfo* msg = (MsgInfo *) data;
	MsgInfo* new_msg;
	VFolderItem* item = (VFolderItem *) user_data;

	g_return_if_fail(msg != NULL);
	g_return_if_fail(item != NULL);

	new_msg = procmsg_msginfo_copy(msg);
	item->msginfos = g_slist_prepend(item->msginfos, new_msg);
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
*/
static MsgInfoList* vfolder_filter_msgs_list(MsgInfoList* msgs, VFolderItem* item) {
	MsgInfoList *list = NULL, *tmp;
	GPatternSpec* pattern;
	MsgInfo* msg;

	if (!item || item->filter == NULL)
		return list;

	pattern = g_pattern_spec_new(item->filter);

	for (tmp = msgs; tmp; tmp = g_slist_next(tmp)) {
		msg = (MsgInfo *) tmp->data;
/*		switch (item->search) {
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
		}*/
	}

	g_pattern_spec_free(pattern);

	return list;
}

static gboolean vfolder_create_msgs_list(VFolderItem* item, gboolean copy) {
	MsgInfoList *msgs = NULL, *filtered = NULL;
	gboolean ok = FALSE;
	GSList* filelist = NULL;

	if (item->filter && item->msg_filter_func) {
//		item->deep_copy = copy;
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
		g_slist_free(msgs);
	}
	return ok;
}

void vfolder_set_msgs_filter(VFolderItem* vfolder_item) {
	g_return_if_fail(vfolder_item != NULL);

	vfolder_item->msg_filter_func = vfolder_filter_msgs_list;
}

gboolean vfolder_create_item_dialog(FolderItem* folder_item) {
	gboolean created = FALSE;
	VFolderItem* item = NULL;

	g_return_val_if_fail(folder_item != NULL, created);
	g_return_val_if_fail(IS_VFOLDER_FOLDER_ITEM(folder_item), created);

	item = VFOLDER_ITEM(folder_item);
	item->msg_filter_func = vfolder_filter_msgs_list;

	if (vfolder_edit_item_dialog(item)) {
		/* save properties */
		if (FOLDER_ITEM_PROPS_OK != vfolder_folder_item_props_write(item))
			created = FALSE;
		else
			created = TRUE;
	}

	return created;
}

gboolean vfolder_edit_item_dialog(VFolderItem* vfolder_item) {
	gboolean ok = FALSE;
	PropsDialog* props_dialog;
	GtkWidget* dialog;
	GtkWidget* content;
	GtkWidget* row;
	GtkWidget* box;
	gint response;
	gchar* name;
	const gchar* str;
	gboolean frozen, deep_copy;
	FolderItem* source;
	gchar* old_filter = NULL;

	g_return_val_if_fail(vfolder_item != NULL, ok);

	MainWindow *mainwin = mainwindow_get_mainwindow();
	props_dialog = g_new0(PropsDialog, 1);
	props_dialog->filter = gtk_entry_new();
	props_dialog->frozen = gtk_check_button_new();
	props_dialog->deep_copy = gtk_check_button_new();
	props_dialog->source = gtk_entry_new();
	props_dialog->label_btn =
		gtk_radio_button_new_with_mnemonic(NULL, HEADERS);
	props_dialog->message_btn =
		gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(props_dialog->label_btn), BODY);
	props_dialog->both_btn =
		gtk_radio_button_new_with_mnemonic_from_widget(
			GTK_RADIO_BUTTON(props_dialog->label_btn), BOTH);
	gtk_widget_set_name(props_dialog->source, "source");
	add_current_config(vfolder_item, props_dialog);

	dialog = gtk_dialog_new_with_buttons(
			N_("Edit VFolder Properties"),
			GTK_WINDOW(mainwin->window),
			GTK_DIALOG_DESTROY_WITH_PARENT,
			GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_REJECT);
	content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	GtkWidget* vbox = gtk_vbox_new(FALSE, 5);

	row = vfolder_prop_row(props_dialog->source, N_("_Source folder"), 110, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 5);

	GtkWidget* frame1 = gtk_frame_new(_("Message filter"));
	GtkWidget* vbox1 = gtk_vbox_new(TRUE, 2);
	gtk_container_add(GTK_CONTAINER(frame1), vbox1);

	row = vfolder_prop_row(props_dialog->filter, N_("_Filter"), 110, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), row, FALSE, FALSE, 5);

	box = gtk_hbox_new(TRUE, 2);
	gtk_box_pack_start(GTK_BOX(box), props_dialog->label_btn, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(box), props_dialog->message_btn, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(box), props_dialog->both_btn, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(vbox1), box, FALSE, FALSE, 5);

	gtk_box_pack_start(GTK_BOX(vbox), frame1, FALSE, FALSE, 5);

	row = vfolder_prop_row(props_dialog->frozen, N_("F_reeze content"), 110, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 5);

	row = vfolder_prop_row(props_dialog->deep_copy, N_("Co_py messages"), 110, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 5);

	name = g_strconcat(FOLDER_ITEM(vfolder_item)->name, N_(": settings"), NULL);
	GtkWidget* frame = gtk_frame_new(name);
	g_free(name);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show_all(frame);

	gtk_container_add(GTK_CONTAINER(content), frame);

	response = gtk_dialog_run(GTK_DIALOG(dialog));
	if (response == GTK_RESPONSE_ACCEPT) {
		frozen = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(props_dialog->frozen));
		deep_copy = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(props_dialog->deep_copy));

		str = gtk_entry_get_text(GTK_ENTRY(props_dialog->filter));
		if (str) {
			old_filter = g_strdup(vfolder_item->filter);
			if (strlen(str) == 0) {
				if (vfolder_item->filter) {
					g_free(vfolder_item->filter);
					vfolder_item->filter = NULL;
					ok = TRUE;
				}
			}
			else {
				if (!vfolder_item->filter || strcmp(vfolder_item->filter, str) != 0) {
					g_free(vfolder_item->filter);
					vfolder_item->filter = g_strdup(str);
					ok = TRUE;
				}
			}
		}
		if (vfolder_set_search_type(vfolder_item, props_dialog->label_btn))
			ok = TRUE;

		str = gtk_entry_get_text(GTK_ENTRY(props_dialog->source));
		if (str) {
			source = folder_find_item_from_identifier(str);
			if (source && (source->stype != F_NORMAL && source->stype != F_INBOX)) {
				alertpanel_error(_("%s: Not suitable for virtual folders\n"
								   "Use only folder type: Normal or Inbox\n"), str);
				g_free(vfolder_item->filter);
				vfolder_item->filter = g_strdup(old_filter);
				ok = FALSE;
				goto error;
			}

			if (strlen(str) == 0) {
				if (vfolder_item->source) {
					vfolder_item->source = NULL;
					folder_item_remove_all_msg(FOLDER_ITEM(vfolder_item));
					ok = TRUE;
				}
			}
			else {
				folder_item_remove_all_msg(FOLDER_ITEM(vfolder_item));
				gchar* id = (vfolder_item->source) ?
					folder_item_get_identifier(vfolder_item->source) : NULL;
				if (!id || strcmp(id, str) != 0)
					vfolder_item->source = source;
				if (vfolder_item->source) {
					ok =  vfolder_create_msgs_list(vfolder_item, deep_copy);
					if (ok == FALSE) {
						g_free(vfolder_item->filter);
						vfolder_item->filter = g_strdup(old_filter);
						goto error;
					}
				}
				else {
					g_free(vfolder_item->filter);
					vfolder_item->filter = g_strdup(old_filter);
					ok = FALSE;
					goto error;
				}
			}
		}

		if (vfolder_item->frozen != frozen) {
			vfolder_item->frozen = frozen;
			ok = TRUE;
		}

/*		if (vfolder_item->deep_copy != deep_copy) {
			vfolder_item->deep_copy = deep_copy;
			ok = TRUE;
		}*/

	}

error:
	gtk_widget_destroy(dialog);
	g_free(props_dialog);
	g_free(old_filter);

	return ok;
}
