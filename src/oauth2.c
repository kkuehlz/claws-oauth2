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

#include "claws.h"
#include "oauth2.h"
#include "passwordstore.h"
#include "utils.h"

static size_t oauth2_refresh_cb(void *data, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct curl_oauth2_refresh *p = (struct curl_oauth2_refresh *) userp;

	p->payload = g_realloc(p->payload, p->size +  realsize + 1);
	if (p->payload == NULL)
		return 0;

	memcpy(&(p->payload[p->size]), data, realsize);
	p->size += realsize;
	p->payload[p->size] = 0;

	return realsize;
}

static void oauth2_refresh_api_thr(void *arg) {
	CURL *ch = NULL;
	gchar *ret = NULL, *form_data = NULL;
	json_object *refresh_json = NULL, *access_token_json;
	enum json_tokener_error jerr;
	CURLcode res;
	struct curl_slist *headers = NULL;
	struct curl_oauth2_refresh sr;

	OAuth2Ctx *ctx = (OAuth2Ctx *) arg;
	ctx->success = FALSE;

	memset(&sr, 0, sizeof(sr));
	if ((ch = curl_easy_init()) == NULL)
		goto done;

	if ((headers = curl_slist_append(headers,
			"Content-Type: application/x-www-form-urlencoded")) == NULL)
		goto done;

	form_data = g_strdup_printf(
			"grant_type=refresh_token&client_id=%s&client_secret=%s&refresh_token=%s",
			ctx->client_id,
			ctx->client_secret,
			ctx->refresh_token);

	curl_easy_setopt(ch, CURLOPT_URL, ctx->refresh_server);
	curl_easy_setopt(ch, CURLOPT_POSTFIELDS, form_data);
	curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, oauth2_refresh_cb);
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
	ctx->access_token = g_strdup(ret); /* json-c manages this memory */
	ctx->success = TRUE;

done:
	if (ch)
		curl_easy_cleanup(ch);
	if (headers)
		curl_slist_free_all(headers);
	if (form_data)
		g_free(form_data);
	if (refresh_json)
		json_object_put(refresh_json);
	ctx->ready = TRUE;
}

static void oauth2_refresh_api(OAuth2Ctx *ctx) {
#ifdef USE_PTHREAD
	pthread_t pt;
#endif

	g_return_if_fail(ctx != NULL);

#ifdef USE_PTHREAD
	if (pthread_create(&pt, NULL, oauth2_refresh_api_thr,
				(void *) ctx) != 0) {
		oauth2_refresh_api_thr(ctx);
	} else {
		debug_print("OAUTH2: Waiting for thread to finish\n");
		while (!ctx->ready) {
			claws_do_idle();
		}
		debug_print("OAUTH2: Thread finished\n");
		pthread_join(pt, NULL);
	}
#else
	oauth2_refresh_api_thr(ctx);
#endif
}

gchar *oauth2_get_access_token(PrefsAccount *account) {
	OAuth2Ctx *ctx = NULL;
	gchar *client_id = NULL, *client_secret = NULL, *refresh_token = NULL;
	gchar *access_token = NULL;
	gint ok;

	ctx = g_new0(OAuth2Ctx, 1);
	if (ctx == NULL)
		goto done;

	/* TODO(keur): Use hooks here? */
	ctx->client_id = passwd_store_get_account(account->account_id,
			PWS_OAUTH_CLIENT_ID);
	if (ctx->client_id == NULL)
		goto done;

	ctx->client_secret = passwd_store_get_account(account->account_id,
			PWS_OAUTH_CLIENT_SECRET);
	if (ctx->client_id == NULL)
		goto done;

	ctx->refresh_token = passwd_store_get_account(account->account_id,
			PWS_OAUTH_REFRESH_TOKEN);
	if (ctx->refresh_token == NULL)
		goto done;
	ctx->refresh_server = account->oauth_refresh_server;

	oauth2_refresh_api(ctx);
	access_token = ctx->success == TRUE ? ctx->access_token : NULL;

done:
	if (ctx) {
		if (ctx->client_id)
			g_free(ctx->client_id);
		if (ctx->client_secret)
			g_free(ctx->client_secret);
		if (ctx->refresh_token)
			g_free(ctx->refresh_token);
		g_free(ctx);
	}

	return access_token;
}
