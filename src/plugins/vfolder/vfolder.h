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

#ifndef __VFOLDER_H__
#define __VFOLDER_H__

#include <glib.h>
#include <glib/gi18n.h>

G_BEGIN_DECLS

#include "folder.h"
#include "procmsg.h"

/* Name of directory in rcdir where VFolder will store its data. */
#define VFOLDER_DIR "vfolder"

/* Parent mailbox name */
#define VFOLDER_DEFAULT_MAILBOX	_("Virtual folders")

typedef struct _VFolderItem VFolderItem;
typedef struct _MsgVault MsgVault;

typedef enum {
	FOLDER_ITEM_PROPS_OK,
	FOLDER_ITEM_PROPS_NO_ITEM,
	FOLDER_ITEM_PROPS_BACKUP_FAIL,
	FOLDER_ITEM_PROPS_READ_DATA_FAIL,
	FOLDER_ITEM_PROPS_MAKE_RC_DIR_FAIL,
	FOLDER_ITEM_PROPS_READ_USING_DEFAULT
} FolderPropsResponse;

typedef enum {
	SEARCH_HEADERS	= 1,				/* from/to/subject */
	SEARCH_BODY		= 2,				/* message */
	SEARCH_BOTH		= 4					/* both */
} SearchType;

typedef MsgInfoList* (*MSGFILTERFUNC) (MsgInfoList* msgs, VFolderItem* item);

struct _VFolderItem {
	FolderItem		item;

	gchar*			filter;				/* Glob used to select messages */
	gboolean		frozen;				/* Automatic update or not */
	gboolean		updating;			/* Is this VFolder currently updating */
	gboolean		changed;

	SearchType		search;
	FolderItem*		source;				/* Source folder for virtual folder */
	gchar*			source_id;
	MsgVault*		msgvault;		    /* Message store */
	MSGFILTERFUNC	msg_filter_func;	/* Active filter function */
};

#define VFOLDER_ITEM(obj) ((VFolderItem *)obj)
#define VFOLDER(obj) ((VFolder *)obj)

#define IS_VFOLDER_FOLDER(folder) \
	((folder) && (folder->klass == vfolder_folder_get_class()))
#define IS_VFOLDER_FOLDER_ITEM(item) \
	((item) && (item->folder->klass == vfolder_folder_get_class()))
#define IS_VFOLDER_MSGINFO(msginfo) \
	((msginfo) && (msginfo->folder) && IS_VFOLDER_FOLDER_ITEM(msginfo->folder))

gboolean vfolder_init(void);
void vfolder_done(void);

FolderClass* vfolder_folder_get_class(void);
gint vfolder_get_virtual_msg_num(VFolderItem* vitem, gint srcnum);
gint vfolder_get_source_msg_num(VFolderItem* vitem, gint virtnum);
void vfolder_set_last_num(Folder *folder, FolderItem *item);
gboolean vfolder_msgvault_add(VFolderItem* vitem);
void vfolder_msgvault_free(MsgVault* data);
MsgVault* vfolder_msgvault_new();
gboolean vfolder_msgvault_serialize(VFolderItem* vitem, GKeyFile* config);
gboolean vfolder_msgvault_restore(VFolderItem* vitem, GKeyFile* config);
void vfolder_scan_source_folder(VFolderItem * vitem);
void vfolder_scan_source_folder_list(GSList* vitems);
void vfolder_scan_source_folder_all();
VFolderItem* vfolder_folder_item_watch(FolderItem* item);
void vfolder_source_path_change(VFolderItem* vitem, FolderItem* newItem);
void vfolder_source_folder_remove(VFolderItem* vitem);

G_END_DECLS

#endif
