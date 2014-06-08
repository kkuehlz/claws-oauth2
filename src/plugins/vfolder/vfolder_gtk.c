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

#include "menu.h"
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "defs.h"

#include "mainwindow.h"
#include "inputdialog.h"
#include "folder.h"
#include "folderview.h"
#include "folder_item_prefs.h"
#include "alertpanel.h"
#include "hooks.h"
#include "utils.h"
#include "summaryview.h"

#include "main.h"
#include "gtkutils.h"

#include "vfolder_gtk.h"
#include "vfolder.h"
#include "vfolder_prop.h"

#define CONFIG_GROUP "VFolder"

static guint folder_hook_id;
static guint item_hook_id;
static guint msginfo_hook_id;
static GSList* widgets = NULL;

typedef struct {
	MsgInfoList*	list;
	VFolderItem*	item;
	gchar*			file;
} AddMsgData;

typedef struct {
	GtkWidget*	widget;
	GtkAction*	action;
} MenuItem;

static char* vfolder_popup_menu_labels[] = {
	N_("_Refresh folder"),
	N_("Refresh _all folders"),
	N_("Folder pr_operties..."),
	N_("Rena_me..."),
	N_("_Create new folder..."),
	N_("_Delete folder..."),
	NULL
};

static GtkActionEntry vfolder_popup_entries[] = {
	{"FolderViewPopup/RefreshFolder",		NULL, NULL, NULL, NULL, NULL /*G_CALLBACK(vfolder_refresh_cb)*/ },
	{"FolderViewPopup/RefreshAllFolders",	NULL, NULL, NULL, NULL, NULL /*G_CALLBACK(vfolder_refresh_cb)*/ },

	{"FolderViewPopup/FolderProperties",	NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_properties_cb) },

	{"FolderViewPopup/RenameFolder",	NULL, NULL, NULL, NULL, NULL /*G_CALLBACK(vfolder_refresh_cb)*/ },

	{"FolderViewPopup/NewFolder",		NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_new_folder_cb) },
	{"FolderViewPopup/RemoveFolder",	NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_remove_folder_cb) },
};

static void vfolder_add_menuitems(GtkUIManager *ui_manager, FolderItem *item) {
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RefreshFolder", "FolderViewPopup/RefreshFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RefreshAllFolders", "FolderViewPopup/RefreshAllFolders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVF1", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "FolderProperties", "FolderViewPopup/FolderProperties", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVF2", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RenameFolder", "FolderViewPopup/RenameFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVF3", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "NewFolder", "FolderViewPopup/NewFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveFolder", "FolderViewPopup/RemoveFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorVF4", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
}

static void vfolder_set_sensitivity(GtkUIManager *ui_manager, FolderItem *item) {
#define SET_SENS(name, sens) \
	cm_menu_set_sensitive_full(ui_manager, "Popup/"name, sens)

	VFolderItem *ritem = (VFolderItem *)item;
	SET_SENS("FolderViewPopup/RefreshFolder", folder_item_parent(item) != NULL && ! ritem->frozen);
	SET_SENS("FolderViewPopup/RefreshAllFolders", folder_item_parent(item) == NULL && ! ritem->frozen);
	SET_SENS("FolderViewPopup/FolderProperties", folder_item_parent(item) != NULL);
	SET_SENS("FolderViewPopup/RenameFolder", folder_item_parent(item) != NULL);
	SET_SENS("FolderViewPopup/NewFolder", TRUE);
	SET_SENS("FolderViewPopup/RemoveFolder", folder_item_parent(item) != NULL);

#undef SET_SENS
}

static FolderViewPopup vfolder_popup = {
	"vfolder",
	"<vfolder>",
	vfolder_popup_entries,
	G_N_ELEMENTS(vfolder_popup_entries),
	NULL, 0,
	NULL, 0, 0, NULL,
	vfolder_add_menuitems,
	vfolder_set_sensitivity
};

static void vfolder_fill_popup_menu_labels(void) {
	gint i;

	for (i = 0; vfolder_popup_menu_labels[i] != NULL; i++) {
		(vfolder_popup_entries[i]).label = _(vfolder_popup_menu_labels[i]);
	}
}

static void gslist_menu_item_free(GSList** menu_list) {
	GSList* list;

	if (! menu_list || ! *menu_list)
		return;

	for (list = *menu_list; list; list = g_slist_next(list)) {
		MenuItem* menu = (MenuItem *) list->data;
		g_free(menu);
	}

	g_slist_free(*menu_list);
	*menu_list = NULL;
}

static gboolean get_menu_widgets() {
	MainWindow* mainwindow;
	MenuItem* menuitem = NULL;
	GtkWidget* widget;

	mainwindow = mainwindow_get_mainwindow();
	if (mainwindow && mainwindow->ui_manager) {
		widget = gtk_ui_manager_get_widget(
			mainwindow->ui_manager, "/Menus/SummaryViewPopup/Move/");
		if (widget) {
			menuitem = g_new0(MenuItem, 1);
			menuitem->widget = widget;
			menuitem->action = gtk_ui_manager_get_action(
				mainwindow->ui_manager, "/Menus/SummaryViewPopup/Move/");
			widgets = g_slist_prepend(widgets, menuitem);
		}
		else
			return FALSE;

		widget = gtk_ui_manager_get_widget(
			mainwindow->ui_manager, "/Menus/SummaryViewPopup/Trash/");
		if (widget) {
			menuitem = g_new0(MenuItem, 1);
			menuitem->widget = widget;
			menuitem->action = gtk_ui_manager_get_action(
				mainwindow->ui_manager, "/Menus/SummaryViewPopup/Trash/");
			widgets = g_slist_prepend(widgets, menuitem);
		}
		else {
			gslist_menu_item_free(&widgets);
			return FALSE;
		}

		widget = gtk_ui_manager_get_widget(
			mainwindow->ui_manager, "/Menus/SummaryViewPopup/Delete/");
		if (widget) {
			menuitem = g_new0(MenuItem, 1);
			menuitem->widget = widget;
			menuitem->action = gtk_ui_manager_get_action(
				mainwindow->ui_manager, "/Menus/SummaryViewPopup/Delete/");
			widgets = g_slist_prepend(widgets, menuitem);
		}
		else {
			gslist_menu_item_free(&widgets);
			return FALSE;
		}

		widget = gtk_ui_manager_get_widget(
			mainwindow->ui_manager, "/Menu/Message/Move/");
		if (widget) {
			menuitem = g_new0(MenuItem, 1);
			menuitem->widget = widget;
			menuitem->action = gtk_ui_manager_get_action(
				mainwindow->ui_manager, "/Menu/Message/Move/");
			widgets = g_slist_prepend(widgets, menuitem);
		}
		else {
			gslist_menu_item_free(&widgets);
			return FALSE;
		}

		widget = gtk_ui_manager_get_widget(
			mainwindow->ui_manager, "/Menu/Message/Trash/");
		if (widget) {
			menuitem = g_new0(MenuItem, 1);
			menuitem->widget = widget;
			menuitem->action = gtk_ui_manager_get_action(
				mainwindow->ui_manager, "/Menu/Message/Trash/");
			widgets = g_slist_prepend(widgets, menuitem);
		}
		else {
			gslist_menu_item_free(&widgets);
			return FALSE;
		}

		widget = gtk_ui_manager_get_widget(
			mainwindow->ui_manager, "/Menu/Message/Delete/");
		if (widget) {
			menuitem = g_new0(MenuItem, 1);
			menuitem->widget = widget;
			menuitem->action = gtk_ui_manager_get_action(
				mainwindow->ui_manager, "/Menu/Message/Delete/");
			widgets = g_slist_prepend(widgets, menuitem);
		}
		else {
			gslist_menu_item_free(&widgets);
			return FALSE;
		}

	}
	else
		return FALSE;

	return TRUE;
}
/*
static gboolean vfolder_widgets_is_visible() {
	gboolean visible = TRUE;

	if (widgets && widgets->data) {
		MenuItem* menu = (MenuItem *) widgets->data;
		visible = gtk_widget_get_visible(menu->widget);
	}

	return visible;
}

static gboolean vfolder_hide_widgets(VFolderItem* item) {
	GSList* list;
	MainWindow* mainwindow;

//	if (! item->deep_copy) {
		for (list = widgets; list; list = g_slist_next(list)) {
			MenuItem* menu = (MenuItem *) list->data;
			gtk_widget_hide(menu->widget);
			gtk_action_block_activate(menu->action);
		}

		mainwindow = mainwindow_get_mainwindow();
		if (mainwindow && mainwindow->toolbar) {
			if (mainwindow->toolbar->trash_btn)
				gtk_widget_hide(mainwindow->toolbar->trash_btn);
			if (mainwindow->toolbar->delete_btn)
				gtk_widget_hide(mainwindow->toolbar->delete_btn);
		}
//	}
	return TRUE;
}
*/
static gboolean vfolder_show_widgets(VFolderItem* item) {
	GSList* list;
	MainWindow* mainwindow;

//	if (! item->deep_copy) {
		for (list = widgets; list; list = g_slist_next(list)) {
			MenuItem* menu = (MenuItem *) list->data;
			gtk_widget_show(menu->widget);
			gtk_action_unblock_activate(menu->action);
		}

		mainwindow = mainwindow_get_mainwindow();
		if (mainwindow && mainwindow->toolbar) {
			if (mainwindow->toolbar->trash_btn)
				gtk_widget_show(mainwindow->toolbar->trash_btn);
			if (mainwindow->toolbar->delete_btn)
				gtk_widget_show(mainwindow->toolbar->delete_btn);
		}
//	}
	return TRUE;
}
/*
static gchar* vfolder_get_message_file_path(VFolderItem* item, MsgInfo* msg) {
	gchar* path;
	GSList* list = NULL, *cur;
	Folder* folder;
	gboolean old_uid;
	guint last = 0;

	if (item->deep_copy) {
		path = procmsg_get_message_file_path(msg);
	}
	else {
		gchar* root = folder_item_get_path(msg->to_folder);
		folder = msg->to_folder->folder;
		guint num = folder->klass->get_num_list(folder, msg->to_folder, &list, &old_uid);
		if (num >= 0) {
			for (cur = list, last = 0; cur; cur = g_slist_next(cur)) {
				guint tmp = GPOINTER_TO_UINT(cur->data);
				if (tmp > last)
					last = tmp;
			}
		}
		g_slist_free(list);

		path = g_strdup_printf("%s%s%u", root, G_DIR_SEPARATOR_S, last + 1);
		g_free(root);
	}
	return path;
}
*/

/*
static void vfolder_item_update(AddMsgData* msgdata) {
	MsgInfoList* cur;
	GSList update;
	MsgFileInfo fileinfo;

	if (!msgdata->item || !msgdata->list->data)
		return;

	for (cur = msgdata->list; cur; cur = g_slist_next(cur)) {
		MsgInfo* msg = (MsgInfo *) cur->data;
		if (MSG_IS_DELETED(msg->flags)) {
			folder_item_remove_msg(FOLDER_ITEM(msgdata->item), msg->msgnum);
		}
		else {
			fileinfo.msginfo = msg;
			fileinfo.flags = &msg->flags;
			fileinfo.file = msgdata->file;
			update.data = &fileinfo;
			update.next = NULL;
			folder_item_scan(msg->folder);
			gint n = folder_item_add_msgs(FOLDER_ITEM(msgdata->item), &update, FALSE);
			gchar* p = strrchr(fileinfo.file, G_DIR_SEPARATOR);
			p += 1;
			guint num = to_number((const gchar *) p);
			vfolder_replace_key_in_bridge(msgdata->item, msg->msgnum, num);
			FOLDER_ITEM(msgdata->item)->last_num = n;
		}
	}

	//procmsg_message_file_list_free(list);
	//item->msginfos = folder_item_get_msg_list(FOLDER_ITEM(item));
}
*/
/*
static void add_msg_data_free(AddMsgData** rec) {
	if (rec && *rec) {
		AddMsgData* data = *rec;
		g_slist_free(data->list);
		g_free(data->file);
		g_free(data);
		*rec = NULL;
	}
}
*/
/*
static void vfolder_update_affected_folder_items(MsgInfo* msginfo) {
	GList *vfolders, *cur;
	GSList* cur_msg;
	gchar* src;
	AddMsgData* data;
	MsgInfo* msg;

	if (! msginfo)
		return;

	if (MSG_IS_NEW(msginfo->flags) ||
		MSG_IS_MOVE(msginfo->flags) ||
		MSG_IS_COPY(msginfo->flags) ||
		MSG_IS_DELETED(msginfo->flags)) {
		vfolders = vfolder_get_vfolder_items();
		for (cur = vfolders; cur; cur = g_list_next(cur)) {
			data = g_new0(AddMsgData, 1);
			VFolderItem* vitem = VFOLDER_ITEM(cur->data);
			if (MSG_IS_MOVE(msginfo->flags) || MSG_IS_COPY(msginfo->flags))
				src = folder_item_get_identifier(msginfo->to_folder);
			else
				src = folder_item_get_identifier(msginfo->folder);
			gchar* shadow = folder_item_get_identifier(vitem->source);
			debug_print("cmp %s : %s\n", src, shadow);
			if (src && shadow && strcmp(src, shadow) == 0) {
				if (MSG_IS_DELETED(msginfo->flags)) {
					msg = vfolder_find_msg_from_claws_num(vitem, msginfo->msgnum);
					if (msg)
						data->list = g_slist_append(data->list, msg);
					else {
						add_msg_data_free(&data);
						g_slist_free(add_msg_data);
						add_msg_data = NULL;
						g_free(src);
						g_free(shadow);
						return;
					}
				}
				else {
					data->list = g_slist_append(data->list, msginfo);
					data->item = vitem;
					data->file = vfolder_get_message_file_path(vitem, msginfo);
					add_msg_data = g_slist_prepend(add_msg_data, data);
				}
				if (data->list && MSG_IS_DELETED(msginfo->flags)) {
					GSList* list = vfolder_filter_msgs_list(data->list, vitem);
					if (list && list->data) {
						MsgInfo* msg = (MsgInfo *) list->data;
						MSG_SET_PERM_FLAGS(msg->flags, MSG_DELETED);
					}
					g_slist_free(data->list);
					data->list = list;
					data->item = vitem;
					vfolder_item_update(data);
					add_msg_data_free(&data);
					g_slist_free(add_msg_data);
					add_msg_data = NULL;
				}
			}
			g_free(src);
			g_free(shadow);
		}
	}
	if (add_msg_data) {
		for (cur_msg = add_msg_data; cur_msg; cur_msg = g_slist_next(cur_msg)) {
			data = (AddMsgData *) cur_msg->data;
			GSList* list = vfolder_filter_msgs_list(data->list, data->item);
			g_slist_free(data->list);
			data->list = list;
			vfolder_item_update(data);
			add_msg_data_free(&data);
		}
		g_slist_free(add_msg_data);
		add_msg_data = NULL;
	}
}
*/
static gboolean vfolder_folder_update_hook(gpointer source, gpointer data) {
	FolderUpdateData* hookdata;

	g_return_val_if_fail(source != NULL, FALSE);
	hookdata = (FolderUpdateData *) source;

	if (! hookdata->folder || IS_VFOLDER_FOLDER(hookdata->folder))
		return FALSE;

	if (hookdata->update_flags & FOLDER_REMOVE_FOLDERITEM) {
		/* TODO: check if the removed folder item is foundation for vfolder */
		debug_print("FOLDER_REMOVE_FOLDERITEM\n");
	}

	if (hookdata->update_flags & FOLDER_REMOVE_FOLDER) {
		/* TODO: check if the removed folder is foundation for vfolder */
		debug_print("FOLDER_REMOVE_FOLDER\n");
	}

	if (hookdata->update_flags & FOLDER_TREE_CHANGED) {
		/* TODO: check if the removed folder is foundation for vfolder */
		debug_print("FOLDER_TREE_CHANGED\n");
	}

	if (hookdata->update_flags & FOLDER_RENAME_FOLDERITEM) {
		/* TODO: check if the removed folder is foundation for vfolder */
		debug_print("FOLDER_RENAME_FOLDERITEM\n");
	}

	return FALSE;
}

static gboolean vfolder_folder_item_update_hook(gpointer source, gpointer data) {
	FolderItemUpdateData* hookdata;
//	gint save_state = -1;
	GList *items = NULL, *cur;
//	gboolean r;
//	MainWindow* mainwindow;

	g_return_val_if_fail(source != NULL, FALSE);
	hookdata = (FolderItemUpdateData *) source;

	if (! hookdata->item || IS_VFOLDER_FOLDER_ITEM(hookdata->item))
		return FALSE;

	if (hookdata->update_flags & F_ITEM_UPDATE_REMOVEMSG ) {
		debug_print("F_ITEM_UPDATE_REMOVEMSG\n");
		//items = vfolder_get_vfolder_from_source(hookdata->item);
		if (items) {
			for (cur = items; cur; cur = g_list_next(cur)) {
				//vfolder_folder_item_update_msgs(VFOLDER_ITEM(cur->data), F_ITEM_UPDATE_REMOVEMSG);
			}
			g_list_free(items);
		}
	}

	else if (hookdata->update_flags & F_ITEM_UPDATE_CONTENT) {
		debug_print("F_ITEM_UPDATE_CONTENT\n");
		//items = vfolder_get_vfolder_from_source(hookdata->item);
		if (items) {
			for (cur = items; cur; cur = g_list_next(cur)) {
				//vfolder_folder_item_update_msgs(VFOLDER_ITEM(cur->data), F_ITEM_UPDATE_CONTENT);
			}
			g_list_free(items);
		}
		//mainwindow = mainwindow_get_mainwindow();
		//summary_execute(mainwindow->summaryview);
	}

	else if (hookdata->update_flags & F_ITEM_UPDATE_ADDMSG) {
		debug_print("F_ITEM_UPDATE_ADDMSG\n");
		//items = vfolder_get_vfolder_from_source(hookdata->item);
		if (items) {
			for (cur = items; cur; cur = g_list_next(cur)) {
				//vfolder_folder_item_update_msgs(VFOLDER_ITEM(cur->data), F_ITEM_UPDATE_ADDMSG);
			}
			g_list_free(items);
		}
	}

	else if (hookdata->update_flags & F_ITEM_UPDATE_MSGCNT) {
		debug_print("F_ITEM_UPDATE_MSGCNT\n");
/*		if (IS_VFOLDER_FOLDER_ITEM(item)) {

			if (! (VFOLDER_ITEM(item))->deep_copy) {
				if (! (VFOLDER_ITEM(item))->active) {
					r = vfolder_hide_widgets(VFOLDER_ITEM(item));
					if (r)
						VFOLDER_ITEM(item)->active = TRUE;
				}
				else {
					r = vfolder_show_widgets(VFOLDER_ITEM(item));
					if (r)
						VFOLDER_ITEM(item)->active = FALSE;
				}

				if (r)
					save_state = 1;
				else
					save_state = 0;
			}
			else
				vfolder_show_widgets(VFOLDER_ITEM(item));
		}
		else {
			if (!vfolder_widgets_is_visible())
				vfolder_show_widgets(VFOLDER_ITEM(item));
		}*/
/*
		items = vfolder_get_vfolder_from_source(hookdata->item);
		if (items) {
			for (cur = items; cur; cur = g_list_next(cur)) {
				vfolder_folder_item_update_msgs(VFOLDER_ITEM(cur->data), F_ITEM_UPDATE_MSGCNT);
			}
			g_list_free(items);
		}
*/
	}

	else if (hookdata->update_flags & F_ITEM_UPDATE_NAME) {
		/* TODO: need update? */
		debug_print("F_ITEM_UPDATE_NAME\n");
		//items = vfolder_get_vfolder_from_source(hookdata->item);
		if (items) {
			for (cur = items; cur; cur = g_list_next(cur)) {
				//vfolder_folder_item_update_msgs(VFOLDER_ITEM(cur->data), F_ITEM_UPDATE_NAME);
			}
			g_list_free(items);
		}
	}

	else {
		/* Unhandled callback */
		debug_print("Unhandled FolderItem callback\n");
	}
/*
	if (!save_state) {
		MainWindow* mainwindow = mainwindow_get_mainwindow();
		alertpanel_error(_("%s: Could not hide dangerous actions"), hookdata->item->name);
		summary_lock(mainwindow->summaryview);
	}
*/
	return FALSE;
}
/*
static gboolean vfolder_msg_info_update_hook(gpointer source, gpointer data) {
	MsgInfoUpdate* hookdata;
	MainWindow* mainwindow;
	MsgInfo* msginfo;

	g_return_val_if_fail(source != NULL, FALSE);
	hookdata = (MsgInfoUpdate *) source;
	msginfo = hookdata->msginfo;

	g_return_val_if_fail(msginfo != NULL, TRUE);

	if (IS_VFOLDER_MSGINFO(msginfo))
		return FALSE;

	debug_print("\n\nPermflag: %u Tmpflag: %u (scanned: %u)\n\n\n",
		(guint32) msginfo->flags.perm_flags, (guint32) msginfo->flags.tmp_flags, 1U << 31);
	if (MSG_IS_NEW(msginfo->flags)) {
		debug_print("MSG_IS_NEW\n");
		vfolder_update_affected_folder_items(msginfo);
		mainwindow = mainwindow_get_mainwindow();
		summary_execute(mainwindow->summaryview);
	}

	if (MSG_IS_DELETED(msginfo->flags)) {
		debug_print("MSG_IS_DELETED\n");
		vfolder_update_affected_folder_items(msginfo);
		mainwindow = mainwindow_get_mainwindow();
		summary_execute(mainwindow->summaryview);
	}

	if (MSG_IS_MOVE(msginfo->flags)) {
		debug_print("MSG_IS_MOVE\n");
		vfolder_update_affected_folder_items(msginfo);
		mainwindow = mainwindow_get_mainwindow();
		summary_execute(mainwindow->summaryview);
	}

	if (MSG_IS_COPY(msginfo->flags)) {
		debug_print("MSG_IS_COPY\n");
		vfolder_update_affected_folder_items(msginfo);
		mainwindow = mainwindow_get_mainwindow();
		summary_execute(mainwindow->summaryview);
	}

//	if (MSG_IS_POSTFILTERED(msginfo->flags)) {
//		debug_print("MSG_IS_POSTFILTERED\n");
//		vfolder_update_affected_folder_items(msginfo);
//		mainwindow = mainwindow_get_mainwindow();
//		summary_execute(mainwindow->summaryview);
//	}


	return FALSE;
}
*/
static gchar* vfolder_get_rc_file(VFolderItem* item) {
	gchar* (*item_get_path)	(Folder* folder, FolderItem* item);
	gchar* path;
	gchar* rc_file;

	item_get_path = FOLDER_ITEM(item)->folder->klass->item_get_path;

	path = item_get_path(FOLDER_ITEM(item)->folder, FOLDER_ITEM(item));
	rc_file = g_strconcat(path, G_DIR_SEPARATOR_S, "folderitemrc", NULL);
	g_free(path);

	return rc_file;
}

FolderPropsResponse vfolder_folder_item_props_write(VFolderItem* item) {
	gchar* rc_file;
	GKeyFile* config;
	gchar* id;
	gchar* data = NULL;
	FILE* fp;
	gsize len = 0;
	FolderPropsResponse resp = FOLDER_ITEM_PROPS_NO_ITEM;
/*	gchar* numstr;
	GHashTableIter iter;
	gpointer key, value;*/

	g_return_val_if_fail(item != NULL, resp);

	rc_file = vfolder_get_rc_file(item);
	config = g_key_file_new();

	if (item->filter)
		g_key_file_set_string(config, CONFIG_GROUP, "filter", item->filter);

	g_key_file_set_boolean(config, CONFIG_GROUP, "frozen", item->frozen);

	if (item->source) {
		id = folder_item_get_identifier(item->source);
		if (id) {
			g_key_file_set_string(config, CONFIG_GROUP, "source", id);
			g_free(id);
		}
	}

/*	if (item->claws_to_me && item->me_to_claws) {
		numstr = NULL;
		g_hash_table_iter_init(&iter, item->claws_to_me);
		while (g_hash_table_iter_next(&iter, &key, &value)) {
			len++;
			MsgBridge* bridge = value;
			if (numstr) {
				gchar* tmp = g_strdup(numstr);
				g_free(numstr);
				numstr = g_strdup_printf("%s, %u:%u",
					tmp, bridge->my_num, bridge->claws_num);
				g_free(tmp);
			}
			else
				numstr = g_strdup_printf("%u:%u", bridge->my_num, bridge->claws_num);
		}

		g_key_file_set_string(config, CONFIG_GROUP, "file_id_list", numstr);
		g_free(numstr);
	}*/

	if (g_file_test(rc_file, G_FILE_TEST_EXISTS)) {
		gchar* bakpath = g_strconcat(rc_file, ".bak", NULL);
		if (g_rename(rc_file, bakpath) < 0) {
			g_warning("%s: Could not create", bakpath);
			resp = FOLDER_ITEM_PROPS_BACKUP_FAIL;
		}
		g_free(bakpath);
	}

//	g_key_file_set_integer(config, CONFIG_GROUP, "filter-function", item->filter_func);

	data = g_key_file_to_data(config, &len, NULL);
	if (len < 1) {
		g_warning("Could not get config data");
		resp = FOLDER_ITEM_PROPS_READ_DATA_FAIL;
	}
	else {
		fp = g_fopen(rc_file, "w");
		if (fp == NULL) {
			gchar* dir_path_end = g_strrstr(rc_file, G_DIR_SEPARATOR_S);
			gchar* rc_dir = g_strndup(rc_file, dir_path_end - rc_file);
			debug_print("rc_dir: %s\n", rc_dir);
			int r = g_mkdir_with_parents(rc_dir, 0700);
			if (r != 0)
				resp = FOLDER_ITEM_PROPS_MAKE_RC_DIR_FAIL;
			g_free(rc_dir);
			if (resp == FOLDER_ITEM_PROPS_MAKE_RC_DIR_FAIL)
				goto error;
			fp = g_fopen(rc_file, "w");
			if (fp == NULL) {
				resp = FOLDER_ITEM_PROPS_MAKE_RC_DIR_FAIL;
				goto error;
			}
		}
		fwrite(data, len, 1, fp);
		fclose(fp);
		resp = FOLDER_ITEM_PROPS_OK;
	}

error:
	g_free(data);

	g_key_file_free(config);
	g_free(rc_file);

	return resp;
}

FolderPropsResponse vfolder_folder_item_props_read(VFolderItem* item) {
	gchar* rc_file;
	GKeyFile* config;
	GError* error = NULL;
	gchar *id, *msgnums;
	gchar **list, **head;
	FolderPropsResponse resp = FOLDER_ITEM_PROPS_NO_ITEM;
	gint lastnum;

	g_return_val_if_fail(item != NULL, resp);

	rc_file = vfolder_get_rc_file(item);
	config = g_key_file_new();

	if (g_file_test(rc_file, G_FILE_TEST_EXISTS)) {
		g_key_file_load_from_file(config, rc_file, G_KEY_FILE_KEEP_COMMENTS, &error);
		if (error) {
			g_warning("%s. Using defaults", error->message);
			g_error_free(error);
			resp = FOLDER_ITEM_PROPS_READ_USING_DEFAULT;
		}
		else {
			item->filter = g_key_file_get_string(config, CONFIG_GROUP, "filter", NULL);
//			item->search = g_key_file_get_integer(config, CONFIG_GROUP, "searchtype", NULL);
			item->frozen = g_key_file_get_boolean(config, CONFIG_GROUP, "frozen", NULL);
//			item->deep_copy = g_key_file_get_boolean(config, CONFIG_GROUP, "deep_copy", NULL);
//			item->filter_func = g_key_file_get_integer(config, CONFIG_GROUP, "filter_function", NULL);

			id = g_key_file_get_string(config, CONFIG_GROUP, "source", NULL);
			if (id) {
				item->source = folder_find_item_from_identifier(id);
				g_free(id);
			}
			msgnums = g_key_file_get_string(config, CONFIG_GROUP, "file_id_list", NULL);
			if (msgnums) {
				list = g_strsplit(msgnums, ",", 0);
				head = list;
				lastnum = -1;
				while (*list) {
					/*gchar* anum = g_strdup(*list++);
					g_strstrip(anum);
					MsgBridge* bridge = vfolder_split_file_id(anum);
					g_free(anum);
					if (lastnum < (gint) bridge->my_num)
						lastnum = bridge->my_num;
					if (bridge->my_num > 0) {
						vfolder_add_message_to_bridge(item, bridge);
					}
					g_free(bridge);*/
				}
				FOLDER_ITEM(item)->last_num = lastnum;
				g_strfreev(head);
				g_free(msgnums);
			}
			resp = FOLDER_ITEM_PROPS_OK;
		}
	}

	g_key_file_free(config);
	g_free(rc_file);

	return resp;
}

gboolean vfolder_gtk_init(gchar** error) {
	vfolder_fill_popup_menu_labels();
	folderview_register_popup(&vfolder_popup);

	folder_hook_id = hooks_register_hook(FOLDER_UPDATE_HOOKLIST,
		vfolder_folder_update_hook, NULL);
	if (folder_hook_id == -1) {
		*error = g_strdup(_("Failed to register folder update hook"));
		return FALSE;
	}

	item_hook_id = hooks_register_hook(FOLDER_ITEM_UPDATE_HOOKLIST,
		vfolder_folder_item_update_hook, NULL);
	if (item_hook_id == -1) {
		*error = g_strdup(_("Failed to register folder item update hook"));
		hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, folder_hook_id);
		return FALSE;
	}

/*	msginfo_hook_id = hooks_register_hook(MSGINFO_UPDATE_HOOKLIST,
		vfolder_msg_info_update_hook, NULL);
	if (msginfo_hook_id == -1) {
		*error = g_strdup(_("Failed to register message info update hook"));
		hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, folder_hook_id);
		hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, item_hook_id);
		return FALSE;
	}*/

	if (! get_menu_widgets()) {
		*error = g_strdup(_("Failed to get menu widgets"));
		hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, folder_hook_id);
		hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, item_hook_id);
		hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, msginfo_hook_id);
		return FALSE;
	}

	return TRUE;
}

void vfolder_gtk_done(void) {
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderView *folderview = NULL;
	FolderItem *fitem = NULL;

	hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, folder_hook_id);
	hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, item_hook_id);
	//hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, msginfo_hook_id);

	if (mainwin == NULL || claws_is_exiting())
		return;

	folderview = mainwin->folderview;
	fitem = folderview->summaryview->folder_item;

	if (fitem && IS_VFOLDER_FOLDER_ITEM(fitem)) {
		vfolder_show_widgets(VFOLDER_ITEM(fitem));
		gslist_menu_item_free(&widgets);

		folderview_unselect(folderview);
		summary_clear_all(folderview->summaryview);
	}

	folderview_unregister_popup(&vfolder_popup);
}

void vfolder_properties_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;

	g_return_if_fail(folderview != NULL);

	item = folderview_get_selected_item(folderview);

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	if (vfolder_edit_item_dialog(VFOLDER_ITEM(item))) {
		/* TODO: update */
		if (debug_get_mode()) {
//			GHashTableIter iter;
//			gpointer key, value;

/*			g_hash_table_iter_init(&iter, VFOLDER_ITEM(item)->me_to_claws);
			while (g_hash_table_iter_next(&iter, &key, &value)) {
				gchar* buf = g_new0(gchar, BUFSIZ);
				MsgInfo* msginfo = vfolder_find_msg_from_vfolder_num(
					VFOLDER_ITEM(item), GPOINTER_TO_UINT(key));
				FILE* msg = procmsg_open_message(msginfo);
				while (fread(buf, 1, BUFSIZ - 1, msg) > 0) {
					fprintf(stderr, "%s", buf);
					g_free(buf);
					buf = g_new0(gchar, BUFSIZ);
				}
				fprintf(stderr, "\n");
				if (buf)
					g_free(buf);
				fclose(msg);
			}*/
		}
		vfolder_folder_item_props_write(VFOLDER_ITEM(item));
	}
}

void vfolder_new_folder_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree = NULL;
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;

	if (!folderview->selected) return;
    if (!GTK_IS_CMCTREE(folderview->ctree)) return;

    ctree = GTK_CMCTREE(folderview->ctree);
	item = gtk_cmctree_node_get_row_data(ctree, folderview->selected);
    //item = folderview_get_selected_item(folderview);
	if (! item) {
		//item = FOLDER_ITEM(vfolder_get_vfolder_item(NULL));
	}

    cm_return_if_fail(item != NULL);
    cm_return_if_fail(item->folder != NULL);

	if (item->no_sub) {
		alertpanel_error(N_("Virtual folders cannot contain subfolders"));
		return;
	}

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if (p) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	name = trim_string(new_folder, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});

	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(item, new_folder)) {
		alertpanel_error(_("The folder '%s' already exists."), name);
		return;
	}

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder '%s'."), name);
		return;
	}

	if (! vfolder_create_item_dialog(new_item)) {
		//VFolderItem* vitem = VFOLDER_ITEM(new_item);
		new_item->folder->klass->remove_folder(new_item->folder, new_item);
		new_item = NULL;
		return;
	}

	folder_write_list();
}

void vfolder_remove_folder_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree = GTK_CMCTREE(folderview->ctree);
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_path = NULL;
	gchar *old_id;

	/* Silence lame warnings */
	old_id = (old_path) ? NULL : old_path;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folders and messages under '%s' will be permanently deleted. "
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel_full(_("Delete folder"), message,
				 GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL, FALSE,
				 NULL, ALERT_WARNING, G_ALERTDEFAULT);
	g_free(message);
	if (avalue != G_ALERTALTERNATE) return;

	Xstrdup_a(old_path, item->path, return);
	old_id = folder_item_get_identifier(item);

	if (folderview->opened == folderview->selected ||
	    gtk_cmctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	if (item->folder->klass->remove_folder(item->folder, item) < 0) {
		folder_item_scan(item);
		alertpanel_error(_("Can't remove the folder '%s'."), name);
		g_free(old_id);
		return;
	}

	folder_write_list();

	g_free(old_id);
}
