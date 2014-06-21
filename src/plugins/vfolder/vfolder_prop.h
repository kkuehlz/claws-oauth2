/*
 * $Id: $
* vim:et:ts=4:sw=4:et:sts=4:ai:set list listchars=tab\:»·,trail\:·:
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

#ifndef __VFOLDER_PROP_H__
#define __VFOLDER_PROP_H__

#include <glib.h>

G_BEGIN_DECLS

#include "folder.h"
#include "vfolder.h"
#include <gtk/gtk.h>

gboolean vfolder_create_item_dialog(VFolderItem* vitem, FolderItem* item);
gboolean vfolder_edit_item_dialog(VFolderItem* vitem, FolderItem* item);
void vfolder_set_msgs_filter(VFolderItem* vfolder_item);
void vfolder_item_props_response(FolderPropsResponse resp);

G_END_DECLS

#endif
