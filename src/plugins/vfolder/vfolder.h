/*
 * $Id: $
 */

/* vim:et:ts=4:sw=4:et:sts=4:ai:set list listchars=tab\:»·,trail\:·: */

/*
 * Virtual folder plugin for claws-mail
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

typedef MsgInfoList* (*MSGFILTERFUNC) (MsgInfoList* msgs, VFolderItem* item);

struct _VFolderItem {
	FolderItem		item;

	gchar*			filter;				/* Regex used to select messages */
	gboolean		frozen;				/* Automatic update or not */
	gboolean		updating;			/* Is this VFolder currently updating */

	FolderItem*		source;				/* Source folder for virtual folder */
	GHashTable*		msg_store;		    /* Hashtable containing MsgInfo. Key is msginfo_identifier */
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

G_END_DECLS

#endif
