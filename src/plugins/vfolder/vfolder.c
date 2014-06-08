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

#include <glib.h>
#include <glib/gi18n.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "defs.h"

#include "procmsg.h"
#include "procheader.h"
#include "folder.h"
#include "alertpanel.h"
#include "main.h"
#include "localfolder.h"
#include "statusbar.h"

#include "vfolder.h"
#include "vfolder_gtk.h"
#include "vfolder_prop.h"

typedef struct {
	LocalFolder folder;
} VFolder;

FolderClass vfolder_class;
GHashTable* vfolders;
static gboolean existing_tree_found = FALSE;

static void free_vfolder_hashtable(gpointer data) {
	g_return_if_fail(data!= NULL);

    GHashTable* vfolder = (GHashTable *) data;
    g_hash_table_destroy(vfolder);
}

static GHashTable* vfolders_hashtable_new() {
	return g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, free_vfolder_hashtable);
}

static GHashTable* vfolder_hashtable_new() {
	return g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, (GDestroyNotify) procmsg_msginfo_free);
}

static void vfolder_init_read_func(FolderItem* item, gpointer data) {
	g_return_if_fail(item != NULL);

	if (! IS_VFOLDER_FOLDER_ITEM(item))
		return;

    existing_tree_found = TRUE;

	if (folder_item_parent(item) == NULL)
		return;

	vfolder_folder_item_props_read(VFOLDER_ITEM(item));
	vfolder_set_msgs_filter(VFOLDER_ITEM(item));

/*	if (! IS_VFOLDER_FROZEN(VFOLDER_ITEM(item))) {
		folder_item_scan(VFOLDER_ITEM(item)->source);
	}*/
}

static void vfolder_make_rc_dir(void) {
	gchar* vfolder_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, VFOLDER_DIR, NULL);

	if (! is_dir_exist(vfolder_dir)) {
		if (make_dir(vfolder_dir) < 0) {
			g_warning("couldn't create directory %s\n", vfolder_dir);
		}

		debug_print("created directorty %s\n", vfolder_dir);
	}

	g_free(vfolder_dir);
}

static void vfolder_create_default_mailbox(void) {
	Folder* root = NULL;

	vfolder_make_rc_dir();

	root = folder_new(vfolder_folder_get_class(), VFOLDER_DEFAULT_MAILBOX, NULL);

	g_return_if_fail(root != NULL);

	folder_add(root);
}

static Folder* vfolder_new_folder(const gchar* name, const gchar* path) {
	VFolder* folder;

	debug_print("VFolder: new_folder\n");

	vfolder_make_rc_dir();

	folder = g_new0(VFolder, 1);
	FOLDER(folder)->klass = &vfolder_class;
	folder_init(FOLDER(folder), name);

	return FOLDER(folder);
}

static void vfolder_destroy_folder(Folder* _folder)
{
	VFolder* folder = VFOLDER(_folder);

	folder_local_folder_destroy(LOCAL_FOLDER(folder));
}

static gint vfolder_create_tree(Folder* folder) {
	FolderItem* rootitem;
	GNode* rootnode;

	vfolder_make_rc_dir();

	if (!folder->node) {
		rootitem = folder_item_new(folder, folder->name, NULL);
		rootitem->folder = folder;
		rootnode = g_node_new(rootitem);
		folder->node = rootnode;
		rootitem->node = rootnode;
	} else {
		rootitem = FOLDER_ITEM(folder->node->data);
		rootnode = folder->node;
	}

	debug_print("VFolder: created new vfolder tree\n");
	return 0;
}

static gint vfolder_scan_tree(Folder *folder) {
	g_return_val_if_fail(folder != NULL, -1);

	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;

	debug_print("VFolder: scanning tree\n");
	vfolder_create_tree(folder);

	return 0;
}

FolderClass* vfolder_folder_get_class() {
	if (vfolder_class.idstr == NULL ) {
		vfolder_class.type = F_UNKNOWN;
		vfolder_class.idstr = "vfolder";
		vfolder_class.uistr = "VFolder";

		/* Folder functions */
		vfolder_class.new_folder = vfolder_new_folder;
		vfolder_class.destroy_folder = vfolder_destroy_folder;
		vfolder_class.set_xml = folder_set_xml;
		vfolder_class.get_xml = folder_get_xml;
		vfolder_class.scan_tree = vfolder_scan_tree;
		vfolder_class.create_tree = vfolder_create_tree;

		/* FolderItem functions */
/*		vfolder_class.item_new = vfolder_item_new;
		vfolder_class.item_destroy = vfolder_item_destroy;
		vfolder_class.item_get_path = vfolder_item_get_path;
		vfolder_class.create_folder = vfolder_create_folder;
		vfolder_class.rename_folder = NULL;
		vfolder_class.remove_folder = vfolder_remove_folder;
		vfolder_class.get_num_list = vfolder_get_num_list;
		vfolder_class.scan_required = vfolder_scan_required;*/
		vfolder_class.item_new = NULL;
		vfolder_class.item_destroy = NULL;
		vfolder_class.item_get_path = NULL;
		vfolder_class.create_folder = NULL;
		vfolder_class.rename_folder = NULL;
		vfolder_class.remove_folder = NULL;
		vfolder_class.get_num_list = NULL;
		vfolder_class.scan_required = NULL;

		/* Message functions */
/*		vfolder_class.get_msginfo = vfolder_get_msginfo;
		vfolder_class.fetch_msg = vfolder_fetch_msg;
		vfolder_class.copy_msgs = vfolder_copy_msgs;
		vfolder_class.copy_msg = vfolder_copy_msg;
		vfolder_class.add_msg = vfolder_add_msg;
		vfolder_class.add_msgs = vfolder_add_msgs;
		vfolder_class.remove_msg = vfolder_remove_msg;
		vfolder_class.remove_msgs = NULL;
		vfolder_class.remove_all_msg = vfolder_remove_all_msg;
		vfolder_class.change_flags = NULL;
		vfolder_class.subscribe = vfolder_subscribe_uri;*/
		vfolder_class.get_msginfo = NULL;
		vfolder_class.fetch_msg = NULL;
		vfolder_class.copy_msgs = NULL;
		vfolder_class.copy_msg = NULL;
		vfolder_class.add_msg = NULL;
		vfolder_class.add_msgs = NULL;
		vfolder_class.remove_msg = NULL;
		vfolder_class.remove_msgs = NULL;
		vfolder_class.remove_all_msg = NULL;
		vfolder_class.change_flags = NULL;
		vfolder_class.subscribe = NULL;

		debug_print("VFolder: registered folderclass\n");
	}

	return &vfolder_class;
}

/* Local functions */

gboolean vfolder_init(void) {
	gchar* error = g_new0(gchar, 1);

	folder_register_class(vfolder_folder_get_class());

	if (! vfolder_gtk_init(&error)) {
		alertpanel_error("%s", error);
		vfolder_done();
		return FALSE;
	}

	folder_func_to_all_folders((FolderItemFunc)vfolder_init_read_func, NULL);

    vfolders = vfolders_hashtable_new();

    if (existing_tree_found == FALSE)
        vfolder_create_default_mailbox();

	return TRUE;
}

void vfolder_done(void) {

	vfolder_gtk_done();

    g_hash_table_destroy(vfolders);

	if (!claws_is_exiting())
		folder_unregister_class(vfolder_folder_get_class());
}
