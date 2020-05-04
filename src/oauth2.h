/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 Hiroyuki Yamamoto and the Claws Mail team
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
 */

#ifndef OAUTH2_H
#define OAUTH2_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef HAVE_OAUTH2

#include <glib.h>

#include "prefs_account.h"

struct curl_oauth2_refresh {
	gchar *payload;
	size_t size;
};

struct _OAuth2Ctx {
    gchar *client_id;
    gchar *client_secret;
    gchar *refresh_server;
    gchar *refresh_token;
    gchar *access_token;
    gboolean success;
    gboolean ready;
};

typedef struct _OAuth2Ctx OAuth2Ctx;

gchar *oauth2_get_access_token(PrefsAccount *account);
#endif /* HAVE_OAUTH2 */

#endif /* OAUTH2_H */
