
#include "module.h"

/* TODO


*/

MODULE = Gaim::Notify  PACKAGE = Gaim::Notify  PREFIX = gaim_notify_
PROTOTYPES: ENABLE



void 
gaim_notify_close(type, ui_handle)
	Gaim::NotifyType type
 	void * ui_handle

void 
gaim_notify_close_with_handle(handle)
 	void * handle 

void *
gaim_notify_email(handle, subject, from, to, url, cb, user_data)
 	void * handle
	const char *subject
	const char *from
	const char *to
	const char *url
	GCallback cb
	void * user_data
 

void *
gaim_notify_emails(handle, count, detailed, subjects, froms, tos, urls, cb, user_data)
 	void * handle
	size_t count
	gboolean detailed
	const char **subjects
	const char **froms
	const char **tos
	const char **urls
	GCallback cb
	void * user_data
 

void *
gaim_notify_formatted(handle, title, primary, secondary, text, cb, user_data)
 	void * handle
	const char *title
	const char *primary
	const char *secondary
	const char *text
	GCallback cb
	void * user_data
 

Gaim::NotifyUiOps
gaim_notify_get_ui_ops()
 

void *
gaim_notify_message(handle, type, title, primary, secondary, cb, user_data)
 	void * handle
	Gaim::NotifyMsgType type
	const char *title
	const char *primary
	const char *secondary
	GCallback cb
	void * user_data
 

void *
gaim_notify_searchresults(gc, title, primary, secondary, results, cb, user_data)
	Gaim::Connection gc
	const char *title
	const char *primary
	const char *secondary
	const char **results
	GCallback cb
	void * user_data

void 
gaim_notify_set_ui_ops(ops)
	Gaim::NotifyUiOps ops

void *
gaim_notify_uri(handle, uri)
 	void * handle
	const char *uri

void *
gaim_notify_userinfo(gc, who, title, primary, secondary, text, cb, user_data)
	Gaim::Connection gc
	const char *who
	const char *title
	const char *primary
	const char *secondary
	const char *text
	GCallback cb
	void * user_data
 

