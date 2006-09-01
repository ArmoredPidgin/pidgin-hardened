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
 * I feel like this is a good place to explain OFT, so I'm going to
 * do just that.  Each OFT packet has a header type.  I guess this
 * is pretty similar to the subtype of a SNAC packet.  The type
 * basically tells the other client the meaning of the OFT packet.
 * There are two distinct types of file transfer, which I usually
 * call "sendfile" and "getfile."  Sendfile is when you send a file
 * to another AIM user.  Getfile is when you share a group of files,
 * and other users request that you send them the files.
 *
 * A typical sendfile file transfer goes like this:
 *   1) Sender sends a channel 2 ICBM telling the other user that
 *      we want to send them a file.  At the same time, we open a
 *      listener socket (this should be done before sending the
 *      ICBM) on some port, and wait for them to connect to us.
 *      The ICBM we sent should contain our IP address and the port
 *      number that we're listening on.
 *   2) The receiver connects to the sender on the given IP address
 *      and port.  After the connection is established, the receiver
 *      sends an ICBM signifying that we are ready and waiting.
 *   3) The sender sends an OFT PROMPT message over the OFT
 *      connection.
 *   4) The receiver of the file sends back an exact copy of this
 *      OFT packet, except the cookie is filled in with the cookie
 *      from the ICBM.  I think this might be an attempt to verify
 *      that the user that is connected is actually the guy that
 *      we sent the ICBM to.  Oh, I've been calling this the ACK.
 *   5) The sender starts sending raw data across the connection
 *      until the entire file has been sent.
 *   6) The receiver knows the file is finished because the sender
 *      sent the file size in an earlier OFT packet.  So then the
 *      receiver sends the DONE thingy (after filling in the
 *      "received" checksum and size) and closes the connection.
 */

#include "oscar.h"
#include "peer.h"

/**
 * Calculate oft checksum of buffer
 *
 * Prevcheck should be 0xFFFF0000 when starting a checksum of a file.  The
 * checksum is kind of a rolling checksum thing, so each time you get bytes
 * of a file you just call this puppy and it updates the checksum.  You can
 * calculate the checksum of an entire file by calling this in a while or a
 * for loop, or something.
 *
 * Thanks to Graham Booker for providing this improved checksum routine,
 * which is simpler and should be more accurate than Josh Myer's original
 * code. -- wtm
 *
 * This algorithm works every time I have tried it.  The other fails
 * sometimes.  So, AOL who thought this up?  It has got to be the weirdest
 * checksum I have ever seen.
 *
 * @param buffer Buffer of data to checksum.  Man I'd like to buff her...
 * @param bufsize Size of buffer.
 * @param prevchecksum Previous checksum.
 */
static guint32
peer_oft_checksum_chunk(const guint8 *buffer, int bufferlen, guint32 prevchecksum)
{
	guint32 checksum, oldchecksum;
	int i;
	unsigned short val;

	checksum = (prevchecksum >> 16) & 0xffff;
	for (i = 0; i < bufferlen; i++)
	{
		oldchecksum = checksum;
		if (i & 1)
			val = buffer[i];
		else
			val = buffer[i] << 8;
		checksum -= val;
		/*
		 * The following appears to be necessary.... It happens
		 * every once in a while and the checksum doesn't fail.
		 */
		if (checksum > oldchecksum)
			checksum--;
	}
	checksum = ((checksum & 0x0000ffff) + (checksum >> 16));
	checksum = ((checksum & 0x0000ffff) + (checksum >> 16));
	return checksum << 16;
}

static guint32
peer_oft_checksum_file(char *filename)
{
	FILE *fd;
	guint32 checksum = 0xffff0000;

	if ((fd = fopen(filename, "rb")))
	{
		int bytes;
		guint8 buffer[1024];

		while ((bytes = fread(buffer, 1, 1024, fd)) != 0)
			checksum = peer_oft_checksum_chunk(buffer, bytes, checksum);
		fclose(fd);
	}

	return checksum;
}

/**
 * Free any OFT related data.
 */
void
peer_oft_close(PeerConnection *conn)
{
	/*
	 * If canceled by local user, and we're receiving a file, and
	 * we're not connected/ready then send an ICBM cancel message.
	 */
	if ((gaim_xfer_get_status(conn->xfer) == GAIM_XFER_STATUS_CANCEL_LOCAL) &&
		!conn->ready)
	{
		aim_im_sendch2_cancel(conn);
	}

	if (conn->sending_data_timer != 0)
	{
		gaim_timeout_remove(conn->sending_data_timer);
		conn->sending_data_timer = 0;
	}
}

/**
 * Write the given OftFrame to a ByteStream and send it out
 * on the established PeerConnection.
 */
static void
peer_oft_send(PeerConnection *conn, OftFrame *frame)
{
	size_t length;
	ByteStream bs;

	length = 192 + MAX(64, frame->name_length + 1);
	byte_stream_init(&bs, malloc(length), length);
	byte_stream_putraw(&bs, conn->magic, 4);
	byte_stream_put16(&bs, length);
	byte_stream_put16(&bs, frame->type);
	byte_stream_putraw(&bs, frame->cookie, 8);
	byte_stream_put16(&bs, frame->encrypt);
	byte_stream_put16(&bs, frame->compress);
	byte_stream_put16(&bs, frame->totfiles);
	byte_stream_put16(&bs, frame->filesleft);
	byte_stream_put16(&bs, frame->totparts);
	byte_stream_put16(&bs, frame->partsleft);
	byte_stream_put32(&bs, frame->totsize);
	byte_stream_put32(&bs, frame->size);
	byte_stream_put32(&bs, frame->modtime);
	byte_stream_put32(&bs, frame->checksum);
	byte_stream_put32(&bs, frame->rfrcsum);
	byte_stream_put32(&bs, frame->rfsize);
	byte_stream_put32(&bs, frame->cretime);
	byte_stream_put32(&bs, frame->rfcsum);
	byte_stream_put32(&bs, frame->nrecvd);
	byte_stream_put32(&bs, frame->recvcsum);
	byte_stream_putraw(&bs, frame->idstring, 32);
	byte_stream_put8(&bs, frame->flags);
	byte_stream_put8(&bs, frame->lnameoffset);
	byte_stream_put8(&bs, frame->lsizeoffset);
	byte_stream_putraw(&bs, frame->dummy, 69);
	byte_stream_putraw(&bs, frame->macfileinfo, 16);
	byte_stream_put16(&bs, frame->nencode);
	byte_stream_put16(&bs, frame->nlanguage);
	/*
	 * The name can be more than 64 characters, but if it is less than
	 * 64 characters it is padded with NULLs.
	 */
	byte_stream_putraw(&bs, frame->name, MAX(64, frame->name_length + 1));

	peer_connection_send(conn, &bs);

	free(bs.data);
}

void
peer_oft_send_prompt(PeerConnection *conn)
{
	conn->xferdata.type = PEER_TYPE_PROMPT;
	peer_oft_send(conn, &conn->xferdata);
}

static void
peer_oft_send_ack(PeerConnection *conn)
{
	conn->xferdata.type = PEER_TYPE_ACK;

	/* Fill in the cookie */
	memcpy(conn->xferdata.cookie, conn->cookie, 8);

	peer_oft_send(conn, &conn->xferdata);
}

static void
peer_oft_send_done(PeerConnection *conn)
{
	conn->xferdata.type = PEER_TYPE_DONE;
	conn->xferdata.filesleft = 0;
	conn->xferdata.partsleft = 0;
	conn->xferdata.nrecvd = gaim_xfer_get_bytes_sent(conn->xfer);
	peer_oft_send(conn, &conn->xferdata);
}

/**
 * This function exists so that we don't remove the outgoing
 * data watcher while we're still sending data.  In most cases
 * any data we're sending will be instantly wisked away to a TCP
 * buffer maintained by our operating system... but we want to
 * make sure the core doesn't start sending file data while
 * we're still sending OFT frame data.  That would be bad.
 */
static gboolean
start_transfer_when_done_sending_data(gpointer data)
{
	PeerConnection *conn;

	conn = data;

	if (gaim_circ_buffer_get_max_read(conn->buffer_outgoing) == 0)
	{
		conn->sending_data_timer = 0;
		conn->xfer->fd = conn->fd;
		conn->fd = -1;
		gaim_xfer_start(conn->xfer, conn->xfer->fd, NULL, 0);
		return FALSE;
	}

	return TRUE;
}

/**
 * This function is similar to the above function, except instead
 * of starting the xfer it will destroy the connection.  This is
 * used when you want to send one final message across the peer
 * connection, and then close everything.
 */
static gboolean
destroy_connection_when_done_sending_data(gpointer data)
{
	PeerConnection *conn;

	conn = data;

	if (gaim_circ_buffer_get_max_read(conn->buffer_outgoing) == 0)
	{
		conn->sending_data_timer = 0;
		peer_connection_destroy(conn, conn->disconnect_reason, NULL);
		return FALSE;
	}

	return TRUE;
}

/*
 * This is called when a buddy sends us some file info.  This happens when they
 * are sending a file to you, and you have just established a connection to them.
 * You should send them the exact same info except use the real cookie.  We also
 * get like totally ready to like, receive the file, kay?
 */
static void
peer_oft_recv_frame_prompt(PeerConnection *conn, OftFrame *frame)
{
	/* Record the file information and send an ack */
	memcpy(&conn->xferdata, frame, sizeof(OftFrame));
	peer_oft_send_ack(conn);

	/* Remove our watchers and use the file transfer watchers in the core */
	gaim_input_remove(conn->watcher_incoming);
	conn->watcher_incoming = 0;
	conn->sending_data_timer = gaim_timeout_add(100,
			start_transfer_when_done_sending_data, conn);
}

/**
 * We are sending a file to someone else.  They have just acknowledged our
 * prompt, so we want to start sending data like there's no tomorrow.
 */
static void
peer_oft_recv_frame_ack(PeerConnection *conn, OftFrame *frame)
{
	if (memcmp(conn->cookie, frame->cookie, 8))
	{
		gaim_debug_info("oscar", "Received an incorrect cookie.  "
				"Closing connection.\n");
		peer_connection_destroy(conn, OSCAR_DISCONNECT_INVALID_DATA, NULL);
		return;
	}

	/* Remove our watchers and use the file transfer watchers in the core */
	gaim_input_remove(conn->watcher_incoming);
	conn->watcher_incoming = 0;
	conn->sending_data_timer = gaim_timeout_add(100,
			start_transfer_when_done_sending_data, conn);
}

/*
 * We just sent a file to someone.  They said they got it and everything,
 * so we can close our direct connection and what not.
 */
static void
peer_oft_recv_frame_done(PeerConnection *conn, OftFrame *frame)
{
	gaim_input_remove(conn->watcher_incoming);
	conn->watcher_incoming = 0;
	conn->xfer->fd = conn->fd;
	conn->fd = -1;
	gaim_xfer_end(conn->xfer);
}

/**
 * Handle an incoming OftFrame.  If there is a payload associated
 * with this frame, then we remove the old watcher and add the
 * OFT watcher to read in the payload.
 */
void
peer_oft_recv_frame(PeerConnection *conn, ByteStream *bs)
{
	OftFrame frame;

	frame.type = byte_stream_get16(bs);
	byte_stream_getrawbuf(bs, frame.cookie, 8);
	frame.encrypt = byte_stream_get16(bs);
	frame.compress = byte_stream_get16(bs);
	frame.totfiles = byte_stream_get16(bs);
	frame.filesleft = byte_stream_get16(bs);
	frame.totparts = byte_stream_get16(bs);
	frame.partsleft = byte_stream_get16(bs);
	frame.totsize = byte_stream_get32(bs);
	frame.size = byte_stream_get32(bs);
	frame.modtime = byte_stream_get32(bs);
	frame.checksum = byte_stream_get32(bs);
	frame.rfrcsum = byte_stream_get32(bs);
	frame.rfsize = byte_stream_get32(bs);
	frame.cretime = byte_stream_get32(bs);
	frame.rfcsum = byte_stream_get32(bs);
	frame.nrecvd = byte_stream_get32(bs);
	frame.recvcsum = byte_stream_get32(bs);
	byte_stream_getrawbuf(bs, frame.idstring, 32);
	frame.flags = byte_stream_get8(bs);
	frame.lnameoffset = byte_stream_get8(bs);
	frame.lsizeoffset = byte_stream_get8(bs);
	byte_stream_getrawbuf(bs, frame.dummy, 69);
	byte_stream_getrawbuf(bs, frame.macfileinfo, 16);
	frame.nencode = byte_stream_get16(bs);
	frame.nlanguage = byte_stream_get16(bs);
	frame.name_length = bs->len - 186;
	frame.name = byte_stream_getraw(bs, frame.name_length);

	gaim_debug_info("oscar", "Incoming OFT frame from %s with "
			"type=0x%04x\n", conn->sn, frame.type);

	/* TODOFT: peer_oft_dirconvert_fromstupid(frame->name); */

	if (frame.type == PEER_TYPE_PROMPT)
		peer_oft_recv_frame_prompt(conn, &frame);
	else if (frame.type == PEER_TYPE_ACK)
		peer_oft_recv_frame_ack(conn, &frame);
	else if (frame.type == PEER_TYPE_DONE)
		peer_oft_recv_frame_done(conn, &frame);

	free(frame.name);
}

/*******************************************************************/
/* Begin GaimXfer callbacks for use when receiving a file          */
/*******************************************************************/

void
peer_oft_recvcb_init(GaimXfer *xfer)
{
	PeerConnection *conn;

	conn = xfer->data;
	conn->flags |= PEER_CONNECTION_FLAG_APPROVED;
	peer_connection_trynext(conn);
}

void
peer_oft_recvcb_end(GaimXfer *xfer)
{
	PeerConnection *conn;

	conn = xfer->data;

	/* Tell the other person that we've received everything */
	conn->fd = conn->xfer->fd;
	conn->xfer->fd = -1;
	peer_oft_send_done(conn);

	conn->disconnect_reason = OSCAR_DISCONNECT_DONE;
	conn->sending_data_timer = gaim_timeout_add(100,
			destroy_connection_when_done_sending_data, conn);
}

void
peer_oft_recvcb_ack_recv(GaimXfer *xfer, const guchar *buffer, size_t size)
{
	PeerConnection *conn;

	/* Update our rolling checksum.  Like Walmart, yo. */
	conn = xfer->data;
	conn->xferdata.recvcsum = peer_oft_checksum_chunk(buffer,
			size, conn->xferdata.recvcsum);
}

/*******************************************************************/
/* End GaimXfer callbacks for use when receiving a file            */
/*******************************************************************/

/*******************************************************************/
/* Begin GaimXfer callbacks for use when sending a file            */
/*******************************************************************/

void
peer_oft_sendcb_init(GaimXfer *xfer)
{
	PeerConnection *conn;

	conn = xfer->data;
	conn->flags |= PEER_CONNECTION_FLAG_APPROVED;

	/* Keep track of file transfer info */
	conn->xferdata.totfiles = 1;
	conn->xferdata.filesleft = 1;
	conn->xferdata.totparts = 1;
	conn->xferdata.partsleft = 1;
	conn->xferdata.totsize = gaim_xfer_get_size(xfer);
	conn->xferdata.size = gaim_xfer_get_size(xfer);
	conn->xferdata.checksum = 0xffff0000;
	conn->xferdata.rfrcsum = 0xffff0000;
	conn->xferdata.rfcsum = 0xffff0000;
	conn->xferdata.recvcsum = 0xffff0000;
	strncpy((gchar *)conn->xferdata.idstring, "OFT_Windows ICBMFT V1.1 32", 31);
	conn->xferdata.modtime = 0;
	conn->xferdata.cretime = 0;
	xfer->filename = g_path_get_basename(xfer->local_filename);
	conn->xferdata.name = (guchar *)g_strdup(xfer->filename);
	conn->xferdata.name_length = strlen(xfer->filename);

	/* Calculating the checksum can take a very long time for large files */
	gaim_debug_info("oscar","calculating file checksum\n");
	conn->xferdata.checksum = peer_oft_checksum_file(xfer->local_filename);
	gaim_debug_info("oscar","checksum calculated\n");

	/* Start the connection process */
	peer_connection_trynext(conn);
}

/*
 * AIM file transfers aren't really meant to be thought
 * of as a transferring just a single file.  The rendezvous
 * establishes a connection between two computers, and then
 * those computers can use the same connection for transferring
 * multiple files.  So we don't want the Gaim core up and closing
 * the socket all willy-nilly.  We want to do that in the oscar
 * prpl, whenever one side or the other says they're finished
 * using the connection.  There might be a better way to intercept
 * the socket from the core...
 */
void
peer_oft_sendcb_ack(GaimXfer *xfer, const guchar *buffer, size_t size)
{
	PeerConnection *conn;

	conn = xfer->data;

	/*
	 * If we're done sending, intercept the socket from the core ft code
	 * and wait for the other guy to send the "done" OFT packet.
	 */
	if (gaim_xfer_get_bytes_remaining(xfer) <= 0)
	{
		gaim_input_remove(xfer->watcher);
		conn->fd = xfer->fd;
		xfer->fd = -1;
		conn->watcher_incoming = gaim_input_add(conn->fd,
				GAIM_INPUT_READ, peer_connection_recv_cb, conn);
	}
}

/*******************************************************************/
/* End GaimXfer callbacks for use when sending a file              */
/*******************************************************************/

/*******************************************************************/
/* Begin GaimXfer callbacks for use when sending and receiving     */
/*******************************************************************/

void
peer_oft_cb_generic_cancel(GaimXfer *xfer)
{
	PeerConnection *conn;

	conn = xfer->data;

	if (conn == NULL)
		return;

	peer_connection_destroy(conn, OSCAR_DISCONNECT_LOCAL_CLOSED, NULL);
}

/*******************************************************************/
/* End GaimXfer callbacks for use when sending and receiving       */
/*******************************************************************/

#if 0
/*
 * This little area in oscar.c is the nexus of file transfer code,
 * so I wrote a little explanation of what happens.  I am such a
 * ninja.
 *
 * The series of events for a file send is:
 *  -Create xfer and call gaim_xfer_request (this happens in oscar_ask_sendfile)
 *  -User chooses a file and oscar_xfer_init is called.  It establishes a
 *   listening socket, then asks the remote user to connect to us (and
 *   gives them the file name, port, IP, etc.)
 *  -They connect to us and we send them an PEER_TYPE_PROMPT (this happens
 *   in peer_oft_recv_frame_established)
 *  -They send us an PEER_TYPE_ACK and then we start sending data
 *  -When we finish, they send us an PEER_TYPE_DONE and they close the
 *   connection.
 *  -We get drunk because file transfer kicks ass.
 *
 * The series of events for a file receive is:
 *  -Create xfer and call gaim_xfer request (this happens in incomingim_chan2)
 *  -Gaim user selects file to name and location to save file to and
 *   oscar_xfer_init is called
 *  -It connects to the remote user using the IP they gave us earlier
 *  -After connecting, they send us an PEER_TYPE_PROMPT.  In reply, we send
 *   them an PEER_TYPE_ACK.
 *  -They begin to send us lots of raw data.
 *  -When they finish sending data we send an PEER_TYPE_DONE and then close
 *   the connection.
 *
 * Update August 2005:
 * The series of events for transfers has been seriously complicated by the addition
 * of transfer redirects and proxied connections. I could throw a whole lot of words
 * at trying to explain things here, but it probably wouldn't do much good. To get
 * a better idea of what happens, take a look at the diagrams and documentation
 * from my Summer of Code project. -- Jonathan Clark
 */

/**
 * Convert the directory separator from / (0x2f) to ^A (0x01)
 *
 * @param name The filename to convert.
 */
static void
peer_oft_dirconvert_tostupid(char *name)
{
	while (name[0]) {
		if (name[0] == 0x01)
			name[0] = G_DIR_SEPARATOR;
		name++;
	}
}

/**
 * Convert the directory separator from ^A (0x01) to / (0x2f)
 *
 * @param name The filename to convert.
 */
static void
peer_oft_dirconvert_fromstupid(char *name)
{
	while (name[0]) {
		if (name[0] == G_DIR_SEPARATOR)
			name[0] = 0x01;
		name++;
	}
}
#endif
