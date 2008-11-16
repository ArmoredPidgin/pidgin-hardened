/*
 * Purple's oscar protocol plugin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

/*
 * Family 0x0002 - Locate.
 *
 * The functions here are responsible for requesting and parsing information-
 * gathering SNACs.  Or something like that.  This family contains the SNACs
 * for getting and setting info, away messages, directory profile thingy, etc.
 */

#include "oscar.h"
#ifdef _WIN32
#include "win32dep.h"
#endif

/* Define to log unknown TLVs */
/* #define LOG_UNKNOWN_TLV */

/*
 * Capability blocks.
 *
 * These are CLSIDs. They should actually be of the form:
 *
 * {0x0946134b, 0x4c7f, 0x11d1,
 *  {0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}}},
 *
 * But, eh.
 */
static const struct {
	guint32 flag;
	guint8 data[16];
} aim_caps[] = {

	/*
	 * These are in ascending numerical order.
	 */

	/* Client understands short caps, a UUID of the form
	 * 0946XXYY-4C7F-11D1-8222-444553540000 where XXYY is the short cap. */
	{OSCAR_CAPABILITY_SHORTCAPS,
	 {0x09, 0x46, 0x00, 0x00, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_SECUREIM,
	 {0x09, 0x46, 0x00, 0x01, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* OSCAR_CAPABILITY_XHTML_IM */
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0x00, 0x02, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_VIDEO,
	 {0x09, 0x46, 0x01, 0x00, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* "Live Video" (SIP/RTC Video) support in Windows AIM 5.5.3501 and newer */
	{OSCAR_CAPABILITY_LIVEVIDEO,
	 {0x09, 0x46, 0x01, 0x01, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* "Camera" support in Windows AIM 5.5.3501 and newer */
	{OSCAR_CAPABILITY_CAMERA,
	 {0x09, 0x46, 0x01, 0x02, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* "Microphone" support in Windows AIM 5.5.3501 and newer */
	/* OSCAR_CAPABILITY_MICROPHONE */
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0x01, 0x03, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* Supports RTC Audio */
	/* OSCAR_CAPABILITY_RTCAUDIO */
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0x01, 0x04, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* In iChatAV (version numbers...?) */
	{OSCAR_CAPABILITY_ICHATAV,
	 {0x09, 0x46, 0x01, 0x05, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x45, 0x53, 0x54, 0x00}},

	/* Supports "new status message features" (Who advertises this one?) */
	/* OSCAR_CAPABILITY_HOST_STATUS_TEXT_AWARE */ 
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0x01, 0x0a, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* Supports "see as I type" (Who advertises this one?) */
	/* OSCAR_CAPABILITY_SEE_AS_I_TYPE */
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0x01, 0x0b, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* Client only asserts caps for services in which it is participating */
	/* OSCAR_CAPABILITY_SMARTCAPS */ 
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0x01, 0xff, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_HIPTOP,
	 {0x09, 0x46, 0x13, 0x23, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_TALK,
	 {0x09, 0x46, 0x13, 0x41, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_SENDFILE,
	 {0x09, 0x46, 0x13, 0x43, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_ICQ_DIRECT,
	 {0x09, 0x46, 0x13, 0x44, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_DIRECTIM,
	 {0x09, 0x46, 0x13, 0x45, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_BUDDYICON,
	 {0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_ADDINS,
	 {0x09, 0x46, 0x13, 0x47, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_GETFILE,
	 {0x09, 0x46, 0x13, 0x48, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_ICQSERVERRELAY,
	 {0x09, 0x46, 0x13, 0x49, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/*
	 * Indeed, there are two of these.  The former appears to be correct,
	 * but in some versions of winaim, the second one is set.  Either they
	 * forgot to fix endianness, or they made a typo. It really doesn't
	 * matter which.
	 */
	{OSCAR_CAPABILITY_GAMES,
	 {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},
	{OSCAR_CAPABILITY_GAMES2,
	 {0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1,
	  0x22, 0x82, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* New format of caps (xtraz icons) */
	{OSCAR_CAPABILITY_NEWCAPS,
	 {0x09, 0x46, 0x00, 0x00, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* Support xtraz statuses */
	{OSCAR_CAPABILITY_XTRAZ,
	 {0x1a, 0x09, 0x3c, 0x6c, 0xd7, 0xFD, 0x4e, 0xc5, 
	  0x9d, 0x51, 0xa6, 0x47, 0x4e, 0x34, 0xf5, 0xa0}},

	{OSCAR_CAPABILITY_SENDBUDDYLIST,
	 {0x09, 0x46, 0x13, 0x4b, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/*
	 * Setting this lets AIM users receive messages from ICQ users, and ICQ
	 * users receive messages from AIM users.  It also lets ICQ users show
	 * up in buddy lists for AIM users, and AIM users show up in buddy lists
	 * for ICQ users.  And ICQ privacy/invisibility acts like AIM privacy,
	 * in that if you add a user to your deny list, you will not be able to
	 * see them as online (previous you could still see them, but they
	 * couldn't see you.
	 */
	{OSCAR_CAPABILITY_INTEROPERATE,
	 {0x09, 0x46, 0x13, 0x4d, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_UNICODE,
	 {0x09, 0x46, 0x13, 0x4e, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0xf0, 0x03, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_ICHAT_SCREENSHARE,
	 {0x09, 0x46, 0xf0, 0x04, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x09, 0x46, 0xf0, 0x05, 0x4c, 0x7f, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	{OSCAR_CAPABILITY_UNICODEOLD,
	 {0x2e, 0x7a, 0x64, 0x75, 0xfa, 0xdf, 0x4d, 0xc8,
	  0x88, 0x6f, 0xea, 0x35, 0x95, 0xfd, 0xb6, 0xdf}},

	/*
	{OSCAR_CAPABILITY_ICQ2GO,
	 {0x56, 0x3f, 0xc8, 0x09, 0x0b, 0x6f, 0x41, 0xbd,
	  0x9f, 0x79, 0x42, 0x26, 0x09, 0xdf, 0xa2, 0xf3}},
	*/

	/*
	 * Chat is oddball.
	 */
	{OSCAR_CAPABILITY_CHAT,
	 {0x74, 0x8f, 0x24, 0x20, 0x62, 0x87, 0x11, 0xd1,
	  0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}},

	/* This is added by the servers and it only shows up for ourselves... */
	{OSCAR_CAPABILITY_GENERICUNKNOWN,
	 {0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34,
	  0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x09}},

	{OSCAR_CAPABILITY_ICQRTF,
	 {0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34,
	  0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x92}},

	{OSCAR_CAPABILITY_APINFO,
	 {0xaa, 0x4a, 0x32, 0xb5, 0xf8, 0x84, 0x48, 0xc6,
	  0xa3, 0xd7, 0x8c, 0x50, 0x97, 0x19, 0xfd, 0x5b}},

	{OSCAR_CAPABILITY_TRILLIANCRYPT,
	 {0xf2, 0xe7, 0xc7, 0xf4, 0xfe, 0xad, 0x4d, 0xfb,
	  0xb2, 0x35, 0x36, 0x79, 0x8b, 0xdf, 0x00, 0x00}},

	{OSCAR_CAPABILITY_EMPTY,
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},

	{OSCAR_CAPABILITY_LAST,
	 {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

#define AIM_CUSTOM_ICONS_COUNT 35

static const struct {
	char *filename;
	char *descriptivename;
	guint8 data[16];
} aim_custom_icons[AIM_CUSTOM_ICONS_COUNT] = {
	/* empty X-Status for the case when customicon == 0 */
	{NULL, NULL,
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},

	{"thinking", N_("Thinking"),
	 {0x3f, 0xb0, 0xbd, 0x36, 0xaf, 0x3b, 0x4a, 0x60,
	  0x9e, 0xef, 0xcf, 0x19, 0x0f, 0x6a, 0x5a, 0x7f}},

	{"busy", N_("Busy"),
	 {0x48, 0x8e, 0x14, 0x89, 0x8a, 0xca, 0x4a, 0x08,
	  0x82, 0xaa, 0x77, 0xce, 0x7a, 0x16, 0x52, 0x08}},

	{"xstatus_shopping", N_("Shopping"),
	 {0x63, 0x62, 0x73, 0x37, 0xa0, 0x3f, 0x49, 0xff,
	  0x80, 0xe5, 0xf7, 0x09, 0xcd, 0xe0, 0xa4, 0xee}},

	/* This was in the original patch, but isn't what the official client
	 * (ICQ 6) sets when you choose its typewriter icon. */
	{"xstatus_typing", NULL,
	 {0x63, 0x4f, 0x6b, 0xd8 ,0xad, 0xd2, 0x4a, 0xa1,
	  0xaa, 0xb9, 0x11, 0x5b, 0xc2, 0x6d, 0x05, 0xa1}},

	{"question", N_("Questioning"),
	 {0x63, 0x14, 0x36, 0xff, 0x3f, 0x8a, 0x40, 0xd0,
	  0xa5, 0xcb, 0x7b, 0x66, 0xe0, 0x51, 0xb3, 0x64}},

	{"angry", N_("Angry"),
	 {0x01, 0xd8, 0xd7, 0xee, 0xac, 0x3b, 0x49, 0x2a,
	  0xa5, 0x8d, 0xd3, 0xd8, 0x77, 0xe6, 0x6b, 0x92}},

	{"plate", N_("Eating"),
	 {0xf8, 0xe8, 0xd7, 0xb2, 0x82, 0xc4, 0x41, 0x42,
	  0x90, 0xf8, 0x10, 0xc6, 0xce, 0x0a, 0x89, 0xa6}},

	{"xstatus_cinema", N_("Watching a movie"),
	 {0x10, 0x7a, 0x9a, 0x18, 0x12, 0x32, 0x4d, 0xa4,
	  0xb6, 0xcd, 0x08, 0x79, 0xdb, 0x78, 0x0f, 0x09}},

	{"sick", N_("Sick"),
	 {0x1f, 0x7a, 0x40, 0x71, 0xbf, 0x3b, 0x4e, 0x60,
	  0xbc, 0x32, 0x4c, 0x57, 0x87, 0xb0, 0x4c, 0xf1}},

	{"xstatus_typing", N_("Typing"),
	 {0x2c, 0xe0, 0xe4, 0xe5, 0x7c, 0x64, 0x43, 0x70,
	  0x9c, 0x3a, 0x7a, 0x1c, 0xe8, 0x78, 0xa7, 0xdc}},

	{"xstatus_suit", N_("At the office"),
	 {0xb7, 0x08, 0x67, 0xf5, 0x38, 0x25, 0x43, 0x27,
	  0xa1, 0xff, 0xcf, 0x4c, 0xc1, 0x93, 0x97, 0x97}},

	{"xstatus_bathing", N_("Taking a bath"),
	 {0x5a, 0x58, 0x1e, 0xa1, 0xe5, 0x80, 0x43, 0x0c,
	  0xa0, 0x6f, 0x61, 0x22, 0x98, 0xb7, 0xe4, 0xc7}},

	{"tv", N_("Watching TV"),
	 {0x80, 0x53, 0x7d, 0xe2, 0xa4, 0x67, 0x4a, 0x76,
	  0xb3, 0x54, 0x6d, 0xfd, 0x07, 0x5f, 0x5e, 0xc6}},

	{"smile-big", N_("Having fun"),
	 {0x6f, 0x49, 0x30, 0x98, 0x4f, 0x7c, 0x4a, 0xff,
	  0xa2, 0x76, 0x34, 0xa0, 0x3b, 0xce, 0xae, 0xa7}},

	{"sleepy", N_("Sleeping"),
	 {0x78, 0x5e, 0x8c, 0x48, 0x40, 0xd3, 0x4c, 0x65,
	  0x88, 0x6f, 0x04, 0xcf, 0x3f, 0x3f, 0x43, 0xdf}},

	{"hiptop", N_("Using a PDA"),
	 {0x10, 0x11, 0x17, 0xc9, 0xa3, 0xb0, 0x40, 0xf9,
	  0x81, 0xac, 0x49, 0xe1, 0x59, 0xfb, 0xd5, 0xd4}},

	{"love", N_("In love"),
	 {0xdd, 0xcf, 0x0e, 0xa9, 0x71, 0x95, 0x40, 0x48,
	  0xa9, 0xc6, 0x41, 0x32, 0x06, 0xd6, 0xf2, 0x80}},

	/* Sleepy / Tired */
	{"yawn", N_("Sleepy"),
	 {0x83, 0xc9, 0xb7, 0x8e, 0x77, 0xe7, 0x43, 0x78,
	  0xb2, 0xc5, 0xfb, 0x6c, 0xfc, 0xc3, 0x5b, 0xec}},

	{"meeting", N_("Meeting friends"),
	 {0xf1, 0x8a, 0xb5, 0x2e, 0xdc, 0x57, 0x49, 0x1d,
	  0x99, 0xdc, 0x64, 0x44, 0x50, 0x24, 0x57, 0xaf}},

	{"phone", N_("On the phone"),
	 {0x12, 0x92, 0xe5, 0x50, 0x1b, 0x64, 0x4f, 0x66,
	  0xb2, 0x06, 0xb2, 0x9a, 0xf3, 0x78, 0xe4, 0x8d}},

	{"xstatus_surfing", N_("Surfing"),
	 {0xa6, 0xed, 0x55, 0x7e, 0x6b, 0xf7, 0x44, 0xd4,
	  0xa5, 0xd4, 0xd2, 0xe7, 0xd9, 0x5c, 0xe8, 0x1f}},

	/* "I am mobile." / "John is mobile." */
	{"mobile", N_("Mobile"),
	 {0x16, 0x0c, 0x60, 0xbb, 0xdd, 0x44, 0x43, 0xf3,
	  0x91, 0x40, 0x05, 0x0f, 0x00, 0xe6, 0xc0, 0x09}},

	{"search", N_("Searching the web"),
	 {0xd4, 0xe2, 0xb0, 0xba, 0x33, 0x4e, 0x4f, 0xa5,
	  0x98, 0xd0, 0x11, 0x7d, 0xbf, 0x4d, 0x3c, 0xc8}},

	{"party", N_("At a party"),
	 {0xe6, 0x01, 0xe4, 0x1c, 0x33, 0x73, 0x4b, 0xd1,
	  0xbc, 0x06, 0x81, 0x1d, 0x6c, 0x32, 0x3d, 0x81}},

	{"coffee", N_("Having Coffee"),
	 {0x1b, 0x78, 0xae, 0x31, 0xfa, 0x0b, 0x4d, 0x38,
	  0x93, 0xd1, 0x99, 0x7e, 0xee, 0xaf, 0xb2, 0x18}},

	/* Playing video games */
	{"console", N_("Gaming"),
	 {0xd4, 0xa6, 0x11, 0xd0, 0x8f, 0x01, 0x4e, 0xc0,
	  0x92, 0x23, 0xc5, 0xb6, 0xbe, 0xc6, 0xcc, 0xf0}},

	{"xstatus_internet", N_("Browsing the web"),
	 {0x12, 0xd0, 0x7e, 0x3e, 0xf8, 0x85, 0x48, 0x9e,
	  0x8e, 0x97, 0xa7, 0x2a, 0x65, 0x51, 0xe5, 0x8d}},

	{"cigarette", N_("Smoking"),
	 {0x64, 0x43, 0xc6, 0xaf, 0x22, 0x60, 0x45, 0x17,
	  0xb5, 0x8c, 0xd7, 0xdf, 0x8e, 0x29, 0x03, 0x52}},

	{"xstatus_writing", N_("Writing"),
	 {0x00, 0x72, 0xd9, 0x08, 0x4a, 0xd1, 0x43, 0xdd,
	  0x91, 0x99, 0x6f, 0x02, 0x69, 0x66, 0x02, 0x6f}},

	/* Drinking [Alcohol] */
	{"beer", N_("Drinking"),
	 {0x8c, 0x50, 0xdb, 0xae, 0x81, 0xed, 0x47, 0x86,
	  0xac, 0xca, 0x16, 0xcc, 0x32, 0x13, 0xc7, 0xb7}},

	{"music", N_("Listening to music"),
	 {0x61, 0xbe, 0xe0, 0xdd, 0x8b, 0xdd, 0x47, 0x5d,
	  0x8d, 0xee, 0x5f, 0x4b, 0xaa, 0xcf, 0x19, 0xa7}},

	{"xstatus_studying", N_("Studying"),
	 {0x60, 0x9d, 0x52, 0xf8, 0xa2, 0x9a, 0x49, 0xa6,
	  0xb2, 0xa0, 0x25, 0x24, 0xc5, 0xe9, 0xd2, 0x60}},

	{"xstatus_working", N_("Working"),
	 {0xba, 0x74, 0xdb, 0x3e, 0x9e, 0x24, 0x43, 0x4b,
	  0x87, 0xb6, 0x2f, 0x6b, 0x8d, 0xfe, 0xe5, 0x0f}},

	{"xstatus_restroom", N_("In the restroom"),
	 {0x16, 0xf5, 0xb7, 0x6f, 0xa9, 0xd2, 0x40, 0x35,
	  0x8c, 0xc5, 0xc0, 0x84, 0x70, 0x3c, 0x98, 0xfa}}
};

/*
 * Add the userinfo to our linked list.  If we already have userinfo
 * for this buddy, then just overwrite parts of the old data.
 *
 * @param userinfo Contains the new information for the buddy.
 */
static void
aim_locate_adduserinfo(OscarData *od, aim_userinfo_t *userinfo)
{
	aim_userinfo_t *cur;
	FlapConnection *conn;
	aim_rxcallback_t userfunc;

	cur = aim_locate_finduserinfo(od, userinfo->sn);

	if (cur == NULL) {
		cur = (aim_userinfo_t *)g_new0(aim_userinfo_t, 1);
		cur->sn = g_strdup(userinfo->sn);
		cur->next = od->locate.userinfo;
		od->locate.userinfo = cur;
	}

	cur->warnlevel = userinfo->warnlevel;
	cur->idletime = userinfo->idletime;
	if (userinfo->flags != 0)
		cur->flags = userinfo->flags;
	if (userinfo->createtime != 0)
		cur->createtime = userinfo->createtime;
	if (userinfo->membersince != 0)
		cur->membersince = userinfo->membersince;
	if (userinfo->onlinesince != 0)
		cur->onlinesince = userinfo->onlinesince;
	if (userinfo->sessionlen != 0)
		cur->sessionlen = userinfo->sessionlen;
	if (userinfo->capabilities != 0)
		cur->capabilities = userinfo->capabilities;
	if (userinfo->customicon != 0)
		cur->customicon = userinfo->customicon;

	cur->present |= userinfo->present;

	if (userinfo->iconcsumlen > 0) {
		g_free(cur->iconcsum);
		cur->iconcsum = (guint8 *)g_malloc(userinfo->iconcsumlen);
		memcpy(cur->iconcsum, userinfo->iconcsum, userinfo->iconcsumlen);
		cur->iconcsumlen = userinfo->iconcsumlen;
	}

	if (userinfo->info != NULL) {
		g_free(cur->info);
		g_free(cur->info_encoding);
		if (userinfo->info_len > 0) {
			cur->info = (char *)g_malloc(userinfo->info_len);
			memcpy(cur->info, userinfo->info, userinfo->info_len);
		} else
			cur->info = NULL;
		cur->info_encoding = g_strdup(userinfo->info_encoding);
		cur->info_len = userinfo->info_len;
	}

	if (userinfo->status != NULL) {
		g_free(cur->status);
		g_free(cur->status_encoding);
		if (userinfo->status_len > 0) {
			cur->status = (char *)g_malloc(userinfo->status_len);
			memcpy(cur->status, userinfo->status, userinfo->status_len);
		} else
			cur->status = NULL;
		if (userinfo->status_encoding != NULL)
			cur->status_encoding = g_strdup(userinfo->status_encoding);
		else
			cur->status_encoding = NULL;
		cur->status_len = userinfo->status_len;
	}

	if (userinfo->itmsurl != NULL) {
		g_free(cur->itmsurl);
		g_free(cur->itmsurl_encoding);
		if (userinfo->itmsurl_len > 0) {
			cur->itmsurl = (char *)g_malloc(userinfo->itmsurl_len);
			memcpy(cur->itmsurl, userinfo->itmsurl, userinfo->itmsurl_len);
		} else
			cur->itmsurl = NULL;
		if (userinfo->itmsurl_encoding != NULL)
			cur->itmsurl_encoding = g_strdup(userinfo->itmsurl_encoding);
		else
			cur->itmsurl_encoding = NULL;
		cur->itmsurl_len = userinfo->itmsurl_len;
	}

	if (userinfo->away != NULL) {
		g_free(cur->away);
		g_free(cur->away_encoding);
		if (userinfo->away_len > 0) {
			cur->away = (char *)g_malloc(userinfo->away_len);
			memcpy(cur->away, userinfo->away, userinfo->away_len);
		} else
			cur->away = NULL;
		cur->away_encoding = g_strdup(userinfo->away_encoding);
		cur->away_len = userinfo->away_len;

	} else {
		/*
		 * We don't have an away message specified in this user_info
		 * block, so clear any cached away message now.
		 */
		if (cur->away) {
			g_free(cur->away);
			cur->away = NULL;
		}
		if (cur->away_encoding) {
			g_free(cur->away_encoding);
			cur->away_encoding = NULL;
		}
		cur->away_len = 0;
	}

	/*
	 * This callback can be used by a client if they want to know whenever
	 * info for a buddy is updated.  For example, if a client shows away
	 * messages in its buddy list, then it would need to know if a user's
	 * away message changes.
	 */
	conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE);
	if ((userfunc = aim_callhandler(od, SNAC_FAMILY_LOCATE, SNAC_SUBTYPE_LOCATE_GOTINFOBLOCK)))
		userfunc(od, conn, NULL, cur);
}

/**
 * Remove this screen name from our queue.  If this info was requested
 * by our info request queue, then pop the next element off of the queue.
 *
 * @param od The aim session.
 * @param sn Screen name of the info we just received.
 * @return True if the request was explicit (client requested the info),
 *         false if the request was implicit (libfaim request the info).
 */
static int
aim_locate_gotuserinfo(OscarData *od, FlapConnection *conn, const char *sn)
{
	struct userinfo_node *cur, *del;
	int was_explicit = TRUE;

	while ((od->locate.requested != NULL) && (aim_sncmp(sn, od->locate.requested->sn) == 0)) {
		del = od->locate.requested;
		od->locate.requested = del->next;
		was_explicit = FALSE;
		g_free(del->sn);
		g_free(del);
	}

	cur = od->locate.requested;
	while ((cur != NULL) && (cur->next != NULL)) {
		if (aim_sncmp(sn, cur->next->sn) == 0) {
			del = cur->next;
			cur->next = del->next;
			was_explicit = FALSE;
			g_free(del->sn);
			g_free(del);
		} else
			cur = cur->next;
	}

	return was_explicit;
}

void
aim_locate_autofetch_away_message(OscarData *od, const char *sn)
{
	struct userinfo_node *cur;

	/* Make sure we haven't already made an info request for this buddy */
	for (cur = od->locate.requested; cur != NULL; cur = cur->next)
		if (aim_sncmp(sn, cur->sn) == 0)
			return;

	/* Add a new node to our request queue */
	cur = (struct userinfo_node *)g_malloc(sizeof(struct userinfo_node));
	cur->sn = g_strdup(sn);
	cur->next = od->locate.requested;
	od->locate.requested = cur;

	aim_locate_getinfoshort(od, cur->sn, 0x00000002);
}

aim_userinfo_t *aim_locate_finduserinfo(OscarData *od, const char *sn) {
	aim_userinfo_t *cur = NULL;

	if (sn == NULL)
		return NULL;

	cur = od->locate.userinfo;

	while (cur != NULL) {
		if (aim_sncmp(cur->sn, sn) == 0)
			return cur;
		cur = cur->next;
	}

	return NULL;
}

guint32
aim_locate_getcaps(OscarData *od, ByteStream *bs, int len)
{
	guint32 flags = 0;
	int offset;

	for (offset = 0; byte_stream_empty(bs) && (offset < len); offset += 0x10) {
		guint8 *cap;
		int i, identified;

		cap = byte_stream_getraw(bs, 0x10);

		for (i = 0, identified = 0; !(aim_caps[i].flag & OSCAR_CAPABILITY_LAST); i++) {
			if (memcmp(&aim_caps[i].data, cap, 0x10) == 0) {
				flags |= aim_caps[i].flag;
				identified++;
				break; /* should only match once... */
			}
		}

		if (!identified)
			purple_debug_misc("oscar", "unknown capability: {%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
					cap[0], cap[1], cap[2], cap[3],
					cap[4], cap[5],
					cap[6], cap[7],
					cap[8], cap[9],
					cap[10], cap[11], cap[12], cap[13],
					cap[14], cap[15]);
		g_free(cap);
	}

	return flags;
}

gint32
aim_get_custom_icon(OscarData *od, ByteStream *bs, int len)
{
	int offset;
	gint32 result = -1;

	for (offset = 0; byte_stream_empty(bs) && (offset < len); offset += 0x10) {
		/* check wheather this capability is a custom user icon */
		guint8 *cap;
		int i;

		cap = byte_stream_getraw(bs, 0x10);

		for (i = 1; i < AIM_CUSTOM_ICONS_COUNT; i++) {
			if (memcmp(&aim_custom_icons[i].data, cap, 0x10) == 0) {
				purple_debug_misc("oscar", "Custom status icon: %s\n", aim_custom_icons[i].descriptivename);		
				result = i;
				break; /* should only match once... */
			}
		}
		g_free(cap);
	}

	return result;
}

guint32
aim_locate_getcaps_short(OscarData *od, ByteStream *bs, int len)
{
	guint32 flags = 0;
	int offset;

	for (offset = 0; byte_stream_empty(bs) && (offset < len); offset += 0x02) {
		guint8 *cap;
		int i, identified;

		cap = byte_stream_getraw(bs, 0x02);

		for (i = 0, identified = 0; !(aim_caps[i].flag & OSCAR_CAPABILITY_LAST); i++) {
			if (memcmp(&aim_caps[i].data[2], cap, 0x02) == 0) {
				flags |= aim_caps[i].flag;
				identified++;
				break; /* should only match once... */
			}
		}

		if (!identified)
			purple_debug_misc("oscar", "unknown short capability: {%02x%02x}\n", cap[0], cap[1]);

		g_free(cap);
	}

	return flags;
}

int
byte_stream_putcaps(ByteStream *bs, guint32 caps)
{
	int i;

	if (!bs)
		return -EINVAL;

	for (i = 0; byte_stream_empty(bs); i++) {

		if (aim_caps[i].flag == OSCAR_CAPABILITY_LAST)
			break;

		if (caps & aim_caps[i].flag)
			byte_stream_putraw(bs, aim_caps[i].data, 0x10);

	}

	return 0;
}

#ifdef LOG_UNKNOWN_TLV
static void
dumptlv(OscarData *od, guint16 type, ByteStream *bs, guint8 len)
{
	int i;

	if (!od || !bs || !len)
		return;

	purple_debug_misc("oscar", "userinfo:   type  =0x%04x\n", type);
	purple_debug_misc("oscar", "userinfo:   length=0x%04x\n", len);
	purple_debug_misc("oscar", "userinfo:   value:\n");

	for (i = 0; i < len; i++) {
		if ((i % 8) == 0)
			purple_debug_misc("oscar", "\nuserinfo:        ");
		purple_debug_misc("oscar", "0x%2x ", byte_stream_get8(bs));
	}

	purple_debug_misc("oscar", "\n");

	return;
}
#endif

void
aim_info_free(aim_userinfo_t *info)
{
	g_free(info->sn);
	g_free(info->iconcsum);
	g_free(info->info);
	g_free(info->info_encoding);
	g_free(info->status);
	g_free(info->status_encoding);
	g_free(info->itmsurl);
	g_free(info->itmsurl_encoding);
	g_free(info->away);
	g_free(info->away_encoding);
}

#define ICQMOODS_COUNT 23

static const struct {
	char *mood;
	gint32 icon_num;
} icqmoods[ICQMOODS_COUNT] = {
	{"icqmood0", 3},
	{"icqmood1", 12},
	{"icqmood2", 18},
	{"icqmood3", 24},
	{"icqmood4", 30},
	{"icqmood5", 1},
	{"icqmood6", 7},
	{"icqmood7", 13},
	{"icqmood8", 19},
	{"icqmood9", 25},
	{"icqmood10", 31},
	{"icqmood11", 11},
	{"icqmood12", 8},
	{"icqmood13", 14},
	{"icqmood14", 20},
	{"icqmood15", 26},
	{"icqmood16", 32},
	{"icqmood17", 9},
	{"icqmood18", 15},
	{"icqmood19", 21},
	{"icqmood20", 27},
	{"icqmood21", 33},
	{"icqmood22", 10},
	{"icqmood23", 6},
};

/*
 * AIM is fairly regular about providing user info.  This is a generic
 * routine to extract it in its standard form.
 */
int
aim_info_extract(OscarData *od, ByteStream *bs, aim_userinfo_t *outinfo)
{
	int curtlv, tlvcnt;
	guint8 snlen;

	if (!bs || !outinfo)
		return -EINVAL;

	/* Clear out old data first */
	memset(outinfo, 0x00, sizeof(aim_userinfo_t));

	/*
	 * Screen name.  Stored as an unterminated string prepended with a
	 * byte containing its length.
	 */
	snlen = byte_stream_get8(bs);
	outinfo->sn = byte_stream_getstr(bs, snlen);

	/*
	 * Warning Level.  Stored as an unsigned short.
	 */
	outinfo->warnlevel = byte_stream_get16(bs);

	/*
	 * TLV Count. Unsigned short representing the number of
	 * Type-Length-Value triples that follow.
	 */
	tlvcnt = byte_stream_get16(bs);

	/*
	 * Parse out the Type-Length-Value triples as they're found.
	 */
	for (curtlv = 0; curtlv < tlvcnt; curtlv++) {
		guint16 type, length;
		int endpos;
		int curpos;

		type = byte_stream_get16(bs);
		length = byte_stream_get16(bs);
		curpos = byte_stream_curpos(bs);
		endpos = curpos + MIN(length, byte_stream_empty(bs));

		if (type == 0x0001) {
			/*
			 * User flags
			 *
			 * Specified as any of the following ORed together:
			 *      0x0001  Trial (user less than 60days)
			 *      0x0002  Unknown bit 2
			 *      0x0004  AOL Main Service user
			 *      0x0008  Unknown bit 4
			 *      0x0010  Free (AIM) user
			 *      0x0020  Away
			 *      0x0400  ActiveBuddy
			 */
			outinfo->flags = byte_stream_get16(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_FLAGS;

		} else if (type == 0x0002) {
			/*
			 * Account creation time
			 *
			 * The time/date that the user originally registered for
			 * the service, stored in time_t format.
			 *
			 * I'm not sure how this differs from type 5 ("member
			 * since").
			 *
			 * Note: This is the field formerly known as "member
			 * since".  All these years and I finally found out
			 * that I got the name wrong.
			 */
			outinfo->createtime = byte_stream_get32(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_CREATETIME;

		} else if (type == 0x0003) {
			/*
			 * On-Since date
			 *
			 * The time/date that the user started their current
			 * session, stored in time_t format.
			 */
			outinfo->onlinesince = byte_stream_get32(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_ONLINESINCE;

		} else if (type == 0x0004) {
			/*
			 * Idle time
			 *
			 * Number of minutes since the user actively used the
			 * service.
			 *
			 * Note that the client tells the server when to start
			 * counting idle times, so this may or may not be
			 * related to reality.
			 */
			outinfo->idletime = byte_stream_get16(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_IDLE;

		} else if (type == 0x0005) {
			/*
			 * Member since date
			 *
			 * The time/date that the user originally registered for
			 * the service, stored in time_t format.
			 *
			 * This is sometimes sent instead of type 2 ("account
			 * creation time"), particularly in the self-info.
			 * And particularly for ICQ?
			 */
			outinfo->membersince = byte_stream_get32(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_MEMBERSINCE;

		} else if (type == 0x0006) {
			/*
			 * ICQ Online Status
			 *
			 * ICQ's Away/DND/etc "enriched" status. Some decoding
			 * of values done by Scott <darkagl@pcnet.com>
			 */
			byte_stream_get16(bs);
			outinfo->icqinfo.status = byte_stream_get16(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_ICQEXTSTATUS;

		} else if (type == 0x0008) {
			/*
			 * Client type, or some such.
			 */

		} else if (type == 0x000a) {
			/*
			 * ICQ User IP Address
			 *
			 * Ahh, the joy of ICQ security.
			 */
			outinfo->icqinfo.ipaddr = byte_stream_get32(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_ICQIPADDR;

		} else if (type == 0x000c) {
			/*
			 * Random crap containing the IP address,
			 * apparently a port number, and some Other Stuff.
			 *
			 * Format is:
			 * 4 bytes - Our IP address, 0xc0 a8 01 2b for 192.168.1.43
			 */
			byte_stream_getrawbuf(bs, outinfo->icqinfo.crap, 0x25);
			outinfo->present |= AIM_USERINFO_PRESENT_ICQDATA;

		} else if (type == 0x000d) {
			/*
			 * OSCAR Capability information
			 */
			outinfo->capabilities |= aim_locate_getcaps(od, bs, length);
			outinfo->present |= AIM_USERINFO_PRESENT_CAPABILITIES;
			byte_stream_setpos(bs, curpos);
			outinfo->customicon = aim_get_custom_icon(od, bs, length);

		} else if (type == 0x000e) {
			/*
			 * AOL capability information
			 */

		} else if ((type == 0x000f) || (type == 0x0010)) {
			/*
			 * Type = 0x000f: Session Length. (AIM)
			 * Type = 0x0010: Session Length. (AOL)
			 *
			 * The duration, in seconds, of the user's current
			 * session.
			 *
			 * Which TLV type this comes in depends on the
			 * service the user is using (AIM or AOL).
			 */
			outinfo->sessionlen = byte_stream_get32(bs);
			outinfo->present |= AIM_USERINFO_PRESENT_SESSIONLEN;

		} else if (type == 0x0019) {
			/*
			 * OSCAR short capability information.  A shortened
			 * form of the normal capabilities.
			 */
			outinfo->capabilities |= aim_locate_getcaps_short(od, bs, length);
			outinfo->present |= AIM_USERINFO_PRESENT_CAPABILITIES;

		} else if (type == 0x001a) {
			/*
			 * Type = 0x001a
			 *
			 * AOL short capability information.  A shortened
			 * form of the normal capabilities.
			 */

		} else if (type == 0x001b) {
			/*
			 * Encryption certification MD5 checksum.
			 */

		} else if (type == 0x001d) {
			/*
			 * Buddy icon information and status/available messages.
			 *
			 * This almost seems like the AIM protocol guys gave
			 * the iChat guys a Type, and the iChat guys tried to
			 * cram as much cool shit into it as possible.  Then
			 * the Windows AIM guys were like, "hey, that's
			 * pretty neat, let's copy those prawns."
			 *
			 * In that spirit, this can contain a custom message,
			 * kind of like an away message, but you're not away
			 * (it's called an "available" message).  Or it can
			 * contain information about the buddy icon the user
			 * has stored on the server.
			 */
			guint16 type2;
			guint8 number2, length2;
			int endpos2;

			/*
			 * Continue looping as long as we're able to read type2,
			 * number2, and length2.
			 */
			while (byte_stream_curpos(bs) + 4 <= endpos) {
				type2 = byte_stream_get16(bs);
				number2 = byte_stream_get8(bs);
				length2 = byte_stream_get8(bs);

				endpos2 = byte_stream_curpos(bs) + MIN(length2, byte_stream_empty(bs));

				switch (type2) {
					case 0x0000: { /* This is an official buddy icon? */
						/* This is always 5 bytes of "0x02 01 d2 04 72"? */
					} break;

					case 0x0001: { /* A buddy icon checksum */
						if ((length2 > 0) && ((number2 == 0x00) || (number2 == 0x01))) {
							g_free(outinfo->iconcsum);
							outinfo->iconcsumtype = number2;
							outinfo->iconcsum = byte_stream_getraw(bs, length2);
							outinfo->iconcsumlen = length2;
						}
					} break;

					case 0x0002: { /* A status/available message */
						g_free(outinfo->status);
						g_free(outinfo->status_encoding);
						if (length2 >= 4) {
							outinfo->status_len = byte_stream_get16(bs);
							outinfo->status = byte_stream_getstr(bs, outinfo->status_len);
							if (byte_stream_get16(bs) == 0x0001) { /* We have an encoding */
								byte_stream_get16(bs);
								outinfo->status_encoding = byte_stream_getstr(bs, byte_stream_get16(bs));
							} else {
								/* No explicit encoding, client should use UTF-8 */
								outinfo->status_encoding = NULL;
							}
						} else {
							byte_stream_advance(bs, length2);
							outinfo->status_len = 0;
							outinfo->status = g_strdup("");
							outinfo->status_encoding = NULL;
						}
					} break;

					case 0x0009: { /* An iTunes Music Store link */
						g_free(outinfo->itmsurl);
						g_free(outinfo->itmsurl_encoding);
						if (length2 >= 4) {
							outinfo->itmsurl_len = byte_stream_get16(bs);
							outinfo->itmsurl = byte_stream_getstr(bs, outinfo->itmsurl_len);
							if (byte_stream_get16(bs) == 0x0001) {
								/* We have an encoding */
								byte_stream_get16(bs);
								outinfo->itmsurl_encoding = byte_stream_getstr(bs, byte_stream_get16(bs));
							} else {
								/* No explicit encoding, client should use UTF-8 */
								outinfo->itmsurl_encoding = NULL;
							}
						} else {
							byte_stream_advance(bs, length2);
							outinfo->itmsurl_len = 0;
							outinfo->itmsurl = g_strdup("");
							outinfo->itmsurl_encoding = NULL;
						}
					} break;

					case 0x000e: { /* ICQ mood */
						char *mood;
						gint32 i;
						gint32 icon_num = -1;

						mood = byte_stream_getstr(bs, length2);

						/* The official clients allow
						 * you to set your custom icon
						 * to the "default" icon, to
						 * allow setting a status
						 * message.  We'll ignore it.
						 */
						if (!*mood)
							break;

						for (i = 0; i < ICQMOODS_COUNT; i++)
							if (!strcmp(mood, icqmoods[i].mood)) {
								icon_num = icqmoods[i].icon_num;
								break; /* should only match once... */
							}

						if (icon_num >= 0)
							outinfo->customicon = icon_num;
						else
							purple_debug_warning("oscar", "Unknown icqmood: %s\n", mood);

						g_free(mood);
					} break;
				}

				/* Save ourselves. */
				byte_stream_setpos(bs, endpos2);
			}

		} else if (type == 0x001e) {
			/*
			 * Always four bytes, but it doesn't look like an int.
			 */

		} else if (type == 0x001f) {
			/*
			 * Seen on a buddy using DeadAIM.  Data was 4 bytes:
			 * 0x00 00 00 10
			 */

		} else {

			/*
			 * Reaching here indicates that either AOL has
			 * added yet another TLV for us to deal with,
			 * or the parsing has gone Terribly Wrong.
			 *
			 * Either way, inform the owner and attempt
			 * recovery.
			 *
			 */
#ifdef LOG_UNKNOWN_TLV
			purple_debug_misc("oscar", "userinfo: **warning: unexpected TLV:\n");
			purple_debug_misc("oscar", "userinfo:   sn    =%s\n", outinfo->sn);
			dumptlv(od, type, bs, length);
#endif
		}

		/* Save ourselves. */
		byte_stream_setpos(bs, endpos);
	}

	aim_locate_adduserinfo(od, outinfo);

	return 0;
}

/*
 * Inverse of aim_info_extract()
 */
int
aim_putuserinfo(ByteStream *bs, aim_userinfo_t *info)
{
	GSList *tlvlist = NULL;

	if (!bs || !info)
		return -EINVAL;

	byte_stream_put8(bs, strlen(info->sn));
	byte_stream_putstr(bs, info->sn);

	byte_stream_put16(bs, info->warnlevel);

	if (info->present & AIM_USERINFO_PRESENT_FLAGS)
		aim_tlvlist_add_16(&tlvlist, 0x0001, info->flags);
	if (info->present & AIM_USERINFO_PRESENT_MEMBERSINCE)
		aim_tlvlist_add_32(&tlvlist, 0x0002, info->membersince);
	if (info->present & AIM_USERINFO_PRESENT_ONLINESINCE)
		aim_tlvlist_add_32(&tlvlist, 0x0003, info->onlinesince);
	if (info->present & AIM_USERINFO_PRESENT_IDLE)
		aim_tlvlist_add_16(&tlvlist, 0x0004, info->idletime);

/* XXX - So, ICQ_OSCAR_SUPPORT is never defined anywhere... */
#ifdef ICQ_OSCAR_SUPPORT
	if (atoi(info->sn) != 0) {
		if (info->present & AIM_USERINFO_PRESENT_ICQEXTSTATUS)
			aim_tlvlist_add_16(&tlvlist, 0x0006, info->icqinfo.status);
		if (info->present & AIM_USERINFO_PRESENT_ICQIPADDR)
			aim_tlvlist_add_32(&tlvlist, 0x000a, info->icqinfo.ipaddr);
	}
#endif

	if (info->present & AIM_USERINFO_PRESENT_CAPABILITIES)
		aim_tlvlist_add_caps(&tlvlist, 0x000d, info->capabilities, info->customicon);

	if (info->present & AIM_USERINFO_PRESENT_SESSIONLEN)
		aim_tlvlist_add_32(&tlvlist, (guint16)((info->flags & AIM_FLAG_AOL) ? 0x0010 : 0x000f), info->sessionlen);

	byte_stream_put16(bs, aim_tlvlist_count(tlvlist));
	aim_tlvlist_write(bs, &tlvlist);
	aim_tlvlist_free(tlvlist);

	return 0;
}

/*
 * Subtype 0x0001
 */
static int
error(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	aim_snac_t *snac2;
	guint16 reason;
	char *sn;
	int was_explicit;

	if (!(snac2 = aim_remsnac(od, snac->id))) {
		purple_debug_misc("oscar", "faim: locate.c, error(): received response from unknown request!\n");
		return 0;
	}

	if ((snac2->family != SNAC_FAMILY_LOCATE) && (snac2->type != 0x0015)) {
		purple_debug_misc("oscar", "faim: locate.c, error(): received response from invalid request! %d\n", snac2->family);
		return 0;
	}

	if (!(sn = snac2->data)) {
		purple_debug_misc("oscar", "faim: locate.c, error(): received response from request without a screen name!\n");
		return 0;
	}

	reason = byte_stream_get16(bs);

	/*
	 * Remove this screen name from our queue.  If the client requested
	 * this buddy's info explicitly, then notify them that we do not have
	 * info for this buddy.
	 */
	was_explicit = aim_locate_gotuserinfo(od, conn, sn);
	if (was_explicit == TRUE)
		if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
			ret = userfunc(od, conn, frame, reason, sn);

	if (snac2)
		g_free(snac2->data);
	g_free(snac2);

	return ret;
}

/*
 * Subtype 0x0002
 *
 * Request Location services rights.
 *
 */
int
aim_locate_reqrights(OscarData *od)
{
	FlapConnection *conn;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)))
		return -EINVAL;

	aim_genericreq_n_snacid(od, conn, SNAC_FAMILY_LOCATE, SNAC_SUBTYPE_LOCATE_REQRIGHTS);

	return 0;
}

/*
 * Subtype 0x0003
 *
 * Normally contains:
 *   t(0001)  - short containing max profile length (value = 1024)
 *   t(0002)  - short - unknown (value = 16) [max MIME type length?]
 *   t(0003)  - short - unknown (value = 10)
 *   t(0004)  - short - unknown (value = 2048) [ICQ only?]
 */
static int
rights(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	GSList *tlvlist;
	aim_rxcallback_t userfunc;
	int ret = 0;
	guint16 maxsiglen = 0;

	tlvlist = aim_tlvlist_read(bs);

	if (aim_tlv_gettlv(tlvlist, 0x0001, 1))
		maxsiglen = aim_tlv_get16(tlvlist, 0x0001, 1);

	if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
		ret = userfunc(od, conn, frame, maxsiglen);

	aim_tlvlist_free(tlvlist);

	return ret;
}

/*
 * Subtype 0x0004
 *
 * Gives BOS your profile.
 *
 * profile_encoding and awaymsg_encoding MUST be set if profile or
 * away are set, respectively, and their value may or may not be
 * restricted to a few choices.  I am currently aware of:
 *
 * us-ascii		Just that
 * unicode-2-0		UTF-16BE
 *
 * profile_len and awaymsg_len MUST be set similarly, and they MUST
 * be the length of their respective strings in bytes.
 *
 * To get the previous behavior of awaymsg == "" un-setting the away
 * message, set awaymsg non-NULL and awaymsg_len to 0 (this is the
 * obvious equivalent).
 *
 */
int
aim_locate_setprofile(OscarData *od,
				  const char *profile_encoding, const gchar *profile, const int profile_len,
				  const char *awaymsg_encoding, const gchar *awaymsg, const int awaymsg_len)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;
	char *encoding;
	static const char defencoding[] = {"text/aolrtf; charset=\"%s\""};

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)))
		return -EINVAL;

	if (!profile && !awaymsg)
		return -EINVAL;

	if ((profile && profile_encoding == NULL) || (awaymsg && awaymsg_len && awaymsg_encoding == NULL)) {
		return -EINVAL;
	}

	/* Build the packet first to get real length */
	if (profile) {
		/* no + 1 here because of %s */
		encoding = g_malloc(strlen(defencoding) + strlen(profile_encoding));
		snprintf(encoding, strlen(defencoding) + strlen(profile_encoding), defencoding, profile_encoding);
		aim_tlvlist_add_str(&tlvlist, 0x0001, encoding);
		aim_tlvlist_add_raw(&tlvlist, 0x0002, profile_len, (const guchar *)profile);
		g_free(encoding);
	}

	/*
	 * So here's how this works:
	 *   - You are away when you have a non-zero-length type 4 TLV stored.
	 *   - You become unaway when you clear the TLV with a zero-length
	 *       type 4 TLV.
	 *   - If you do not send the type 4 TLV, your status does not change
	 *       (that is, if you were away, you'll remain away).
	 */
	if (awaymsg) {
		if (awaymsg_len) {
			encoding = g_malloc(strlen(defencoding) + strlen(awaymsg_encoding));
			snprintf(encoding, strlen(defencoding) + strlen(awaymsg_encoding), defencoding, awaymsg_encoding);
			aim_tlvlist_add_str(&tlvlist, 0x0003, encoding);
			aim_tlvlist_add_raw(&tlvlist, 0x0004, awaymsg_len, (const guchar *)awaymsg);
			g_free(encoding);
		} else
			aim_tlvlist_add_noval(&tlvlist, 0x0004);
	}

	byte_stream_new(&bs, aim_tlvlist_size(tlvlist));

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x0004, 0x0000, NULL, 0);

	aim_tlvlist_write(&bs, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_LOCATE, 0x0004, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * Subtype 0x0004 - Set your client's capabilities.
 */
int
aim_locate_setcaps(OscarData *od, guint32 caps)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)))
		return -EINVAL;

	aim_tlvlist_add_caps(&tlvlist, 0x0005, caps, purple_account_get_int(purple_connection_get_account(od->gc), "customicon", -1));

	byte_stream_new(&bs, aim_tlvlist_size(tlvlist));

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x0004, 0x0000, NULL, 0);

	aim_tlvlist_write(&bs, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_LOCATE, 0x0004, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * Subtype 0x0005 - Request info of another AIM user.
 *
 * @param sn The screenname whose info you wish to request.
 * @param infotype The type of info you wish to request.
 *        0x0001 - Info/profile
 *        0x0003 - Away message
 *        0x0004 - Capabilities
 */
int
aim_locate_getinfo(OscarData *od, const char *sn, guint16 infotype)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)) || !sn)
		return -EINVAL;

	byte_stream_new(&bs, 2+1+strlen(sn));

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x0005, 0x0000, NULL, 0);

	byte_stream_put16(&bs, infotype);
	byte_stream_put8(&bs, strlen(sn));
	byte_stream_putstr(&bs, sn);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_LOCATE, 0x0005, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/* Subtype 0x0006 */
static int
userinfo(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	aim_userinfo_t *userinfo, *userinfo2;
	GSList *tlvlist;
	aim_tlv_t *tlv = NULL;
	int was_explicit;

	userinfo = (aim_userinfo_t *)g_malloc(sizeof(aim_userinfo_t));
	aim_info_extract(od, bs, userinfo);
	tlvlist = aim_tlvlist_read(bs);

	/* Profile will be 1 and 2 */
	userinfo->info_encoding = aim_tlv_getstr(tlvlist, 0x0001, 1);
	if ((tlv = aim_tlv_gettlv(tlvlist, 0x0002, 1))) {
		userinfo->info = (char *)g_malloc(tlv->length);
		memcpy(userinfo->info, tlv->value, tlv->length);
		userinfo->info_len = tlv->length;
	}

	/* Away message will be 3 and 4 */
	userinfo->away_encoding = aim_tlv_getstr(tlvlist, 0x0003, 1);
	if ((tlv = aim_tlv_gettlv(tlvlist, 0x0004, 1))) {
		userinfo->away = (char *)g_malloc(tlv->length);
		memcpy(userinfo->away, tlv->value, tlv->length);
		userinfo->away_len = tlv->length;
	}

	/* Caps will be 5 */
	if ((tlv = aim_tlv_gettlv(tlvlist, 0x0005, 1))) {
		ByteStream cbs;
		byte_stream_init(&cbs, tlv->value, tlv->length);
		userinfo->capabilities = aim_locate_getcaps(od, &cbs, tlv->length);
		byte_stream_rewind(&cbs);
		userinfo->customicon = aim_get_custom_icon(od, &cbs, tlv->length);
		userinfo->present = AIM_USERINFO_PRESENT_CAPABILITIES;
	}
	aim_tlvlist_free(tlvlist);

	aim_locate_adduserinfo(od, userinfo);
	userinfo2 = aim_locate_finduserinfo(od, userinfo->sn);
	aim_info_free(userinfo);
	g_free(userinfo);

	/*
	 * Remove this screen name from our queue.  If the client requested
	 * this buddy's info explicitly, then notify them that we have info
	 * for this buddy.
	 */
	if (userinfo2 != NULL)
	{
		was_explicit = aim_locate_gotuserinfo(od, conn, userinfo2->sn);
		if (was_explicit == TRUE)
			if ((userfunc = aim_callhandler(od, snac->family, snac->subtype)))
				ret = userfunc(od, conn, frame, userinfo2);
	}

	return ret;
}

/*
 * Subtype 0x0009 - Set directory profile data.
 *
 * This is not the same as aim_location_setprofile!
 * privacy: 1 to allow searching, 0 to disallow.
 *
 */
int aim_locate_setdirinfo(OscarData *od, const char *first, const char *middle, const char *last, const char *maiden, const char *nickname, const char *street, const char *city, const char *state, const char *zip, int country, guint16 privacy)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)))
		return -EINVAL;

	aim_tlvlist_add_16(&tlvlist, 0x000a, privacy);

	if (first)
		aim_tlvlist_add_str(&tlvlist, 0x0001, first);
	if (last)
		aim_tlvlist_add_str(&tlvlist, 0x0002, last);
	if (middle)
		aim_tlvlist_add_str(&tlvlist, 0x0003, middle);
	if (maiden)
		aim_tlvlist_add_str(&tlvlist, 0x0004, maiden);

	if (state)
		aim_tlvlist_add_str(&tlvlist, 0x0007, state);
	if (city)
		aim_tlvlist_add_str(&tlvlist, 0x0008, city);

	if (nickname)
		aim_tlvlist_add_str(&tlvlist, 0x000c, nickname);
	if (zip)
		aim_tlvlist_add_str(&tlvlist, 0x000d, zip);

	if (street)
		aim_tlvlist_add_str(&tlvlist, 0x0021, street);

	byte_stream_new(&bs, aim_tlvlist_size(tlvlist));

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x0009, 0x0000, NULL, 0);

	aim_tlvlist_write(&bs, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_LOCATE, 0x0009, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * Subtype 0x000b - Huh? What is this?
 */
int aim_locate_000b(OscarData *od, const char *sn)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;

		return -EINVAL;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)) || !sn)
		return -EINVAL;

	byte_stream_new(&bs, 1+strlen(sn));

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x000b, 0x0000, NULL, 0);

	byte_stream_put8(&bs, strlen(sn));
	byte_stream_putstr(&bs, sn);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_LOCATE, 0x000b, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);

	return 0;
}

/*
 * Subtype 0x000f
 *
 * XXX pass these in better
 *
 */
int
aim_locate_setinterests(OscarData *od, const char *interest1, const char *interest2, const char *interest3, const char *interest4, const char *interest5, guint16 privacy)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;
	GSList *tlvlist = NULL;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)))
		return -EINVAL;

	/* ?? privacy ?? */
	aim_tlvlist_add_16(&tlvlist, 0x000a, privacy);

	if (interest1)
		aim_tlvlist_add_str(&tlvlist, 0x0000b, interest1);
	if (interest2)
		aim_tlvlist_add_str(&tlvlist, 0x0000b, interest2);
	if (interest3)
		aim_tlvlist_add_str(&tlvlist, 0x0000b, interest3);
	if (interest4)
		aim_tlvlist_add_str(&tlvlist, 0x0000b, interest4);
	if (interest5)
		aim_tlvlist_add_str(&tlvlist, 0x0000b, interest5);

	byte_stream_new(&bs, aim_tlvlist_size(tlvlist));

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x000f, 0x0000, NULL, 0);

	aim_tlvlist_write(&bs, &tlvlist);
	aim_tlvlist_free(tlvlist);

	flap_connection_send_snac(od, conn, SNAC_FAMILY_LOCATE, 0x000f, 0x0000, snacid, &bs);

	byte_stream_destroy(&bs);
	return 0;
}

/*
 * Subtype 0x0015 - Request the info of a user using the short method.  This is
 * what iChat uses.  It normally is VERY leniently rate limited.
 *
 * @param sn The screen name whose info you wish to request.
 * @param flags The bitmask which specifies the type of info you wish to request.
 *        0x00000001 - Info/profile.
 *        0x00000002 - Away message.
 *        0x00000004 - Capabilities.
 *        0x00000008 - Certification.
 * @return Return 0 if no errors, otherwise return the error number.
 */
int
aim_locate_getinfoshort(OscarData *od, const char *sn, guint32 flags)
{
	FlapConnection *conn;
	ByteStream bs;
	aim_snacid_t snacid;

	if (!od || !(conn = flap_connection_findbygroup(od, SNAC_FAMILY_LOCATE)) || !sn)
		return -EINVAL;

	byte_stream_new(&bs, 4 + 1 + strlen(sn));
	byte_stream_put32(&bs, flags);
	byte_stream_put8(&bs, strlen(sn));
	byte_stream_putstr(&bs, sn);

	snacid = aim_cachesnac(od, SNAC_FAMILY_LOCATE, 0x0015, 0x0000, sn, strlen(sn)+1);
	flap_connection_send_snac_with_priority(od, conn, SNAC_FAMILY_LOCATE, 0x0015, 0x0000, snacid, &bs, FALSE);

	byte_stream_destroy(&bs);

	return 0;
}

static int
snachandler(OscarData *od, FlapConnection *conn, aim_module_t *mod, FlapFrame *frame, aim_modsnac_t *snac, ByteStream *bs)
{
	if (snac->subtype == 0x0001)
		return error(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x0003)
		return rights(od, conn, mod, frame, snac, bs);
	else if (snac->subtype == 0x0006)
		return userinfo(od, conn, mod, frame, snac, bs);

	return 0;
}

static void
locate_shutdown(OscarData *od, aim_module_t *mod)
{
	aim_userinfo_t *del;

	while (od->locate.userinfo) {
		del = od->locate.userinfo;
		od->locate.userinfo = od->locate.userinfo->next;
		aim_info_free(del);
		g_free(del);
	}
}

int
locate_modfirst(OscarData *od, aim_module_t *mod)
{
	mod->family = SNAC_FAMILY_LOCATE;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "locate", sizeof(mod->name));
	mod->snachandler = snachandler;
	mod->shutdown = locate_shutdown;

	return 0;
}

guint32
aim_get_custom_icons_count()
{
	return AIM_CUSTOM_ICONS_COUNT;
}

char*
aim_get_custom_icon_filename(gint32 no)
{
	if (no >= AIM_CUSTOM_ICONS_COUNT || no < 1)
		return NULL;
	return aim_custom_icons[no].filename;
}

char*
aim_get_custom_icon_descriptivename(gint32 no)
{
	if (no >= AIM_CUSTOM_ICONS_COUNT || no < 1)
		return NULL;
	return aim_custom_icons[no].descriptivename;
}

guint8*
aim_get_custom_icon_data(gint32 no)
{
	if (no >= AIM_CUSTOM_ICONS_COUNT || no < 1)
		return NULL;
	return (guint8 *)aim_custom_icons[no].data;
}
