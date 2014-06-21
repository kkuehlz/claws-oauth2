/*
 * $Id: $
 */
/* vim:et:ts=4:sw=4:et:sts=4:ai:set list listchars=tab\:»·,trail\:·: */

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
	{"FolderViewPopup/RefreshFolder",		NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_refresh_cb) },
	{"FolderViewPopup/RefreshAllFolders",	NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_refresh_all_cb) },

	{"FolderViewPopup/FolderProperties",	NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_properties_cb) },

	{"FolderViewPopup/RenameFolder",	NULL, NULL, NULL, NULL, G_CALLBACK(vfolder_rename_cb) },

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

	SET_SENS("FolderViewPopup/RefreshFolder", folder_item_parent(item) != NULL);
	SET_SENS("FolderViewPopup/RefreshAllFolders", folder_item_parent(item) == NULL);
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

static gboolean vfolder_folder_update_hook(gpointer source, gpointer data) {
	FolderUpdateData* hookdata;
	VFolderItem* vitem;

	g_return_val_if_fail(source != NULL, FALSE);
	hookdata = (FolderUpdateData *) source;

	if (! hookdata->folder || IS_VFOLDER_FOLDER(hookdata->folder))
		return FALSE;

	if (hookdata->update_flags & FOLDER_REMOVE_FOLDERITEM) {
		/* TODO: check if the removed folder item is foundation for vfolder */
		debug_print("FOLDER_REMOVE_FOLDERITEM\n");
		vitem = vfolder_folder_item_watch(hookdata->item);
		if (vitem) {
			vfolder_source_folder_remove(vitem);
		}
	}

	if (hookdata->update_flags & FOLDER_MOVE_FOLDERITEM) {
		debug_print("FOLDER_MOVE_FOLDERITEM\n");
		/* item = from item2 = to */
		vitem = vfolder_folder_item_watch(hookdata->item);
		if (vitem)
			vfolder_source_path_change(vitem, hookdata->item2);
	}

	return FALSE;
}

static gboolean vfolder_folder_item_update_hook(gpointer source, gpointer data) {
	FolderItemUpdateData* hookdata;
	VFolderItem* vitem;

	g_return_val_if_fail(source != NULL, FALSE);
	hookdata = (FolderItemUpdateData *) source;

	if (! hookdata->item || IS_VFOLDER_FOLDER_ITEM(hookdata->item))
		return FALSE;

	if (hookdata->update_flags & F_ITEM_UPDATE_CONTENT) {
		debug_print("F_ITEM_UPDATE_CONTENT\n");
		vitem = vfolder_folder_item_watch(hookdata->item);
		if (vitem && ! vitem->updating && ! vitem->frozen)
			vfolder_scan_source_folder(vitem);
	}

	return FALSE;
}

static gboolean vfolder_msg_info_update_hook(gpointer source, gpointer data) {
	MsgInfoUpdate* hookdata;
	MsgInfo* msginfo;
	VFolderItem* vitem;

	cm_return_val_if_fail(source != NULL, FALSE);

	hookdata = (MsgInfoUpdate *) source;
	msginfo = hookdata->msginfo;

	cm_return_val_if_fail(msginfo != NULL, FALSE);

	if (IS_VFOLDER_MSGINFO(msginfo))
		return FALSE;

	if (MSG_IS_DELETED(msginfo->flags)) {
		vitem = vfolder_folder_item_watch(msginfo->folder);
		if (vitem) {
			debug_print("MSG_IS_DELETED\n");
			folder_item_remove_msg(FOLDER_ITEM(vitem), msginfo->msgnum);
		}
	}

	if (MSG_IS_MOVE(msginfo->flags)) {
		debug_print("MSG_IS_MOVE to VFolder\n");
		if (IS_VFOLDER_FOLDER_ITEM(msginfo->to_folder)) {
			vitem = VFOLDER_ITEM(msginfo->to_folder);
			gchar* msg = g_strconcat(vitem->source_id, ":\n",
							_("Cannot move to VFolder"), NULL);
			alertpanel_error(msg);
			g_free(msg);
			return FALSE;
		}
		vitem = vfolder_folder_item_watch(msginfo->folder);
		if (vitem && ! vitem->frozen) {
			/*
			 * if folder we move to is already monitored we need not
			 * do anything since distination folder will be automatically
			 * scanned when F_ITEM_UPDATE_MSGCNT is invoked
			 */
			 folder_item_remove_msg(FOLDER_ITEM(vitem), msginfo->msgnum);
		}
	}

	if (MSG_IS_COPY(msginfo->flags)) {
		debug_print("MSG_IS_COPY\n");
		if (IS_VFOLDER_FOLDER_ITEM(msginfo->to_folder)) {
			vitem = VFOLDER_ITEM(msginfo->to_folder);
			gchar* msg = g_strconcat(vitem->source_id, ":\n",
							_("Cannot copy to VFolder"), NULL);
			alertpanel_error(msg);
			g_free(msg);
			return FALSE;
		}
	}

	return FALSE;
}

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

FolderPropsResponse vfolder_folder_item_props_write(VFolderItem* vitem) {
	gchar* rc_file;
	GKeyFile* config;
	gchar* data = NULL;
	FILE* fp;
	gsize len = 0;
	FolderPropsResponse resp = FOLDER_ITEM_PROPS_NO_ITEM;

	g_return_val_if_fail(vitem != NULL, resp);

	if (! vitem->changed) return FOLDER_ITEM_PROPS_OK;

	debug_print("%s: Writing configuration\n", vitem->source_id);

	rc_file = vfolder_get_rc_file(vitem);
	config = g_key_file_new();

	if (vitem->filter)
		g_key_file_set_string(config, CONFIG_GROUP, "filter", vitem->filter);
	if (vitem->source_id)
		g_key_file_set_string(config, CONFIG_GROUP, "source_id", vitem->source_id);
	g_key_file_set_boolean(config, CONFIG_GROUP, "frozen", vitem->frozen);
	g_key_file_set_integer(config, CONFIG_GROUP, "searchtype", vitem->search);

	if (vitem->msgvault)
		vfolder_msgvault_serialize(vitem, config);

	if (g_file_test(rc_file, G_FILE_TEST_EXISTS)) {
		gchar* bakpath = g_strconcat(rc_file, ".bak", NULL);
		if (g_rename(rc_file, bakpath) < 0) {
			g_warning("%s: Could not create", bakpath);
			resp = FOLDER_ITEM_PROPS_BACKUP_FAIL;
		}
		g_free(bakpath);
	}

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
		vitem->changed = FALSE;
		resp = FOLDER_ITEM_PROPS_OK;
	}

error:
	g_free(data);

	g_key_file_free(config);
	g_free(rc_file);

	return resp;
}

FolderPropsResponse vfolder_folder_item_props_read(VFolderItem* vitem) {
	gchar* rc_file;
	GKeyFile* config;
	GError* error = NULL;
	FolderPropsResponse resp = FOLDER_ITEM_PROPS_NO_ITEM;

	g_return_val_if_fail(vitem != NULL, resp);

	if (! vitem->changed) return FOLDER_ITEM_PROPS_OK;

	rc_file = vfolder_get_rc_file(vitem);
	config = g_key_file_new();

	if (g_file_test(rc_file, G_FILE_TEST_EXISTS)) {
		g_key_file_load_from_file(config, rc_file, G_KEY_FILE_KEEP_COMMENTS, &error);
		if (error) {
			g_warning("%s. Using defaults", error->message);
			g_error_free(error);
			resp = FOLDER_ITEM_PROPS_READ_USING_DEFAULT;
		}
		else {
			vitem->filter = g_key_file_get_string(config, CONFIG_GROUP, "filter", NULL);
			vitem->search = g_key_file_get_integer(config, CONFIG_GROUP, "searchtype", NULL);
			vitem->frozen = g_key_file_get_boolean(config, CONFIG_GROUP, "frozen", NULL);
			vitem->source_id = g_key_file_get_string(config, CONFIG_GROUP, "source_id", NULL);
			if (vitem->source_id)
				vitem->source = folder_find_item_from_identifier(vitem->source_id);
			vfolder_msgvault_restore(vitem, config);

			FolderItem* item = FOLDER_ITEM(vitem);
			vfolder_set_last_num(item->folder, item);

			vfolder_msgvault_add(vitem);

			resp = FOLDER_ITEM_PROPS_OK;

			vitem->changed = FALSE;

			debug_print("%s: Read configuration\n", vitem->source_id);
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

	msginfo_hook_id = hooks_register_hook(MSGINFO_UPDATE_HOOKLIST,
		vfolder_msg_info_update_hook, NULL);
	if (msginfo_hook_id == -1) {
		*error = g_strdup(_("Failed to register message info update hook"));
		hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, folder_hook_id);
		hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, item_hook_id);
		return FALSE;
	}

	return TRUE;
}

void vfolder_gtk_done(void) {
	MainWindow *mainwin = mainwindow_get_mainwindow();

	hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, folder_hook_id);
	hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, item_hook_id);
	hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST, msginfo_hook_id);

	if (mainwin == NULL || claws_is_exiting())
		return;

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

	if (vfolder_edit_item_dialog(VFOLDER_ITEM(item), NULL)) {
		FolderPropsResponse resp = vfolder_folder_item_props_write(VFOLDER_ITEM(item));
		vfolder_item_props_response(resp);
	}
}

void vfolder_rename_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	gchar *new_name, *old_name;

    item = folderview_get_selected_item(folderview);
	cm_return_if_fail(item != NULL);
	cm_return_if_fail(item->path != NULL);
	cm_return_if_fail(item->folder != NULL);

	old_name = folder_item_get_name(item);
	new_name = input_dialog(_("Rename folder"),
					_("Input the new name for folder:"),
					old_name);
	g_free(old_name);
	if (!new_name) return;
	AUTORELEASE_STR(new_name, {g_free(new_name); return;});

	folder_item_rename(item, new_name);
}

void vfolder_new_folder_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	FolderItem *item, *new_item, *parent;
	gchar* new_folder;
	gchar* name;
	gchar* p;

    item = folderview_get_selected_item(folderview);
    if (item) {
		cm_return_if_fail(item->path != NULL);
		cm_return_if_fail(item->folder != NULL);

		if (item->no_sub) {
			alertpanel_error(N_("Virtual folders cannot contain subfolders"));
			return;
		}
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

	Folder* folder = folder_find_from_name(VFOLDER_DEFAULT_MAILBOX, vfolder_folder_get_class());
	parent = FOLDER_ITEM(folder->node->data);
	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(parent, new_folder)) {
		alertpanel_error(_("The folder '%s' already exists."), name);
		return;
	}

	new_item = folder_create_folder(parent, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder '%s'."), name);
		return;
	}

	if (! vfolder_create_item_dialog(VFOLDER_ITEM(new_item), item)) {
		new_item->folder->klass->remove_folder(new_item->folder, new_item);
		new_item = NULL;
		return;
	}

	if (vfolder_msgvault_add(VFOLDER_ITEM(new_item))) {
		new_item->folder->klass->remove_folder(new_item->folder, new_item);
		new_item = NULL;
		return;
	}

	folder_write_list();
}

void vfolder_refresh_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;

	g_return_if_fail(folderview != NULL);

	item = folderview_get_selected_item(folderview);

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	vfolder_scan_source_folder(VFOLDER_ITEM(item));
}

void vfolder_refresh_all_cb(GtkAction* action, gpointer data) {
	vfolder_scan_source_folder_all();
}

void vfolder_remove_folder_cb(GtkAction* action, gpointer data) {
	FolderView *folderview = (FolderView *)data;
	GtkCMCTree *ctree;
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	ctree = GTK_CMCTREE(folderview->ctree);

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All messages under '%s' will be permanently deleted. "
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel_full(_("Delete folder"), message,
				 GTK_STOCK_CANCEL, GTK_STOCK_DELETE, NULL, FALSE,
				 NULL, ALERT_WARNING, G_ALERTDEFAULT);
	g_free(message);
	if (avalue != G_ALERTALTERNATE) return;

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
		return;
	}

	folder_write_list();
}

GtkResponseType vfolder_msg_dialog(GtkMessageType msgtype, GtkButtonsType btntype,
								   const gchar* message) {
	GtkResponseType ret;
	GtkWidget* dialog;

	MainWindow* mainwin = mainwindow_get_mainwindow();
	dialog = gtk_message_dialog_new (GTK_WINDOW(mainwin->window),
									 GTK_DIALOG_DESTROY_WITH_PARENT,
									 msgtype, btntype, message);

	ret = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	return ret;
}
