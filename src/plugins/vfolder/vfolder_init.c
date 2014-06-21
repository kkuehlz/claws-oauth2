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

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"

#include "mimeview.h"
#include "utils.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "menu.h"
#include "vfolder.h"
#include "vfolder_gtk.h"

#define PLUGIN_NAME (_("VFolder"))

static GtkActionEntry vfolder_main_menu[] = {{
	"View/CreateVfolder",
	NULL, N_("Create virtual folder..."),
	"<Control>v", N_("Create a virtual folder"),
	G_CALLBACK(vfolder_new_folder_cb)
}};

static gint main_menu_id = 0;

gint plugin_init(gchar** error) {
	MainWindow *mainwin = mainwindow_get_mainwindow();

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(0,0,1,0),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	gtk_action_group_add_actions(mainwin->action_group, vfolder_main_menu,
			1, (gpointer)mainwin->folderview);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/View", "CreateVfolder",
			  "View/CreateVfolder", GTK_UI_MANAGER_MENUITEM,
			  main_menu_id)

	if (! vfolder_init()) {
		debug_print("vfolder plugin unloading due to init errors\n");
		plugin_done();
		return -1;
	}

	debug_print("vfolder plugin loaded\n");

	return 0;
}

gboolean plugin_done(void) {
	MainWindow *mainwin = mainwindow_get_mainwindow();

	vfolder_done();

	if (mainwin) {
		MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "View/CreateVfolder", main_menu_id);
		main_menu_id = 0;
	}

	debug_print("vfolder plugin unloaded\n");

	return TRUE;
}

const gchar* plugin_licence(void) {
	return "GPL3+";
}

const gchar* plugin_version(void) {
	return VERSION;
}

const gchar* plugin_type(void) {
	return "GTK2";
}

const gchar* plugin_name(void) {
	return PLUGIN_NAME;
}

const gchar* plugin_desc(void) {
	return _("This plugin adds virtual folder support to Claws Mail.\n"
			"\n"
			"1) Select one or more mail folder(s) to use as the basic mail pool\n"
			"2) Define a filter\n"
			"3) Specify name for virtual folder\n"
			"4) Press create and wait until the scanning of the mail pool finishes\n"
			"\n"
			"The VFolder will be updated periodically and when claws-mail is initially opened\n"
			"Manual update is available from the context menu of the VFolder.\n"
			"\n"
			"The supported folder types are MH and IMAP.\n"
			"Messages in a VFolder cannot be updated.\n"
			"\n"
			"To create a VFolder go to /View/Create virtual folder or choose a folder\n"
			"in the folder view an press <Control>+v\n"
			);
}

struct PluginFeature* plugin_provides(void) {
	static struct PluginFeature features[] =
	{ {PLUGIN_UTILITY, N_("VFolder")},
	  {PLUGIN_FOLDERCLASS, N_("VFolder")},
	  {PLUGIN_NOTHING, NULL} };
	return features;
}
