#include <tpl-observer.h>
#include <tpl-channel.h>

G_DEFINE_TYPE (TplChannel, tpl_channel, G_TYPE_OBJECT)

static void tpl_channel_class_init(TplChannelClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}


static void tpl_channel_init(TplChannel* self) {
	/* Init TplChannel's members to zero/NULL */
#define TPL_SET_NULL(x) tpl_channel_set_##x(self, NULL)
	TPL_SET_NULL(channel);
	TPL_SET_NULL(channel_path);
	TPL_SET_NULL(channel_type);
	TPL_SET_NULL(channel_properties);
	TPL_SET_NULL(account);
	TPL_SET_NULL(account_path);
	TPL_SET_NULL(connection);
	TPL_SET_NULL(connection_path);
	TPL_SET_NULL(observer);
#undef TPL_SET_NULL
}

TplChannel* tpl_channel_new(TpSvcClientObserver* observer) {
	TplChannel *ret = g_object_new(TPL_TYPE_CHANNEL,NULL);
	tpl_channel_set_observer(ret, observer);
	return ret;
}

void tpl_channel_free(TplChannel* tpl_text) {
	/* TODO free and unref other members */
	g_free(tpl_text);
}


TpSvcClientObserver* tpl_channel_get_observer(TplChannel *self) {
	return self->observer;
}
TpAccount *tpl_channel_get_account(TplChannel *self) {
	return self->account;
}
const gchar *tpl_channel_get_account_path(TplChannel *self) {
	return self->account_path;
}
TpConnection *tpl_channel_get_connection(TplChannel *self) {
	return self->connection;
}
const gchar *tpl_channel_get_connection_path(TplChannel *self) {
	return self->connection_path;
}
TpChannel *tpl_channel_get_channel(TplChannel *self) {
	return self->channel;
}
const gchar *tpl_channel_get_channel_path(TplChannel *self) {
	return self->channel_path;
}
const gchar *tpl_channel_get_channel_type(TplChannel *self) {
	return self->channel_type;
}
GHashTable *tpl_channel_get_channel_properties(TplChannel *self) {
	return self->channel_properties;
}



void tpl_channel_set_observer(TplChannel *self,
		TpSvcClientObserver *data) {
	//g_debug("SET observer\n");
	_unref_object_if_not_null(&(self->observer));
	self->observer = data;
	_ref_object_if_not_null(data);
}
void tpl_channel_set_account(TplChannel *self, TpAccount *data) {
	//g_debug("SET account\n");
	_unref_object_if_not_null(&(self->account));
	if (self->account!=NULL)
		g_object_unref(self->account);
	self->account = data;
	_ref_object_if_not_null(data);
}
void tpl_channel_set_account_path(TplChannel *self, const gchar *data) {
	//g_debug("SET path\n");
	if (self->account!=NULL)
	self->account_path = data;
}
void tpl_channel_set_connection(TplChannel *self, TpConnection *data) {
	//g_debug("SET connection\n");
	_unref_object_if_not_null(&(self->connection));
	self->connection = data;
	_ref_object_if_not_null(data);
}
void tpl_channel_set_connection_path(TplChannel *self, const gchar *data) {
	//g_debug("SET connectin path\n");
	self->connection_path = data;
}
void tpl_channel_set_channel(TplChannel *self, TpChannel *data) {
	//g_debug("SET channel\n");
	_unref_object_if_not_null(&(self->channel));
	self->channel = data;
	_ref_object_if_not_null(data);
}
void tpl_channel_set_channel_path(TplChannel *self, const gchar *data) {
	//g_debug("SET channel path\n");
	self->channel_path = data;
}
void tpl_channel_set_channel_type(TplChannel *self, const gchar *data) {
	//g_debug("SET channel type\n");
	self->channel_type = data;
}
void tpl_channel_set_channel_properties(TplChannel *self, GHashTable *data) {
	//g_debug("SET channel prop\n");
	self->channel_properties = data;
}
