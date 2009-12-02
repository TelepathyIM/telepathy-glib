#ifndef __TPL_TEXT_CHANNEL_H__
#define __TPL_TEXT_CHANNEL_H__

#include <glib-object.h>
#include <telepathy-glib/channel.h>
#include <telepathy-glib/account.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/contact.h>
#include <telepathy-glib/svc-client.h>

#include <tpl_observer.h>
#include <tpl_utils.h>

G_BEGIN_DECLS

#define TPL_TYPE_TEXT_CHANNEL                  (tpl_text_channel_get_type ())
#define TPL_TEXT_CHANNEL(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TPL_TYPE_TEXT_CHANNEL, TplTextChannel))
#define TPL_TEXT_CHANNEL_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), TPL_TYPE_TEXT_CHANNEL, TplTextChannelClass))
#define TPL_IS_TEXT_CHANNEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TPL_TYPE_TEXT_CHANNEL))
#define TPL_IS_TEXT_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TPL_TYPE_TEXT_CHANNEL))
#define TPL_TEXT_CHANNEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TPL_TYPE_TEXT_CHANNEL, TplTextChannelClass))


typedef struct {
	GObject		parent;

	/* private */
	TplChannel	*tpl_channel;
	TpContact	*remote_contact, *my_contact;
} TplTextChannel;

typedef struct {
	GObjectClass	parent_class;
} TplTextChannelClass;

GType  tpl_text_channel_get_type (void);

TplTextChannel* tpl_text_channel_new(TplChannel* tpl_channel);
void tpl_text_channel_free(TplTextChannel* tpl_chan);

TplChannel *tpl_text_channel_get_tpl_channel(TplTextChannel *self);
TpContact *tpl_text_channel_get_remote_contact(TplTextChannel *self);
TpContact *tpl_text_channel_get_my_contact(TplTextChannel *self);

void tpl_text_channel_set_tpl_channel(TplTextChannel *self, TplChannel *tpl_chan);
void tpl_text_channel_set_remote_contact(TplTextChannel *self, TpContact *data);
void tpl_text_channel_set_my_contact(TplTextChannel *self, TpContact *data);

G_END_DECLS

#endif // __TPL_TEXT_CHANNEL_H__
