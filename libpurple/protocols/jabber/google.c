/**
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "debug.h"
#include "mediamanager.h"
#include "util.h"
#include "privacy.h"

#include "buddy.h"
#include "google.h"
#include "jabber.h"
#include "presence.h"
#include "iq.h"

#ifdef USE_VV

typedef struct {
	char *id;
	char *initiator;
} GoogleSessionId;

typedef enum {
	UNINIT,
	SENT_INITIATE,
	RECEIVED_INITIATE,
	IN_PRORESS,
	TERMINATED
} GoogleSessionState;

typedef struct {
	GoogleSessionId id;
	GoogleSessionState state;
	PurpleMedia *media;
	JabberStream *js; 
	char *remote_jid;
} GoogleSession;

GHashTable *sessions = NULL;

static guint 
google_session_id_hash(gconstpointer key) 
{
	GoogleSessionId *id = (GoogleSessionId*)key;
	
	guint id_hash = g_str_hash(id->id);
	guint init_hash = g_str_hash(id->initiator);

	return 23 * id_hash + init_hash;
}

static gboolean 
google_session_id_equal(gconstpointer a, gconstpointer b)
{
	GoogleSessionId *c = (GoogleSessionId*)a;
	GoogleSessionId *d = (GoogleSessionId*)b;
	
	return !strcmp(c->id, d->id) && !strcmp(c->initiator, d->initiator);
}

static void
google_session_destroy(GoogleSession *session)
{
	if (sessions != NULL)
		g_hash_table_remove(sessions, &(session->id));
	g_free(session->id.id);
	g_free(session->id.initiator);
	g_free(session->remote_jid);
	g_free(session);
}

static xmlnode *
google_session_create_xmlnode(GoogleSession *session, const char *type)
{
	xmlnode *node = xmlnode_new("session");
	xmlnode_set_namespace(node, "http://www.google.com/session");
	xmlnode_set_attrib(node, "id", session->id.id);
	xmlnode_set_attrib(node, "initiator", session->id.initiator);
	xmlnode_set_attrib(node, "type", type);
	return node;
}

static void
google_session_send_terminate(GoogleSession *session)
{
	xmlnode *sess;
	JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);

	xmlnode_set_attrib(iq->node, "to", session->remote_jid);
	sess = google_session_create_xmlnode(session, "terminate");
	xmlnode_insert_child(iq->node, sess);
	
	jabber_iq_send(iq);
	google_session_destroy(session);
}

static void 
google_session_send_candidates(PurpleMedia *media, gchar *session_id,
		gchar *participant, GoogleSession *session)
{
	JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);
	GList *candidates = purple_media_get_local_candidates(session->media, "google-voice",
							      session->remote_jid);
	PurpleMediaCandidate *transport;
	xmlnode *sess;
	xmlnode *candidate;
	sess = google_session_create_xmlnode(session, "candidates");
	xmlnode_insert_child(iq->node, sess);
	xmlnode_set_attrib(iq->node, "to", session->remote_jid);
	
	for (;candidates;candidates = candidates->next) {
		char port[8];
		char pref[8];
		transport = (PurpleMediaCandidate*)(candidates->data);

		if (!strcmp(transport->ip, "127.0.0.1"))
			continue;
	
		candidate = xmlnode_new("candidate");

		g_snprintf(port, sizeof(port), "%d", transport->port);
		g_snprintf(pref, sizeof(pref), "%d", transport->priority);

		xmlnode_set_attrib(candidate, "address", transport->ip);
		xmlnode_set_attrib(candidate, "port", port);
		xmlnode_set_attrib(candidate, "name", "rtp");
		xmlnode_set_attrib(candidate, "username", transport->username);
		/*
		 * As of this writing, Farsight 2 in Google compatibility
		 * mode doesn't provide a password. The Gmail client
		 * requires this to be set.
		 */
		xmlnode_set_attrib(candidate, "password",
				transport->password != NULL ?
				transport->password : "");
		xmlnode_set_attrib(candidate, "preference", pref);
		xmlnode_set_attrib(candidate, "protocol", transport->proto ==
				PURPLE_MEDIA_NETWORK_PROTOCOL_UDP ? "udp" : "tcp");
		xmlnode_set_attrib(candidate, "type", transport->type ==
				PURPLE_MEDIA_CANDIDATE_TYPE_HOST ? "local" :
						      transport->type ==
				PURPLE_MEDIA_CANDIDATE_TYPE_SRFLX ? "stun" :
					       	      transport->type ==
				PURPLE_MEDIA_CANDIDATE_TYPE_RELAY ? "relay" : NULL);
		xmlnode_set_attrib(candidate, "generation", "0");
		xmlnode_set_attrib(candidate, "network", "0");
		xmlnode_insert_child(sess, candidate);
		
	}
	jabber_iq_send(iq);
}

static void
google_session_ready(PurpleMedia *media, gchar *id,
		gchar *participant, GoogleSession *session)
{
	if (id == NULL && participant == NULL) {
		gchar *me = g_strdup_printf("%s@%s/%s",
				session->js->user->node,
				session->js->user->domain,
				session->js->user->resource);
		JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);
		xmlnode *sess, *desc, *payload;
		GList *codecs, *iter;

		if (!strcmp(session->id.initiator, me)) {
			xmlnode_set_attrib(iq->node, "to", session->remote_jid);
			xmlnode_set_attrib(iq->node, "from", session->id.initiator);
			sess = google_session_create_xmlnode(session, "initiate");
		} else {
			xmlnode_set_attrib(iq->node, "to", session->remote_jid);
			xmlnode_set_attrib(iq->node, "from", me);
			sess = google_session_create_xmlnode(session, "accept");
		}
		xmlnode_insert_child(iq->node, sess);
		desc = xmlnode_new_child(sess, "description");
		xmlnode_set_namespace(desc, "http://www.google.com/session/phone");

		codecs = purple_media_get_codecs(media, "google-voice");

		for (iter = codecs; iter; iter = g_list_next(iter)) {
			PurpleMediaCodec *codec = (PurpleMediaCodec*)iter->data;
			gchar *id = g_strdup_printf("%d", codec->id);
			gchar *clock_rate = g_strdup_printf("%d", codec->clock_rate);
			payload = xmlnode_new_child(desc, "payload-type");
			xmlnode_set_attrib(payload, "id", id);
			xmlnode_set_attrib(payload, "name", codec->encoding_name);
			xmlnode_set_attrib(payload, "clockrate", clock_rate);
			g_free(clock_rate);
			g_free(id);
		}
		purple_media_codec_list_free(codecs);

		jabber_iq_send(iq);

		google_session_send_candidates(session->media,
				"google-voice", session->remote_jid, session);
	}
}

static void
google_session_state_changed_cb(PurpleMedia *media,
		PurpleMediaStateChangedType type,
		gchar *sid, gchar *name, GoogleSession *session)
{
	if (sid == NULL && name == NULL) {
		if (type == PURPLE_MEDIA_STATE_CHANGED_END) {
			google_session_destroy(session);
		} else if (type == PURPLE_MEDIA_STATE_CHANGED_HANGUP) {
			xmlnode *sess;
			JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);

			xmlnode_set_attrib(iq->node, "to", session->remote_jid);
			sess = google_session_create_xmlnode(session, "terminate");
			xmlnode_insert_child(iq->node, sess);
	
			jabber_iq_send(iq);
		} else if (type == PURPLE_MEDIA_STATE_CHANGED_REJECTED) {
			xmlnode *sess;
			JabberIq *iq = jabber_iq_new(session->js, JABBER_IQ_SET);

			xmlnode_set_attrib(iq->node, "to", session->remote_jid);
			sess = google_session_create_xmlnode(session, "reject");
			xmlnode_insert_child(iq->node, sess);
	
			jabber_iq_send(iq);
		}
		
	}
}

PurpleMedia*
jabber_google_session_initiate(JabberStream *js, const gchar *who, PurpleMediaSessionType type)
{
	GoogleSession *session;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	gchar *jid;
	GParameter param;

	/* construct JID to send to */
	jb = jabber_buddy_find(js, who, FALSE);
	if (!jb) {
		purple_debug_error("jingle-rtp",
				"Could not find Jabber buddy\n");
		return NULL;
	}
	jbr = jabber_buddy_find_resource(jb, NULL);
	if (!jbr) {
		purple_debug_error("jingle-rtp",
				"Could not find buddy's resource\n");
	}

	if ((strchr(who, '/') == NULL) && jbr && (jbr->name != NULL)) {
		jid = g_strdup_printf("%s/%s", who, jbr->name);
	} else {
		jid = g_strdup(who);
	}

	session = g_new0(GoogleSession, 1);
	session->id.id = jabber_get_next_id(js);
	session->id.initiator = g_strdup_printf("%s@%s/%s", js->user->node,
			js->user->domain, js->user->resource);
	session->state = SENT_INITIATE;
	session->js = js;
	session->remote_jid = jid;

	session->media = purple_media_manager_create_media(
			purple_media_manager_get(), js->gc,
			"fsrtpconference", session->remote_jid, TRUE);

	/* GTalk requires the NICE_COMPATIBILITY_GOOGLE param */
	param.name = "compatibility-mode";
	memset(&param.value, 0, sizeof(GValue));
	g_value_init(&param.value, G_TYPE_UINT);
	g_value_set_uint(&param.value, 1); /* NICE_COMPATIBILITY_GOOGLE */

	if (purple_media_add_stream(session->media, "google-voice",
				session->remote_jid, PURPLE_MEDIA_AUDIO,
				"nice", 1, &param) == FALSE) {
		purple_media_error(session->media, "Error adding stream.");
		purple_media_hangup(session->media);
		google_session_destroy(session);
		return NULL;
	}

	g_signal_connect(G_OBJECT(session->media), "ready-new",
			G_CALLBACK(google_session_ready), session);
	g_signal_connect(G_OBJECT(session->media), "state-changed",
			G_CALLBACK(google_session_state_changed_cb), session);

	if (sessions == NULL)
		sessions = g_hash_table_new(google_session_id_hash,
				google_session_id_equal);
	g_hash_table_insert(sessions, &(session->id), session);

	return session->media;
}

static void
google_session_handle_initiate(JabberStream *js, GoogleSession *session, xmlnode *packet, xmlnode *sess)
{
	JabberIq *result;
	GList *codecs = NULL;
	xmlnode *desc_element, *codec_element;
	PurpleMediaCodec *codec;
	const char *id, *encoding_name,  *clock_rate;
	GParameter param;
		
	if (session->state != UNINIT) {
		purple_debug_error("jabber", "Received initiate for active session.\n");
		return;
	}

	session->media = purple_media_manager_create_media(purple_media_manager_get(), js->gc,
							   "fsrtpconference", session->remote_jid, FALSE);

	/* GTalk requires the NICE_COMPATIBILITY_GOOGLE param */
	param.name = "compatibility-mode";
	memset(&param.value, 0, sizeof(GValue));
	g_value_init(&param.value, G_TYPE_UINT);
	g_value_set_uint(&param.value, 1); /* NICE_COMPATIBILITY_GOOGLE */

	if (purple_media_add_stream(session->media, "google-voice", session->remote_jid, 
				PURPLE_MEDIA_AUDIO, "nice", 1, &param) == FALSE) {
		purple_media_error(session->media, "Error adding stream.");
		purple_media_hangup(session->media);
		google_session_send_terminate(session);
		return;
	}

	desc_element = xmlnode_get_child(sess, "description");
	
	for (codec_element = xmlnode_get_child(desc_element, "payload-type"); 
	     codec_element; 
	     codec_element = xmlnode_get_next_twin(codec_element)) {
		encoding_name = xmlnode_get_attrib(codec_element, "name");
		id = xmlnode_get_attrib(codec_element, "id");
		clock_rate = xmlnode_get_attrib(codec_element, "clockrate");

		codec = purple_media_codec_new(atoi(id), encoding_name, PURPLE_MEDIA_AUDIO,
				     clock_rate ? atoi(clock_rate) : 0);
		codecs = g_list_append(codecs, codec);
	}

	purple_media_set_remote_codecs(session->media, "google-voice", session->remote_jid, codecs);

	g_signal_connect(G_OBJECT(session->media), "ready-new",
			G_CALLBACK(google_session_ready), session);
	g_signal_connect(G_OBJECT(session->media), "state-changed",
			G_CALLBACK(google_session_state_changed_cb), session);

	purple_media_codec_list_free(codecs);
	
	result = jabber_iq_new(js, JABBER_IQ_RESULT);
	jabber_iq_set_id(result, xmlnode_get_attrib(packet, "id"));
	xmlnode_set_attrib(result->node, "to", session->remote_jid);
	jabber_iq_send(result);
}

static void 
google_session_handle_candidates(JabberStream  *js, GoogleSession *session, xmlnode *packet, xmlnode *sess)
{
	JabberIq *result;
	GList *list = NULL;
	xmlnode *cand;
	static int name = 0;
	char n[4];	
		
	for (cand = xmlnode_get_child(sess, "candidate"); cand; cand = xmlnode_get_next_twin(cand)) {
		PurpleMediaCandidate *info;
		g_snprintf(n, sizeof(n), "S%d", name++);
		info = purple_media_candidate_new(n, PURPLE_MEDIA_COMPONENT_RTP,
				!strcmp(xmlnode_get_attrib(cand, "type"), "local") ?
					PURPLE_MEDIA_CANDIDATE_TYPE_HOST :
			     		!strcmp(xmlnode_get_attrib(cand, "type"), "stun") ?
						PURPLE_MEDIA_CANDIDATE_TYPE_PRFLX :
			     			!strcmp(xmlnode_get_attrib(cand, "type"), "relay") ?
							PURPLE_MEDIA_CANDIDATE_TYPE_RELAY :
							PURPLE_MEDIA_CANDIDATE_TYPE_HOST,
						!strcmp(xmlnode_get_attrib(cand, "protocol"),"udp") ?
							PURPLE_MEDIA_NETWORK_PROTOCOL_UDP :
							PURPLE_MEDIA_NETWORK_PROTOCOL_TCP,
					xmlnode_get_attrib(cand, "address"),
					atoi(xmlnode_get_attrib(cand, "port")));

		info->username = g_strdup(xmlnode_get_attrib(cand, "username"));
		info->password = g_strdup(xmlnode_get_attrib(cand, "password"));

		list = g_list_append(list, info);
	}

	purple_media_add_remote_candidates(session->media, "google-voice", session->remote_jid, list);
	purple_media_candidate_list_free(list);

	result = jabber_iq_new(js, JABBER_IQ_RESULT);
	jabber_iq_set_id(result, xmlnode_get_attrib(packet, "id"));
	xmlnode_set_attrib(result->node, "to", session->remote_jid);
	jabber_iq_send(result);
}

static void
google_session_handle_accept(JabberStream *js, GoogleSession *session, xmlnode *packet, xmlnode *sess)
{
	xmlnode *desc_element = xmlnode_get_child(sess, "description");
	xmlnode *codec_element = xmlnode_get_child(desc_element, "payload-type");
	GList *codecs = NULL;

	for (; codec_element; codec_element =
			xmlnode_get_next_twin(codec_element)) {
		const gchar *encoding_name =
				xmlnode_get_attrib(codec_element, "name");
		const gchar *id = xmlnode_get_attrib(codec_element, "id");
		const gchar *clock_rate =
				xmlnode_get_attrib(codec_element, "clockrate");

		PurpleMediaCodec *codec = purple_media_codec_new(atoi(id),
				encoding_name, PURPLE_MEDIA_AUDIO,
				clock_rate ? atoi(clock_rate) : 0);
		codecs = g_list_append(codecs, codec);
	}

	purple_media_set_remote_codecs(session->media, "google-voice",
			session->remote_jid, codecs);

	purple_media_accept(session->media);
}

static void
google_session_handle_reject(JabberStream *js, GoogleSession *session, xmlnode *packet, xmlnode *sess)
{
	purple_media_end(session->media, NULL, NULL);
}

static void
google_session_handle_terminate(JabberStream *js, GoogleSession *session, xmlnode *packet, xmlnode *sess)
{
	purple_media_end(session->media, NULL, NULL);
}

static void
google_session_parse_iq(JabberStream *js, GoogleSession *session, xmlnode *packet)
{
	xmlnode *sess = xmlnode_get_child(packet, "session");	
	const char *type = xmlnode_get_attrib(sess, "type");

	if (!strcmp(type, "initiate")) {
		google_session_handle_initiate(js, session, packet, sess);
	} else if (!strcmp(type, "accept")) {
		google_session_handle_accept(js, session, packet, sess);
	} else if (!strcmp(type, "reject")) {
		google_session_handle_reject(js, session, packet, sess);
	} else if (!strcmp(type, "terminate")) {
		google_session_handle_terminate(js, session, packet, sess);
	} else if (!strcmp(type, "candidates")) {
		google_session_handle_candidates(js, session, packet, sess);
	}
}
#endif /* USE_VV */

void
jabber_google_session_parse(JabberStream *js, xmlnode *packet)
{
#ifdef USE_VV
	GoogleSession *session;
	GoogleSessionId id;

	xmlnode *session_node;
	xmlnode *desc_node;

	if (strcmp(xmlnode_get_attrib(packet, "type"), "set"))
		return;

	session_node = xmlnode_get_child(packet, "session");
	if (!session_node)
		return;

	id.id = (gchar*)xmlnode_get_attrib(session_node, "id");
	if (!id.id)
		return;

	id.initiator = (gchar*)xmlnode_get_attrib(session_node, "initiator");
	if (!id.initiator)
		return;

	if (sessions == NULL)
		sessions = g_hash_table_new(google_session_id_hash, google_session_id_equal);
	session = (GoogleSession*)g_hash_table_lookup(sessions, &id);

	if (session) {
		google_session_parse_iq(js, session, packet);
		return;
	}

	/* If the session doesn't exist, this has to be an initiate message */
	if (strcmp(xmlnode_get_attrib(session_node, "type"), "initiate"))
		return;
	desc_node = xmlnode_get_child(session_node, "description");
	if (!desc_node)
		return;
	session = g_new0(GoogleSession, 1);
	session->id.id = g_strdup(id.id);
	session->id.initiator = g_strdup(id.initiator);
	session->state = UNINIT;
	session->js = js;
	session->remote_jid = g_strdup(session->id.initiator);
	g_hash_table_insert(sessions, &(session->id), session);

	google_session_parse_iq(js, session, packet);
#else
	/* TODO: send proper error response */
#endif /* USE_VV */
}

static void
jabber_gmail_parse(JabberStream *js, xmlnode *packet, gpointer nul)
{
	const char *type = xmlnode_get_attrib(packet, "type");
	xmlnode *child;
	xmlnode *message, *sender_node, *subject_node;
	const char *from, *to, *url, *tid;
	char *subject;
	const char *in_str;
	char *to_name;
	char *default_tos[1];

	int i, count = 1, returned_count;

	const char **tos, **froms, **urls;
	char **subjects;

	if (strcmp(type, "result"))
		return;

	child = xmlnode_get_child(packet, "mailbox");
	if (!child)
		return;

	in_str = xmlnode_get_attrib(child, "total-matched");
	if (in_str && *in_str)
		count = atoi(in_str);

	/* If Gmail doesn't tell us who the mail is to, let's use our JID */
	to = xmlnode_get_attrib(packet, "to");
	default_tos[0] = jabber_get_bare_jid(to);

	message = xmlnode_get_child(child, "mail-thread-info");

	if (count == 0 || !message) {
		if (count > 0)
			purple_notify_emails(js->gc, count, FALSE, NULL, NULL, (const char**) default_tos, NULL, NULL, NULL);
		g_free(default_tos[0]);
		return;
	}

	/* Loop once to see how many messages were returned so we can allocate arrays
	 * accordingly */
	for (returned_count = 0; message; returned_count++, message=xmlnode_get_next_twin(message));

	froms    = g_new0(const char* , returned_count);
	tos      = g_new0(const char* , returned_count);
	subjects = g_new0(char* , returned_count);
	urls     = g_new0(const char* , returned_count);

	to = xmlnode_get_attrib(packet, "to");
	to_name = jabber_get_bare_jid(to);
	url = xmlnode_get_attrib(child, "url");
	if (!url || !*url)
		url = "http://www.gmail.com";

	message= xmlnode_get_child(child, "mail-thread-info");
	for (i=0; message; message = xmlnode_get_next_twin(message), i++) {
		subject_node = xmlnode_get_child(message, "subject");
		sender_node  = xmlnode_get_child(message, "senders");
		sender_node  = xmlnode_get_child(sender_node, "sender");

		while (sender_node && (!xmlnode_get_attrib(sender_node, "unread") ||
		       !strcmp(xmlnode_get_attrib(sender_node, "unread"),"0")))
			sender_node = xmlnode_get_next_twin(sender_node);

		if (!sender_node) {
			i--;
			continue;
		}

		from = xmlnode_get_attrib(sender_node, "name");
		if (!from || !*from)
			from = xmlnode_get_attrib(sender_node, "address");
		subject = xmlnode_get_data(subject_node);
		/*
		 * url = xmlnode_get_attrib(message, "url");
		 */
		tos[i] = (to_name != NULL ?  to_name : "");
		froms[i] = (from != NULL ?  from : "");
		subjects[i] = (subject != NULL ? subject : g_strdup(""));
		urls[i] = url;

		tid = xmlnode_get_attrib(message, "tid");
		if (tid &&
		    (js->gmail_last_tid == NULL || strcmp(tid, js->gmail_last_tid) > 0)) {
			g_free(js->gmail_last_tid);
			js->gmail_last_tid = g_strdup(tid);
		}
	}

	if (i>0)
		purple_notify_emails(js->gc, count, count == i, (const char**) subjects, froms, tos,
				urls, NULL, NULL);
	else
		purple_notify_emails(js->gc, count, FALSE, NULL, NULL, (const char**) default_tos, NULL, NULL, NULL);


	g_free(to_name);
	g_free(tos);
	g_free(default_tos[0]);
	g_free(froms);
	for (; i > 0; i--)
		g_free(subjects[i - 1]);
	g_free(subjects);
	g_free(urls);

	in_str = xmlnode_get_attrib(child, "result-time");
	if (in_str && *in_str) {
		g_free(js->gmail_last_time);
		js->gmail_last_time = g_strdup(in_str);
	}
}

void
jabber_gmail_poke(JabberStream *js, xmlnode *packet)
{
	const char *type;
	xmlnode *query;
	JabberIq *iq;

	/* bail if the user isn't interested */
	if (!purple_account_get_check_mail(js->gc->account))
		return;

	type = xmlnode_get_attrib(packet, "type");


	/* Is this an initial incoming mail notification? If so, send a request for more info */
	if (strcmp(type, "set") || !xmlnode_get_child(packet, "new-mail"))
		return;

	purple_debug(PURPLE_DEBUG_MISC, "jabber",
		   "Got new mail notification. Sending request for more info\n");

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "google:mail:notify");
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	query = xmlnode_get_child(iq->node, "query");

	if (js->gmail_last_time)
		xmlnode_set_attrib(query, "newer-than-time", js->gmail_last_time);
	if (js->gmail_last_tid)
		xmlnode_set_attrib(query, "newer-than-tid", js->gmail_last_tid);

	jabber_iq_send(iq);
	return;
}

void jabber_gmail_init(JabberStream *js) {
	JabberIq *iq;

	if (!purple_account_get_check_mail(js->gc->account))
		return;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "google:mail:notify");
	jabber_iq_set_callback(iq, jabber_gmail_parse, NULL);
	jabber_iq_send(iq);
}

void jabber_google_roster_init(JabberStream *js)
{
	JabberIq *iq;
	xmlnode *query;

	iq = jabber_iq_new_query(js, JABBER_IQ_GET, "jabber:iq:roster");
	query = xmlnode_get_child(iq->node, "query");

	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);
}

void jabber_google_roster_outgoing(JabberStream *js, xmlnode *query, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	char *jid_norm = g_strdup(jabber_normalize(account, jid));

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(item, "gr:t", "B");
			xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
			xmlnode_set_attrib(query, "gr:ext", "2");
			return;
		}
		list = list->next;
	}

	g_free(jid_norm);

}

gboolean jabber_google_roster_incoming(JabberStream *js, xmlnode *item)
{
	PurpleAccount *account = purple_connection_get_account(js->gc);
	GSList *list = account->deny;
	const char *jid = xmlnode_get_attrib(item, "jid");
	gboolean on_block_list = FALSE;

	char *jid_norm;

	const char *grt = xmlnode_get_attrib_with_namespace(item, "t", "google:roster");
	const char *subscription = xmlnode_get_attrib(item, "subscription");

	if (!subscription || !strcmp(subscription, "none")) {
		/* The Google Talk servers will automatically add people from your Gmail address book
		 * with subscription=none. If we see someone with subscription=none, ignore them.
		 */
		return FALSE;
	}

 	jid_norm = g_strdup(jabber_normalize(account, jid));

	while (list) {
		if (!strcmp(jid_norm, (char*)list->data)) {
			on_block_list = TRUE;
			break;
		}
		list = list->next;
	}

	if (grt && (*grt == 'H' || *grt == 'h')) {
		PurpleBuddy *buddy = purple_find_buddy(account, jid_norm);
		if (buddy)
			purple_blist_remove_buddy(buddy);
		g_free(jid_norm);
		return FALSE;
	}

	if (!on_block_list && (grt && (*grt == 'B' || *grt == 'b'))) {
		purple_debug_info("jabber", "Blocking %s\n", jid_norm);
		purple_privacy_deny_add(account, jid_norm, TRUE);
	} else if (on_block_list && (!grt || (*grt != 'B' && *grt != 'b' ))){
		purple_debug_info("jabber", "Unblocking %s\n", jid_norm);
		purple_privacy_deny_remove(account, jid_norm, TRUE);
	}

	g_free(jid_norm);
	return TRUE;
}

void jabber_google_roster_add_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	PurpleBuddy *b;
	JabberBuddy *jb;

	js = (JabberStream*)(gc->proto_data);

	if (!js || !js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
		return;

	jb = jabber_buddy_find(js, who, TRUE);

	buddies = purple_find_buddies(js->gc->account, who);
	if(!buddies)
		return;

	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, g->name, -1);

		buddies = buddies->next;
	}

	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");
	xmlnode_set_attrib(item, "gr:t", "B");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* Synthesize a sign-off */
	if (jb) {
		JabberBuddyResource *jbr;
		GList *l = jb->resources;
		while (l) {
			jbr = l->data;
			if (jbr && jbr->name)
			{
				purple_debug(PURPLE_DEBUG_MISC, "jabber", "Removing resource %s\n", jbr->name);
				jabber_buddy_remove_resource(jb, jbr->name);
			}
			l = l->next;
		}
	}
	purple_prpl_got_user_status(purple_connection_get_account(gc), who, "offline", NULL);
}

void jabber_google_roster_rem_deny(PurpleConnection *gc, const char *who)
{
	JabberStream *js;
	GSList *buddies;
	JabberIq *iq;
	xmlnode *query;
	xmlnode *item;
	xmlnode *group;
	PurpleBuddy *b;

	g_return_if_fail(gc != NULL);
	g_return_if_fail(who != NULL);

	js = (JabberStream*)(gc->proto_data);

	if (!js || !js->server_caps & JABBER_CAP_GOOGLE_ROSTER)
		return;

	buddies = purple_find_buddies(js->gc->account, who);
	if(!buddies)
		return;

	b = buddies->data;

	iq = jabber_iq_new_query(js, JABBER_IQ_SET, "jabber:iq:roster");

	query = xmlnode_get_child(iq->node, "query");
	item = xmlnode_new_child(query, "item");

	while(buddies) {
		PurpleGroup *g;

		b = buddies->data;
		g = purple_buddy_get_group(b);

		group = xmlnode_new_child(item, "group");
		xmlnode_insert_data(group, g->name, -1);

		buddies = buddies->next;
	}

	xmlnode_set_attrib(item, "jid", who);
	xmlnode_set_attrib(item, "name", b->alias ? b->alias : "");
	xmlnode_set_attrib(query, "xmlns:gr", "google:roster");
	xmlnode_set_attrib(query, "gr:ext", "2");

	jabber_iq_send(iq);

	/* See if he's online */
	jabber_presence_subscription_set(js, who, "probe");
}

/* This does two passes on the string. The first pass goes through
 * and determine if all the structured text is properly balanced, and
 * how many instances of each there is. The second pass goes and converts
 * everything to HTML, depending on what's figured out by the first pass.
 * It will short circuit once it knows it has no more replacements to make
 */
char *jabber_google_format_to_html(const char *text)
{
	const char *p;

	/* The start of the screen may be consdiered a space for this purpose */
	gboolean preceding_space = TRUE;

	gboolean in_bold = FALSE, in_italic = FALSE;
	gboolean in_tag = FALSE;

	gint bold_count = 0, italic_count = 0;

	GString *str;

	for (p = text; *p != '\0'; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);
		if (c == '*' && !in_tag) {
			if (in_bold && (g_unichar_isspace(*(p+1)) ||
					*(p+1) == '\0' ||
					*(p+1) == '<')) {
				bold_count++;
				in_bold = FALSE;
			} else if (preceding_space && !in_bold && !g_unichar_isspace(*(p+1))) {
				bold_count++;
				in_bold = TRUE;
			}
			preceding_space = TRUE;
		} else if (c == '_' && !in_tag) {
			if (in_italic && (g_unichar_isspace(*(p+1)) ||
					*(p+1) == '\0' ||
					*(p+1) == '<')) {
				italic_count++;
				in_italic = FALSE;
			} else if (preceding_space && !in_italic && !g_unichar_isspace(*(p+1))) {
				italic_count++;
				in_italic = TRUE;
			}
			preceding_space = TRUE;
		} else if (c == '<' && !in_tag) {
			in_tag = TRUE;
		} else if (c == '>' && in_tag) {
			in_tag = FALSE;
		} else if (!in_tag) {
			if (g_unichar_isspace(c))
				preceding_space = TRUE;
			else
				preceding_space = FALSE;
		}
	}

	str  = g_string_new(NULL);
	in_bold = in_italic = in_tag = FALSE;
	preceding_space = TRUE;

	for (p = text; *p != '\0'; p = g_utf8_next_char(p)) {
		gunichar c = g_utf8_get_char(p);

		if (bold_count < 2 && italic_count < 2 && !in_bold && !in_italic) {
			g_string_append(str, p);
			return g_string_free(str, FALSE);
		}


		if (c == '*' && !in_tag) {
			if (in_bold &&
			    (g_unichar_isspace(*(p+1))||*(p+1)=='<')) { /* This is safe in UTF-8 */
				str = g_string_append(str, "</b>");
				in_bold = FALSE;
				bold_count--;
			} else if (preceding_space && bold_count > 1 && !g_unichar_isspace(*(p+1))) {
				str = g_string_append(str, "<b>");
				bold_count--;
				in_bold = TRUE;
			} else {
				str = g_string_append_unichar(str, c);
			}
			preceding_space = TRUE;
		} else if (c == '_' && !in_tag) {
			if (in_italic &&
			    (g_unichar_isspace(*(p+1))||*(p+1)=='<')) {
				str = g_string_append(str, "</i>");
				italic_count--;
				in_italic = FALSE;
			} else if (preceding_space && italic_count > 1 && !g_unichar_isspace(*(p+1))) {
				str = g_string_append(str, "<i>");
				italic_count--;
				in_italic = TRUE;
			} else {
				str = g_string_append_unichar(str, c);
			}
			preceding_space = TRUE;
		} else if (c == '<' && !in_tag) {
			str = g_string_append_unichar(str, c);
			in_tag = TRUE;
		} else if (c == '>' && in_tag) {
			str = g_string_append_unichar(str, c);
			in_tag = FALSE;
		} else if (!in_tag) {
			str = g_string_append_unichar(str, c);
			if (g_unichar_isspace(c))
				preceding_space = TRUE;
			else
				preceding_space = FALSE;
		} else {
			str = g_string_append_unichar(str, c);
		}
	}
	return g_string_free(str, FALSE);
}

void jabber_google_presence_incoming(JabberStream *js, const char *user, JabberBuddyResource *jbr)
{
	if (!js->googletalk)
		return;
	if (jbr->status && !strncmp(jbr->status, "♫ ", strlen("♫ "))) {
		purple_prpl_got_user_status(js->gc->account, user, "tune",
					    PURPLE_TUNE_TITLE, jbr->status + strlen("♫ "), NULL);
		jbr->status = NULL;
	} else {
		purple_prpl_got_user_status_deactive(js->gc->account, user, "tune");
	}
}

char *jabber_google_presence_outgoing(PurpleStatus *tune)
{
	const char *attr = purple_status_get_attr_string(tune, PURPLE_TUNE_TITLE);
	return attr ? g_strdup_printf("♫ %s", attr) : g_strdup("");
}
