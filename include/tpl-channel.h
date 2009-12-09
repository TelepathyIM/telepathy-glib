#ifndef __TPL_DATA_H__
#define __TPL_DATA_H__

#include <glib-object.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/svc-client.h>

#include <tpl-observer.h>
#include <tpl-utils.h>

G_BEGIN_DECLS

#define TPL_TYPE_CHANNEL                  (tpl_channel_get_type ())
#define TPL_CHANNEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_CHANNEL, TplChannel))
#define TPL_CHANNEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_CHANNEL, TplChannelClass))
#define TPL_IS_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_CHANNEL))
#define TPL_IS_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_CHANNEL))
#define TPL_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_CHANNEL, TplChannelClass))


typedef struct {
	GObject		parent;

	/* private */
	TpChannel	*channel;
	const gchar	*channel_path;
	const gchar	*channel_type;
	GHashTable	*channel_properties;

	TpAccount	*account;
	const gchar	*account_path;
	TpConnection	*connection;
	const gchar	*connection_path;

	// temporarely storing self and remote handle
	// no getter/setter, access the member direclty
	// TODO find a better way to store temporarely the handle
	TpHandle	tmp_remote;

	TpSvcClientObserver	*observer;
} TplChannel;

typedef struct {
	GObjectClass	parent_class;
} TplChannelClass;


GType tpl_channel_get_type (void);

TplChannel* tpl_channel_new (TpSvcClientObserver *observer);
void tpl_channel_free(TplChannel* tpl_chan);


TpSvcClientObserver*tpl_channel_get_observer(TplChannel *self);
TpAccount *tpl_channel_get_account(TplChannel *self);
const gchar *tpl_channel_get_account_path(TplChannel *self);
TpConnection *tpl_channel_get_connection(TplChannel *self);
const gchar *tpl_channel_get_connection_path(TplChannel *self);
TpChannel *tpl_channel_get_channel(TplChannel *self);
const gchar *tpl_channel_get_channel_path(TplChannel *self);
const gchar *tpl_channel_get_channel_type(TplChannel *self);
GHashTable *tpl_channel_get_channel_properties(TplChannel *self);


void tpl_channel_set_observer(TplChannel *self,
		TpSvcClientObserver *data);
void tpl_channel_set_account(TplChannel *self, TpAccount *data);
void tpl_channel_set_account_path(TplChannel *self, const gchar *data);
void tpl_channel_set_connection(TplChannel *self, TpConnection *data);
void tpl_channel_set_connection_path(TplChannel *self, const gchar *data);
void tpl_channel_set_channel(TplChannel *self, TpChannel *data);
void tpl_channel_set_channel_path(TplChannel *self, const gchar *data);
void tpl_channel_set_channel_type(TplChannel *self, const gchar *data);
void tpl_channel_set_channel_properties(TplChannel *self, GHashTable *data);

G_END_DECLS

#endif // __TPL_DATA_H__
