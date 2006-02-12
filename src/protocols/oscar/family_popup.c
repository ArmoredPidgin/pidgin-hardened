/*
 * Gaim's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Family 0x0008 - Popups.
 *
 * Popups are just what it sounds like.  They're a way for the server to
 * open up an informative box on the client's screen.
 */

#include <oscar.h>

/*
 * This is all there is to it.
 *
 * The message is probably HTML.
 *
 */
static int parsepopup(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_rxcallback_t userfunc;
	aim_tlvlist_t *tl;
	int ret = 0;
	char *msg, *url;
	guint16 width, height, delay;

	tl = aim_tlvlist_read(bs);

	msg = aim_tlv_getstr(tl, 0x0001, 1);
	url = aim_tlv_getstr(tl, 0x0002, 1);
	width = aim_tlv_get16(tl, 0x0003, 1);
	height = aim_tlv_get16(tl, 0x0004, 1);
	delay = aim_tlv_get16(tl, 0x0005, 1);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, msg, url, width, height, delay);

	aim_tlvlist_free(&tl);
	free(msg);
	free(url);

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if (snac->subtype == 0x0002)
		return parsepopup(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int popups_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0x0008;
	mod->version = 0x0001;
	mod->toolid = 0x0104;
	mod->toolversion = 0x0001;
	mod->flags = 0;
	strncpy(mod->name, "popup", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
