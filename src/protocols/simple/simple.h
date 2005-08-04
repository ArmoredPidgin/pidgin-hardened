/**
 * @file simple.h
 * 
 * gaim
 *
 * Copyright (C) 2005, Thomas Butter <butter@uni-mannheim.de>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _GAIM_SIMPLE_H
#define _GAIM_SIMPLE_H

#include <glib.h>
#include <time.h>
#include <prpl.h>
#include <digcalc.h>
#include "sipmsg.h"

#define SIMPLE_BUF_INC 1024

struct sip_dialog {
	gchar *ourtag;
	gchar *theirtag;
	gchar *callid;
};

struct simple_watcher {
	gchar *name;
	time_t expire;
	struct sip_dialog dialog;
};

struct simple_buddy {
	gchar *name;
	time_t resubscribe;
};

struct sip_auth {
        gchar *nonce;
        gchar *realm;
	int nc;
        HASHHEX HA1;
	int fouroseven;
};

struct simple_account_data {
	GaimConnection *gc;
	gchar *servername;
	gchar *username;
	gchar *password;
	int fd;
	int cseq;
	time_t reregister;
	time_t republish;
	int registerstatus; // 0 nothing, 1 first registration send, 2 auth received, 3 registered
	struct sip_auth registrar;
	struct sip_auth proxy;
	int listenfd;
	int listenport;
	gchar *ip;
	gchar *status;
	GHashTable *buddies;
	guint registertimeout;
	int connecting;
	GaimAccount *account;
	gchar *sendlater;
	GSList *transactions;
	GSList *watcher;
	GSList *openconns;
	gboolean udp;
	struct sockaddr_in serveraddr;
};

struct sip_connection {
	int fd;
	gchar *inbuf;
	int inbuflen;
	int inbufused;
	int inputhandler;
};

struct transaction;

typedef gboolean (*TransCallback) (struct simple_account_data *, struct sipmsg *, struct transaction *);

struct transaction {
	time_t time;
	int retries;
	int transport; // 0 = tcp, 1 = udp
	int fd;
	gchar *cseq;
	struct sipmsg *msg;
	TransCallback callback;
};

#endif /* _GAIM_SIMPLE_H */
