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

#ifndef __VFOLDER_GTK_H__
#define __VFOLDER_GTK_H__

#include <glib.h>

G_BEGIN_DECLS

#include "vfolder.h"
#include <gtk/gtk.h>

typedef enum {
	FOLDER_ITEM_PROPS_OK,
	FOLDER_ITEM_PROPS_NO_ITEM,
	FOLDER_ITEM_PROPS_BACKUP_FAIL,
	FOLDER_ITEM_PROPS_READ_DATA_FAIL,
	FOLDER_ITEM_PROPS_MAKE_RC_DIR_FAIL,
	FOLDER_ITEM_PROPS_READ_USING_DEFAULT
} FolderPropsResponse;

gboolean vfolder_gtk_init(gchar** error);
void vfolder_gtk_done(void);
FolderPropsResponse vfolder_folder_item_props_read(VFolderItem* item);
FolderPropsResponse vfolder_folder_item_props_write(VFolderItem* item);

/* Callback functions */
void vfolder_new_folder_cb(GtkAction* action, gpointer data);
void vfolder_remove_folder_cb(GtkAction* action, gpointer data);
void vfolder_properties_cb(GtkAction* action, gpointer data);

G_END_DECLS

#endif
