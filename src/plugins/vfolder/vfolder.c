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
#include "mainwindow.h"
#include "folderview.h"

#include "vfolder.h"
#include "vfolder_gtk.h"
#include "vfolder_prop.h"

typedef struct {
	LocalFolder folder;
} VFolder;

typedef struct {
	gint			src;
	gint			dest;
	VFolderItem*	vitem;
} MsgPair;

struct _MsgVault {
	GHashTable*	src_to_virt;
	GHashTable*	virt_to_src;
};

FolderClass vfolder_class;
GHashTable* vfolders;
static gboolean existing_tree_found = FALSE;

static MsgInfo* vfolder_msgvault_get_msginfo(VFolderItem* vitem, gint num, gboolean vmsginfo) {
	if (! vitem || ! vitem->msgvault) return NULL;

	if (vmsginfo) {
		debug_print("src->dest: Look up [%d] from %s\n", num, vitem->source_id);
		return g_hash_table_lookup(vitem->msgvault->src_to_virt, GINT_TO_POINTER(num));
	} else {
		gchar* id = folder_item_get_identifier(FOLDER_ITEM(vitem));
		debug_print("dest->src: Look up [%d] from %s\n", num, id);
		g_free(id);
		return g_hash_table_lookup(vitem->msgvault->virt_to_src, GINT_TO_POINTER(num));
	}
}

static void vfolder_free_hashtable(gpointer data) {
	cm_return_if_fail(data!= NULL);

    VFolderItem* vfolder = (VFolderItem *) data;
    debug_print("%s: Removing virtual folder from folders\n", vfolder->source_id);
    vfolder_msgvault_free(vfolder->msgvault);
}

static GHashTable* vfolder_vfolders_new() {
	return g_hash_table_new_full(g_str_hash,
			g_str_equal, g_free, vfolder_free_hashtable);
}

static void vfolder_vfolders_config_save() {
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, vfolders);
	while (g_hash_table_iter_next(&iter, &key, &value)) {
		if (value) {
			VFolderItem* vitem = VFOLDER_ITEM(value);
			FolderPropsResponse resp = vfolder_folder_item_props_write(vitem);
			vfolder_item_props_response(resp);
		}
	}
}

static void vfolder_msgvault_add_msg(MsgPair* msgpair) {
	VFolderItem* vitem = msgpair->vitem;
	guint* snum = GUINT_TO_POINTER(msgpair->src);
	guint* dnum = GUINT_TO_POINTER(msgpair->dest);

	debug_print("Adding message: src->%d dest->%d\n", GPOINTER_TO_INT(snum), GPOINTER_TO_INT(dnum));
	if (! vitem->msgvault)
		vitem->msgvault = vfolder_msgvault_new();
	g_hash_table_replace(vitem->msgvault->src_to_virt, snum, dnum);
	g_hash_table_replace(vitem->msgvault->virt_to_src, dnum, snum);
}

static gboolean vfolder_msgvault_remove_msg(VFolderItem* vitem, gint msgnum) {
	gboolean ret;
	MsgVault* msgvault = vitem->msgvault;
	guint* num = GUINT_TO_POINTER(msgnum);
	gpointer key;

	if (! msgvault) return FALSE;

	if ((key = g_hash_table_lookup(msgvault->src_to_virt, num)) != NULL) {
		debug_print("Removing message: src->%d dest->%d\n", msgnum, GPOINTER_TO_INT(key));
		g_hash_table_remove(msgvault->virt_to_src, key);
		g_hash_table_remove(msgvault->src_to_virt, num);
		ret = FALSE;
	} else if ((key = g_hash_table_lookup(msgvault->virt_to_src, num)) != NULL) {
		debug_print("Removing message: src->%d dest->%d\n", GPOINTER_TO_INT(key), msgnum);
		g_hash_table_remove(msgvault->virt_to_src, num);
		g_hash_table_remove(msgvault->src_to_virt, key);
		ret = FALSE;
	} else
		ret = TRUE;

	return ret;
}

static void vfolder_init_read_func(FolderItem* item, gpointer data) {
	cm_return_if_fail(item != NULL);
	FolderPropsResponse resp;

	if (! IS_VFOLDER_FOLDER_ITEM(item))
		return;

    existing_tree_found = TRUE;

	if (folder_item_parent(item) == NULL)
		return;

	resp = vfolder_folder_item_props_read(VFOLDER_ITEM(item));
	vfolder_item_props_response(resp);

	vfolder_set_msgs_filter(VFOLDER_ITEM(item));

/*
	if (! VFOLDER_ITEM(item)->frozen) {
		folder_item_scan(VFOLDER_ITEM(item)->source);
	}
*/
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

	cm_return_if_fail(root != NULL);

	folder_add(root);
}

/* Folder functions */
static Folder* vfolder_folder_new(const gchar* name, const gchar* path) {
	VFolder* folder;

	debug_print("VFolder: new_folder\n");

	vfolder_make_rc_dir();

	folder = g_new0(VFolder, 1);
	FOLDER(folder)->klass = &vfolder_class;
	folder_init(FOLDER(folder), name);
	FOLDER(folder)->klass->create_tree(FOLDER(folder));

	return FOLDER(folder);
}

static void vfolder_folder_destroy(Folder* _folder)
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
	cm_return_val_if_fail(folder != NULL, -1);

	folder->inbox = NULL;
	folder->outbox = NULL;
	folder->draft = NULL;
	folder->queue = NULL;
	folder->trash = NULL;

	debug_print("VFolder: scanning tree\n");
	vfolder_create_tree(folder);

	return 0;
}

/* FolderItem functions */
static gboolean vfolder_scan_required(Folder* folder, FolderItem* item) {
	cm_return_val_if_fail(item != NULL, FALSE);
	FolderItem* src = VFOLDER_ITEM(item)->source;

	debug_print("%s: Check if scan is required\n", item->path);
	cm_return_val_if_fail(src != NULL, FALSE);

	debug_print("mtime src [%ld] mtime VFolder [%ld]\n", src->mtime, item->mtime);
	if (src->mtime > item->mtime)
		return TRUE;
	else
		return FALSE;
}

static FolderItem* vfolder_item_new(Folder* folder) {
	VFolderItem* vitem;

	debug_print("VFolder: item_new\n");

	vitem = g_new0(VFolderItem, 1);

	vitem->filter = NULL;
	vitem->frozen = FALSE;
	vitem->updating = FALSE;
	vitem->changed = TRUE;
	vitem->source_id = NULL;
	vitem->search = SEARCH_HEADERS;
	vitem->source = NULL;
	vitem->msgvault = vfolder_msgvault_new();

	return FOLDER_ITEM(vitem);
}

static void vfolder_item_destroy(Folder* folder, FolderItem* item) {
	VFolderItem* vitem = VFOLDER_ITEM(item);

	cm_return_if_fail(vitem != NULL);

	if (vitem->source_id) {
		debug_print("%s: Removing virtual folder from message store\n", vitem->source_id);
		g_hash_table_remove(vfolders, vitem->source_id);
		g_free(vitem->source_id);
		vitem->source_id = NULL;
	}

	g_free(vitem->filter);
	vitem->filter = NULL;
	vitem->msgvault = NULL;
	vitem->source = NULL;

	g_free(item);
	item = NULL;
}

static gchar* vfolder_item_get_path(Folder* folder, FolderItem* item) {
	gchar* result;

	cm_return_val_if_fail(item != NULL, NULL);

	result = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, VFOLDER_DIR,
			G_DIR_SEPARATOR_S, item->path, NULL);
	return result;
}

static gint vfolder_folder_remove(Folder* folder, FolderItem* item) {
	GDir* dp;
	const gchar* name;
	gchar* cwd;
	gint ret = -1;

	cm_return_val_if_fail(folder != NULL, -1);
	cm_return_val_if_fail(item != NULL, -1);
	cm_return_val_if_fail(item->path != NULL, -1);
	cm_return_val_if_fail(item->stype == F_NORMAL, -1);

	debug_print("VFolder: removing folder item %s\n", item->path);

	gchar* path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
		VFOLDER_DIR, G_DIR_SEPARATOR_S, item->path, NULL);
	remove_numbered_files(path, item->last_num - item->total_msgs + 1, item->last_num);

	folder_item_remove(item);

	cwd = g_get_current_dir();

	if (change_dir(path) < 0 ) {
		g_free(path);
		return ret;
	}

	if( (dp = g_dir_open(".", 0, NULL)) == NULL ) {
		FILE_OP_ERROR(path, "g_dir_open");
		change_dir(cwd);
		g_free(cwd);
		g_free(path);
		return ret;
	}

	while ((name = g_dir_read_name(dp)) != NULL) {
		debug_print("Removing: %s\n", name);
		claws_unlink(name);
	}

	g_dir_close(dp);
	change_dir(cwd);
	if (g_rmdir(path) == 0)
		ret = 0;

	g_free(cwd);
	g_free(path);

	return ret;
}

static FolderItem* vfolder_folder_create(Folder* folder,
									   	 FolderItem* parent,
									   	 const gchar* name) {
	gchar* path = NULL;
	FolderItem* newitem = NULL;

	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(parent != NULL, NULL);
	cm_return_val_if_fail(name != NULL, NULL);

	if (parent->no_sub)
		return NULL;

	path = g_strconcat((parent->path != NULL) ? parent->path : "", name, NULL);
	newitem = folder_item_new(folder, name, path);
	newitem->no_sub = TRUE;

	folder_item_append(parent, newitem);
	g_free(path);

	return newitem;
}

static gint vfolder_get_num_list(Folder* folder, FolderItem* item,
		MsgNumberList** list, gboolean* old_uids_valid) {
	gchar* path;
	DIR* dp;
	struct dirent* d;
	gint num, nummsgs = 0;

	cm_return_val_if_fail(item != NULL, -1);

	debug_print("VFolder: scanning '%s'...\n", item->path);

	if (vfolder_scan_required(folder, item))
		vfolder_scan_source_folder(VFOLDER_ITEM(item));
	else
		item->mtime = time(NULL);

	*old_uids_valid = TRUE;

	path = folder_item_get_path(item);
	cm_return_val_if_fail(path != NULL, -1);
	if (change_dir(path) < 0 ) {
		g_free(path);
		return -1;
	}
	g_free(path);

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		return -1;
	}

	while ((d = readdir(dp)) != NULL ) {
		if( (num = to_number(d->d_name)) > 0 ) {
			debug_print("Adding %d to list\n", num);
			*list = g_slist_prepend(*list, GINT_TO_POINTER(num));
			nummsgs++;
		}
	}
	closedir(dp);

	return nummsgs;
}

/* Message functions */
static void vfolder_get_last_num(Folder *folder, FolderItem *item) {
	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint max = 0;
	gint num;

	cm_return_if_fail(item != NULL);

	debug_print("vfolder_get_last_num(): Scanning %s ...\n", item->path);
	path = folder_item_get_path(item);
	cm_return_if_fail(path != NULL);

	if (! is_dir_exist(path)) return;

	if (change_dir(path) < 0) {
		g_free(path);
		return;
	}
	g_free(path);

	if ((dp = opendir(".")) == NULL) {
		FILE_OP_ERROR(item->path, "opendir");
		return;
	}

	while ((d = readdir(dp)) != NULL) {
		if ((num = to_number(d->d_name)) > 0 && dirent_is_regular_file(d)) {
			if( max < num )
				max = num;
		}
	}
	closedir(dp);

	debug_print("Last number in dir %s = %d\n", item->path, max);
	item->last_num = max;
}

static gchar* vfolder_get_new_msg_filename(FolderItem* dest) {
	gchar* destfile;
	gchar* destpath;

	destpath = folder_item_get_path(dest);
	cm_return_val_if_fail(destpath != NULL, NULL);

	if (!is_dir_exist(destpath))
		make_dir_hier(destpath);

	for( ; ; ) {
		destfile = g_strdup_printf("%s%c%d", destpath, G_DIR_SEPARATOR,
				dest->last_num + 1);
		if (is_file_entry_exist(destfile)) {
			dest->last_num++;
			g_free(destfile);
		} else
			break;
	}

	g_free(destpath);

	return destfile;
}

static gint vfolder_add_msgs(Folder* folder, FolderItem* dest, GSList* file_list,
		GHashTable* relation) {
	gchar* destfile;
	GSList* cur;
	MsgFileInfo* fileinfo;
	VFolderItem* vitem;

	cm_return_val_if_fail(dest != NULL, -1);
	cm_return_val_if_fail(file_list != NULL, -1);

	vitem = VFOLDER_ITEM(dest);
	if (dest->last_num < 0) {
		vfolder_get_last_num(folder, dest);
		if (dest->last_num < 0)
			dest->last_num = 0;
	}

	for( cur = file_list; cur != NULL; cur = cur->next ) {
		fileinfo = (MsgFileInfo *)cur->data;

		destfile = vfolder_get_new_msg_filename(dest);
		if (! destfile)
			goto bail;

		if (copy_file(fileinfo->file, destfile, TRUE) < 0) {
			g_warning("can't copy message %s to %s\n", fileinfo->file, destfile);
			g_free(destfile);
			destfile = NULL;
			goto bail;
		}

		if( relation != NULL )
			g_hash_table_insert(relation, fileinfo,
					GINT_TO_POINTER(dest->last_num + 1) );
		g_free(destfile);

		MsgPair* pair = g_new0(MsgPair, 1);
		pair->src = fileinfo->msginfo->msgnum;
		pair->dest = dest->last_num + 1;
		pair->vitem = vitem;
		vfolder_msgvault_add_msg(pair);
		g_free(pair);
		dest->last_num++;
		vitem->changed = TRUE;
	}

	FolderPropsResponse resp = vfolder_folder_item_props_write(VFOLDER_ITEM(dest));
	vfolder_item_props_response(resp);

bail:

	return (destfile) ? dest->last_num : -1;
}

static gint vfolder_add_msg(Folder* folder, FolderItem* dest, const gchar* file,
		MsgFlags* flags) {
	gint ret;
	GSList file_list;
	MsgFileInfo fileinfo;

	cm_return_val_if_fail(file != NULL, -1);

	fileinfo.msginfo = NULL;
	fileinfo.file = (gchar *)file;
	fileinfo.flags = flags;
	file_list.data = &fileinfo;
	file_list.next = NULL;

	ret = vfolder_add_msgs(folder, dest, &file_list, NULL);
	return ret;
}

static gchar* vfolder_fetch_msg(Folder* folder, FolderItem* item, gint num) {
	if (num < 0) return NULL;

	gchar* snum = g_strdup_printf("%d", num);
	gchar* file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, VFOLDER_DIR,
			G_DIR_SEPARATOR_S, item->path, G_DIR_SEPARATOR_S, snum, NULL);

	debug_print("VFolder: fetch_msg: '%s'\n", file);

	g_free(snum);

	return file;
}

static MsgInfo* vfolder_get_msginfo(Folder* folder, FolderItem* item, gint num) {
	MsgInfo* msginfo = NULL;
	gchar* file;
	MsgFlags flags;

	debug_print("VFolder: get_msginfo: %d\n", num);

	cm_return_val_if_fail(folder != NULL, NULL);
	cm_return_val_if_fail(item != NULL, NULL);
	cm_return_val_if_fail(num > 0, NULL);

	file = vfolder_fetch_msg(folder, item, num);
	if (! g_file_test(file, G_FILE_TEST_EXISTS)) {
		g_free(file);
		return NULL;
	}

	flags.perm_flags = MSG_NEW | MSG_UNREAD;
	flags.tmp_flags = 0;

	msginfo = procheader_parse_file(file, flags, TRUE, TRUE);

	if (msginfo)
		msginfo->msgnum = num;

	if (!msginfo->folder)
		msginfo->folder = item;

	g_free(file);

	return msginfo;
}

static gint vfolder_remove_msg(Folder* folder, FolderItem* item, gint num) {
	gchar* file;
	gint vnum;

	cm_return_val_if_fail(item != NULL, -1);

	file = vfolder_fetch_msg(folder, item, num);
	if (! is_file_exist(file)) {
		/* num relates to source */
		g_free(file);
		vnum = vfolder_get_virtual_msg_num(VFOLDER_ITEM(item), num);
		file = vfolder_fetch_msg(folder, item, vnum);
	}
	if (! file) return -1;

	if (claws_unlink(file) < 0) {
		FILE_OP_ERROR(file, "unlink");
		g_free(file);
		return -1;
	}

	vfolder_msgvault_remove_msg(VFOLDER_ITEM(item), num);
	VFOLDER_ITEM(item)->changed = TRUE;

	FolderPropsResponse resp = vfolder_folder_item_props_write(VFOLDER_ITEM(item));
	vfolder_item_props_response(resp);

	folder_item_scan(item);

	g_free(file);

	return 0;
}

static gint vfolder_remove_all_msg(Folder* folder, FolderItem* item) {
	GSList *msg_list, *list;

	cm_return_val_if_fail(item != NULL, -1);

	msg_list = folder_item_get_msg_list(item);
	if (msg_list) {
		for (list = msg_list; list; list = g_slist_next(list)) {
			MsgInfo* msginfo = (MsgInfo *) list->data;
			folder_item_remove_msg(item, msginfo->msgnum);
		}
	}
	g_slist_free(msg_list);

	return 0;
}

static gint vfolder_copy_msg (Folder* folder, FolderItem* dest, MsgInfo* msginfo) {
	return -1;
}

static gboolean vfolder_subscribe_uri(Folder* folder, const gchar* uri) {
	return FALSE;
}

static gint vfolder_folder_rename(Folder *folder, FolderItem *item, const gchar	*name) {
	cm_return_val_if_fail(item != NULL, -1);

	if (item->name)
		g_free(item->name);
	item->name = g_strdup(name);

	return 0;
}

FolderClass* vfolder_folder_get_class() {
	if (vfolder_class.idstr == NULL ) {
		vfolder_class.type = F_UNKNOWN;
		vfolder_class.idstr = "vfolder";
		vfolder_class.uistr = "VFolder";

		/* Folder functions */
		vfolder_class.new_folder = vfolder_folder_new;
		vfolder_class.destroy_folder = vfolder_folder_destroy;
		vfolder_class.set_xml = folder_set_xml;
		vfolder_class.get_xml = folder_get_xml;
		vfolder_class.scan_tree = vfolder_scan_tree;
		vfolder_class.create_tree = vfolder_create_tree;

		/* FolderItem functions */
		vfolder_class.item_new = vfolder_item_new;
		vfolder_class.item_destroy = vfolder_item_destroy;
		vfolder_class.item_get_path = vfolder_item_get_path;
		vfolder_class.create_folder = vfolder_folder_create;
		vfolder_class.rename_folder = vfolder_folder_rename;
		vfolder_class.remove_folder = vfolder_folder_remove;
		vfolder_class.get_num_list = vfolder_get_num_list;
		vfolder_class.scan_required = vfolder_scan_required;

		/* Message functions */
/*
		vfolder_class.copy_msgs = vfolder_copy_msgs;
*/
		vfolder_class.get_msginfo = vfolder_get_msginfo;
		vfolder_class.fetch_msg = vfolder_fetch_msg;
		vfolder_class.copy_msgs = NULL;
		vfolder_class.copy_msg = vfolder_copy_msg;
		vfolder_class.add_msg = vfolder_add_msg;
		vfolder_class.add_msgs = vfolder_add_msgs;
		vfolder_class.remove_msg = vfolder_remove_msg;
		vfolder_class.remove_msgs = NULL;
		vfolder_class.remove_all_msg = vfolder_remove_all_msg;
		vfolder_class.change_flags = NULL;
		vfolder_class.subscribe = vfolder_subscribe_uri;

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

    vfolders = vfolder_vfolders_new();

	folder_func_to_all_folders((FolderItemFunc)vfolder_init_read_func, NULL);

    if (existing_tree_found == FALSE)
        vfolder_create_default_mailbox();

	return TRUE;
}

void vfolder_done(void) {

	vfolder_gtk_done();

	vfolder_vfolders_config_save();

	g_hash_table_destroy(vfolders);

	if (!claws_is_exiting())
		folder_unregister_class(vfolder_folder_get_class());
}

void vfolder_set_last_num(Folder *folder, FolderItem *item) {
	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint max = 0;
	gint num;

	cm_return_if_fail(item != NULL);

	debug_print("vfolder_get_last_num(): Scanning %s ...\n", item->path);
	path = folder_item_get_path(item);
	cm_return_if_fail(path != NULL);
	if( change_dir(path) < 0 ) {
		g_free(path);
		return;
	}
	g_free(path);

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		return;
	}

	while( (d = readdir(dp)) != NULL ) {
		if( (num = to_number(d->d_name)) > 0 && dirent_is_regular_file(d) ) {
			if( max < num )
				max = num;
		}
	}
	closedir(dp);

	debug_print("Last number in dir %s = %d\n", item->path, max);
	item->last_num = max;
}

gint vfolder_get_virtual_msg_num(VFolderItem* vitem, gint srcnum) {
	gpointer val;

	cm_return_val_if_fail(vitem != NULL, -1);

	val = g_hash_table_lookup(vitem->msgvault->src_to_virt, GINT_TO_POINTER(srcnum));

	if (val)
		return GPOINTER_TO_INT(val);
	else
		return -1;
}

gint vfolder_get_source_msg_num(VFolderItem* vitem, gint virtnum) {
	gpointer val;

	cm_return_val_if_fail(vitem != NULL, -1);

	val = g_hash_table_lookup(vitem->msgvault->virt_to_src, GINT_TO_POINTER(virtnum));

	if (val)
		return GPOINTER_TO_INT(val);
	else
		return -1;
}

gboolean vfolder_msgvault_add(VFolderItem* vitem) {
	cm_return_val_if_fail(vitem != NULL, TRUE);
	cm_return_val_if_fail(vitem->source_id != NULL, TRUE);

	g_hash_table_replace(vfolders, g_strdup(vitem->source_id), vitem);

	return FALSE;
}

#define MSGVAULT_GROUP "message vault"
gboolean vfolder_msgvault_serialize(VFolderItem* vitem, GKeyFile* config) {
	MsgVault* msgvault;
	GHashTableIter iter;
	gpointer key, value;

	cm_return_val_if_fail(vitem != NULL, TRUE);
	cm_return_val_if_fail(vitem->msgvault != NULL, TRUE);
	cm_return_val_if_fail(config != NULL, TRUE);

	msgvault = vitem->msgvault;
	g_hash_table_iter_init (&iter, msgvault->virt_to_src);
	// Save each key / value pair to file
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gchar* k = itos(GPOINTER_TO_INT(key));
		g_key_file_set_integer(config, MSGVAULT_GROUP, g_strdup(k), GPOINTER_TO_INT(value));
		debug_print("Vault to config: src->%s dest->%d\n", k, GPOINTER_TO_INT(value));
	}

	return FALSE;
}

gboolean vfolder_msgvault_restore(VFolderItem* vitem, GKeyFile* config) {
	MsgVault* msgvault;
	gchar **keys;
	gchar **keys_it;

	cm_return_val_if_fail(vitem != NULL, TRUE);
	cm_return_val_if_fail(config != NULL, TRUE);

	if (!vitem->msgvault)
		vitem->msgvault = vfolder_msgvault_new();
	msgvault = vitem->msgvault;

	keys = g_key_file_get_keys (config, MSGVAULT_GROUP, NULL, NULL);
	keys_it = keys;
	// Add each key / value pair from file
	while (keys_it != NULL && (*keys_it) != NULL) {
		gint value = g_key_file_get_integer(config, MSGVAULT_GROUP, (*keys_it), NULL);
		gint key = to_number(*keys_it);
		debug_print("Insert into vault: src->%d dest->%d\n", value, key);
		g_hash_table_insert(msgvault->virt_to_src, GINT_TO_POINTER(key), GINT_TO_POINTER(value));
		g_hash_table_insert(msgvault->src_to_virt, GINT_TO_POINTER(value), GINT_TO_POINTER(key));
		++keys_it;
	}
	if (keys != NULL)
		g_strfreev (keys);

	return FALSE;
}

MsgVault* vfolder_msgvault_new() {
	MsgVault* msgvault = g_new0(MsgVault, 1);
	msgvault->src_to_virt = g_hash_table_new(g_direct_hash, g_direct_equal);
	msgvault->virt_to_src = g_hash_table_new(g_direct_hash, g_direct_equal);

	return msgvault;
}

void vfolder_msgvault_free(MsgVault* data) {
	cm_return_if_fail(data != NULL);

	debug_print("Removing message vault\n");

    g_hash_table_destroy(data->src_to_virt);
    g_hash_table_destroy(data->virt_to_src);
    g_free(data);
}

void vfolder_scan_source_folder(VFolderItem * vitem) {
	MsgInfoList *mlist, *cur, *unfiltered = NULL, *filtered;
	GSList* filelist = NULL, *tmp;
	GHashTableIter iter;
	gpointer key, value;
	MsgInfo* msginfo;
	FolderItem* item;

	cm_return_if_fail(vitem != NULL);

	if (! vitem->source) return;

	debug_print("%s: Scanning source [%s]\n", FOLDER_ITEM(vitem)->path, vitem->source_id);

	item = folder_find_item_from_identifier(vitem->source_id);
	if (! item) return;

	vitem->updating = TRUE;
	folder_item_scan(item);

	folder_item_update_freeze();
	mlist = folder_item_get_msg_list(item);

	/* find new messages */
	for (cur = mlist; cur; cur = g_slist_next(cur)) {
		msginfo = (MsgInfo *) cur->data;
		MsgInfo* vmsginfo = vfolder_msgvault_get_msginfo(vitem, msginfo->msgnum, TRUE);
		if (! vmsginfo)
			unfiltered = g_slist_append(unfiltered, msginfo);
		else
			procmsg_msginfo_free(msginfo);
	}
	filtered = vitem->msg_filter_func(unfiltered, vitem);
	if (filtered) {
		filelist = procmsg_get_message_file_list(filtered);
		if (filelist) {
			gint n = folder_item_add_msgs(FOLDER_ITEM(vitem), filelist, FALSE);
			FOLDER_ITEM(vitem)->last_num = n;
			procmsg_message_file_list_free(filelist);
		}
		g_slist_free(filtered);
	}
	procmsg_msg_list_free(unfiltered);

	/* find removed messages */
	filelist = NULL;
	g_hash_table_iter_init (&iter, vitem->msgvault->virt_to_src);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gint num = GPOINTER_TO_INT(value);
		msginfo = folder_item_get_msginfo(vitem->source, num);
		if (! msginfo)
			filelist = g_slist_prepend(filelist, value);
		else
			procmsg_msginfo_free(msginfo);
	}

	for (tmp = filelist; tmp; tmp = g_slist_next(tmp)) {
		gint num = GPOINTER_TO_INT(tmp->data);
		folder_item_remove_msg(FOLDER_ITEM(vitem), num);
	}

	if (filelist)
		g_slist_free(filelist);

	if (mlist)
			g_slist_free(mlist);

	folder_item_update_thaw();

	FOLDER_ITEM(vitem)->mtime = time(NULL);
	vitem->updating = FALSE;
}

void vfolder_scan_source_folder_list(GSList* vitems) {
	GSList* cur;
	VFolderItem* vitem;

	if (! vitems) return;

	for (cur = vitems; cur; cur = g_slist_next(cur)) {
		vitem = (VFolderItem *) cur->data;
		vfolder_scan_source_folder(vitem);
	}
}

void vfolder_scan_source_folder_all() {
	GHashTableIter iter;
	gpointer key, value;
	VFolderItem* vitem;
	GSList* vitems = NULL;

	g_hash_table_iter_init (&iter, vfolders);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		vitem = VFOLDER_ITEM(value);
		vitems = g_slist_prepend(vitems, vitem);
	}

	vfolder_scan_source_folder_list(vitems);
	g_slist_free(vitems);
}

VFolderItem* vfolder_folder_item_watch(FolderItem* item) {
	GHashTableIter iter;
	gpointer key, value;
	VFolderItem* match = NULL;

	cm_return_val_if_fail(item != NULL, FALSE);

	gchar* id = folder_item_get_identifier(item);
	if (!id) return FALSE;

	g_hash_table_iter_init (&iter, vfolders);
	while (g_hash_table_iter_next (&iter, &key, &value) && ! match) {
		VFolderItem* vitem = VFOLDER_ITEM(value);
		if (g_utf8_collate(id, vitem->source_id) == 0)
			match = vitem;
	}

	g_free(id);

	return match;
}

void vfolder_source_path_change(VFolderItem* vitem, FolderItem* newItem) {
	VFolderItem* newVitem = NULL;
	gpointer key, value;

	cm_return_if_fail(vitem != NULL);

	if (g_hash_table_lookup_extended(vfolders, vitem->source_id, &key, &value)) {
		g_hash_table_steal(vfolders, vitem->source_id);
		g_free(key);
		newVitem = VFOLDER_ITEM(value);
		newVitem->source = newItem;
		g_free(newVitem->source_id);
		newVitem->source_id = folder_item_get_identifier(newItem);
		g_hash_table_replace(vfolders, g_strdup(newVitem->source_id), newVitem);
	}
}

void vfolder_source_folder_remove(VFolderItem* vitem) {
	cm_return_if_fail(vitem != NULL);

	MainWindow* mainwindow = mainwindow_get_mainwindow();
	FolderView* folderview = mainwindow->folderview;
	gchar *id1, *id2;

	if (! vitem->frozen) {
		FolderItem* item = folderview_get_selected_item(folderview);
		if (item) {
			id1 = folder_item_get_identifier(item);
			id2 = folder_item_get_identifier(FOLDER_ITEM(vitem));
			if (strcmp(id1, id2) == 0)
				folderview_close_opened(folderview);
			g_free(id1);
			g_free(id2);
		}
		vfolder_folder_remove(FOLDER_ITEM(vitem)->folder, FOLDER_ITEM(vitem));
	}
}
