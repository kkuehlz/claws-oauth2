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

#include <json-c/json.h>
#include <curl/curl.h>

#include "oauth2.h"
#include "passwordstore.h"

static size_t oauth_refresh_cb(void *data, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct curl_oauth_refresh *p = (struct curl_oauth_refresh *) userp;

	p->payload = g_realloc(p->payload, p->size +  realsize + 1);
	if (p->payload == NULL)
		return 0;

	memcpy(&(p->payload[p->size]), data, realsize);
	p->size += realsize;
	p->payload[p->size] = 0;

	return realsize;
}

static gchar *oauth_refresh_api(gchar *client_id,
				gchar *client_secret,
				gchar *refresh_server,
				gchar *refresh_token) {

	CURL *ch = NULL;
	gchar *ret = NULL, *form_data = NULL;
	json_object *refresh_json = NULL, *access_token_json;
	enum json_tokener_error jerr;
	CURLcode res;

	struct curl_slist *headers = NULL;
	struct curl_oauth_refresh sr;

	memset(&sr, 0, sizeof(sr));
	if ((ch = curl_easy_init()) == NULL)
		goto done;

	if ((headers = curl_slist_append(headers,
			"Content-Type: application/x-www-form-urlencoded")) == NULL)
		goto done;

	form_data = g_strdup_printf("grant_type=refresh_token&client_id=%s&client_secret=%s&refresh_token=%s",
			client_id, client_secret, refresh_token);

	curl_easy_setopt(ch, CURLOPT_URL, refresh_server);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, form_data);
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, oauth_refresh_cb);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) &sr);
	curl_easy_setopt(ch, CURLOPT_TIMEOUT, 5L);
	curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1L);
	res = curl_easy_perform(ch);

	if (res != CURLE_OK || sr.size <= 0)
		goto done;

	refresh_json = json_tokener_parse_verbose(sr.payload, &jerr);
	if (jerr != json_tokener_success)
		goto done;

	if (!json_object_object_get_ex(refresh_json, "access_token", &access_token_json))
		goto done;
	ret = json_object_get_string(access_token_json);
	if (ret == NULL)
		goto done;
	ret = g_strdup(ret); /* json-c manages this memory */

done:
	if (ch)
		curl_easy_cleanup(ch);
	if (headers)
		curl_slist_free_all(headers);
	if (form_data)
		g_free(form_data);
	if (refresh_json)
		json_object_put(refresh_json);

	return ret;
}

gchar *oauth2_get_access_token(PrefsAccount *account) {
	gchar *client_id = NULL, *client_secret = NULL, *refresh_token = NULL;
	gchar *access_token = NULL;
	gint ok;

	/* TODO(keur): Use hooks here? */
	client_id = passwd_store_get_account(account->account_id,
			PWS_OAUTH_CLIENT_ID);

	client_secret = passwd_store_get_account(account->account_id,
			PWS_OAUTH_CLIENT_SECRET);

	refresh_token = passwd_store_get_account(account->account_id,
			PWS_OAUTH_REFRESH_TOKEN);

	if (client_id != NULL && client_secret != NULL && refresh_token != NULL)
		access_token = oauth_refresh_api(client_id,
				client_secret,
				account->oauth_refresh_server,
				refresh_token);

	if (client_id)
		g_free(client_id);
	if (client_secret)
		g_free(client_secret);
	if (refresh_token)
		g_free(refresh_token);

	return access_token;
}
