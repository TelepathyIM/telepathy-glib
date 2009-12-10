#include <glib.h>
#include <tpl-observer.h>
#include <tpl-channel.h>

G_DEFINE_TYPE (TplChannel, tpl_channel, G_TYPE_OBJECT)


static void tpl_channel_dispose (GObject* obj)
{
	TplChannel *self = TPL_CHANNEL(obj);

	tpl_object_unref_if_not_null (self->channel);
	self->channel = NULL;
	tpl_object_unref_if_not_null (self->channel_properties);
	self->channel_properties = NULL;
	tpl_object_unref_if_not_null (self->account);
	self->account = NULL;
	tpl_object_unref_if_not_null (self->connection);
	self->connection = NULL;
	tpl_object_unref_if_not_null (self->observer);
	self->observer = NULL;

	G_OBJECT_CLASS (tpl_channel_parent_class)->dispose (obj);
}

static void tpl_channel_finalize (GObject* obj)
{
	TplChannel *self = TPL_CHANNEL(obj);

	g_free ((gchar*) self->channel_path);
	g_free ((gchar*) self->channel_type);
	g_free ((gchar*) self->account_path);
	g_free ((gchar*) self->connection_path);

	G_OBJECT_CLASS (tpl_channel_parent_class)->finalize (obj);

	g_debug("TplChannel instnace finalized\n");
}


static void tpl_channel_class_init(TplChannelClass* klass) {
	GObjectClass* object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = tpl_channel_dispose;
	object_class->finalize = tpl_channel_finalize;
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

TplChannel* tpl_channel_new(TpSvcClientObserver* observer)
{
	TplChannel *ret = g_object_new(TPL_TYPE_CHANNEL,NULL);
	tpl_channel_set_observer(ret, observer);
	return ret;
}

TpSvcClientObserver* tpl_channel_get_observer(TplChannel *self)
{
	return self->observer;
}

TpAccount *tpl_channel_get_account(TplChannel *self)
{
	return self->account;
}

const gchar *tpl_channel_get_account_path(TplChannel *self)
{
	return self->account_path;
}

TpConnection *tpl_channel_get_connection(TplChannel *self)
{
	return self->connection;
}

const gchar *tpl_channel_get_connection_path(TplChannel *self)
{
	return self->connection_path;
}

TpChannel *tpl_channel_get_channel(TplChannel *self)
{
	return self->channel;
}

const gchar *tpl_channel_get_channel_path(TplChannel *self)
{
	return self->channel_path;
}

const gchar *tpl_channel_get_channel_type(TplChannel *self)
{
	return self->channel_type;
}

GHashTable *tpl_channel_get_channel_properties(TplChannel *self)
{
	return self->channel_properties;
}



void tpl_channel_set_observer(TplChannel *self,
		TpSvcClientObserver *data)
{
	tpl_object_unref_if_not_null(self->observer);
	self->observer = data;
	tpl_object_ref_if_not_null(data);
}

void tpl_channel_set_account(TplChannel *self, TpAccount *data)
{
	tpl_object_unref_if_not_null(self->account);
	self->account = data;
	tpl_object_ref_if_not_null(data);
}

void tpl_channel_set_account_path(TplChannel *self, const gchar *data)
{
	g_free ((gchar*) self->account_path);
	self->account_path = data;
}

void tpl_channel_set_connection(TplChannel *self, TpConnection *data)
{
	tpl_object_unref_if_not_null(self->connection);
	self->connection = data;
	tpl_object_ref_if_not_null(data);
}

void tpl_channel_set_connection_path(TplChannel *self, const gchar *data)
{
	g_free((gchar*) self->connection_path);
	self->connection_path = g_strdup (data);
}
void tpl_channel_set_channel(TplChannel *self, TpChannel *data)
{
	tpl_object_unref_if_not_null(self->channel);
	self->channel = data;
	tpl_object_ref_if_not_null(data);
}
void tpl_channel_set_channel_path(TplChannel *self, const gchar *data)
{
	g_free((gchar*) self->channel_path);
	self->channel_path = g_strdup (data);
}

void tpl_channel_set_channel_type(TplChannel *self, const gchar *data)
{
	g_free((gchar*) self->channel_type);
	self->channel_type = g_strdup (data);
}

void tpl_channel_set_channel_properties(TplChannel *self, GHashTable *data)
{
	if (self->channel_properties != NULL)
		g_hash_table_unref(self->channel_properties);
	self->channel_properties = data;
	g_hash_table_ref(data);
}
