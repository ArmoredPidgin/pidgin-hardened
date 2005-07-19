
#include "module.h"

/* TODO

Gaim::Presence
gaim_account_get_presence(account)
	Gaim::Account account

void 
gaim_account_set_presence(account, presence)
	Gaim::Account account
	Gaim::Presence presence

*/

/**********************XS Code for Account.xs*********************************/
MODULE = Gaim::Account  PACKAGE = Gaim::Account  PREFIX = gaim_account_
PROTOTYPES: ENABLE

Gaim::Account
gaim_account_new(username, protocol_id)
	const char * username
	const char * protocol_id

void 
gaim_account_destroy(account)
	Gaim::Account account

void 
gaim_account_connect(account)
	Gaim::Account account

void 
gaim_account_register(account)
	Gaim::Account account

void 
gaim_account_disconnect(account)
	Gaim::Account account

void 
gaim_account_request_change_password(account)
	Gaim::Account account

void 
gaim_account_request_change_user_info(account)
	Gaim::Account account

void 
gaim_account_set_username(account, username)
	Gaim::Account account
	const char * username

void 
gaim_account_set_password(account, password)
	Gaim::Account account
	const char * password

void 
gaim_account_set_alias(account, alias)
	Gaim::Account account
	const char * alias

void 
gaim_account_set_user_info(account, user_info)
	Gaim::Account account
	const char *user_info

void 
gaim_account_set_buddy_icon(account, icon)
	Gaim::Account account
	const char *icon

void 
gaim_account_set_connection(account, gc)
	Gaim::Account account
	Gaim::Connection gc

void 
gaim_account_set_remember_password(account, value)
	Gaim::Account account
	gboolean value

void 
gaim_account_set_check_mail(account, value)
	Gaim::Account account
	gboolean value

void 
gaim_account_set_proxy_info(account, info)
	Gaim::Account account
	Gaim::ProxyInfo info

void
gaim_account_set_status(account, status_id, active) 
	Gaim::Account account
	const char *status_id
	gboolean active
CODE:
	gaim_account_set_status(account, status_id, active);


void 
gaim_account_set_status_types(account, status_types)
	Gaim::Account account
	SV * status_types
PREINIT:
	GList *t_GL;
	int i, t_len;
PPCODE:
	t_GL = NULL;
	t_len = av_len((AV *)SvRV(status_types));

	for (i = 0; i < t_len; i++) {
		STRLEN t_sl;
		t_GL = g_list_append(t_GL, SvPV(*av_fetch((AV *)SvRV(status_types), i, 0), t_sl));
	}
	gaim_account_set_status_types(account, t_GL);


void 
gaim_account_clear_settings(account)
	Gaim::Account account

void 
gaim_account_set_int(account, name, value)
	Gaim::Account account
	const char *name
	int value

gboolean 
gaim_account_is_connected(account)
	Gaim::Account account

const char *
gaim_account_get_username(account)
	Gaim::Account account

const char *
gaim_account_get_password(account)
	Gaim::Account account

const char *
gaim_account_get_alias(account)
	Gaim::Account account

const char *
gaim_account_get_user_info(account)
	Gaim::Account account

const char *
gaim_account_get_buddy_icon(account)
	Gaim::Account account

const char *
gaim_account_get_protocol_id(account)
	Gaim::Account account

const char *
gaim_account_get_protocol_name(account)
	Gaim::Account account

Gaim::Connection
gaim_account_get_connection(account)
	Gaim::Account account

gboolean 
gaim_account_get_remember_password(account)
	Gaim::Account account

gboolean 
gaim_account_get_check_mail(account)
	Gaim::Account account

Gaim::ProxyInfo
gaim_account_get_proxy_info(account)
	Gaim::Account account

Gaim::Status
gaim_account_get_active_status(account)
	Gaim::Account account

void
gaim_account_get_status_types(account)
	Gaim::Account account
PREINIT:
	const GList *l;
PPCODE:
	for (l = gaim_account_get_status_types(account); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Status::Type")));
	}

Gaim::Log
gaim_account_get_log(account)
	Gaim::Account account

void 
gaim_account_destroy_log(account)
	Gaim::Account account




MODULE = Gaim::Account  PACKAGE = Gaim::Accounts  PREFIX = gaim_accounts_
PROTOTYPES: ENABLE

void 
gaim_accounts_add(account)
	Gaim::Account account

void 
gaim_accounts_remove(account)
	Gaim::Account account

void 
gaim_accounts_delete(account)
	Gaim::Account account

void 
gaim_accounts_reorder(account, new_index)
	Gaim::Account account
	size_t new_index

void
gaim_accounts_get_all()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_accounts_get_all(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Account")));
	}

void
gaim_accounts_get_all_active()
PREINIT:
	GList *l;
PPCODE:
	for (l = gaim_accounts_get_all_active(); l != NULL; l = l->next) {
		XPUSHs(sv_2mortal(gaim_perl_bless_object(l->data, "Gaim::Account")));
	}

Gaim::Account
gaim_accounts_find(name, protocol)
	const char * name
	const char * protocol

void 
gaim_accounts_set_ui_ops(ops)
	Gaim::Account::UiOps ops

Gaim::Account::UiOps 
gaim_accounts_get_ui_ops()

void 
gaim_accounts_get_handle()

void 
gaim_accounts_init()

void 
gaim_accounts_uninit()
