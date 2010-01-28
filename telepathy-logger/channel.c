/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "channel.h"

#include <string.h>

#include <glib.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/channel-text.h>

#define TPCHAN_PROP_PREFIX "org.freedesktop.Telepathy.Channel."
#define TPCHAN_PROP_PREFIX_LEN strlen(TPCHAN_PROP_PREFIX)

static void tpl_channel_set_account (TplChannel *self, TpAccount *data);
static void tpl_channel_call_when_ready (TplChannel *self, GAsyncReadyCallback cb,
    gpointer user_data);
static void pendingproc_get_ready_tp_connection (TplActionChain *ctx);
static void got_ready_tp_connection_cb (TpConnection *connection,
    const GError *error, gpointer user_data);
static void pendingproc_get_ready_tp_channel (TplActionChain *ctx);
static void got_ready_tp_channel_cb (TpChannel *channel,
    const GError *error, gpointer user_data);
static void pendingproc_register_tpl_channel (TplActionChain *ctx);

static gchar *channel_types[] = {
  "org.freedesktop.Telepathy.Channel.Type.Text",
  NULL
};

static TplChannelConstructor *channel_constructors[] = {
    (TplChannelConstructor*) tpl_channel_text_new,
    NULL
};


G_DEFINE_ABSTRACT_TYPE (TplChannel, tpl_channel, TP_TYPE_CHANNEL)

#define GET_PRIV(obj) TPL_GET_PRIV (obj, TplChannel)
struct _TplChannelPriv
{
  TpAccount *account;
};

enum
{
  PROP0,
  PROP_ACCOUNT
};

static void
get_prop (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplChannelPriv *priv = GET_PRIV (object);

  switch (param_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, priv->account);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    };
}

static void
set_prop (GObject *object,
    guint param_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TplChannel *self = TPL_CHANNEL (object);

  switch (param_id) {
      case PROP_ACCOUNT:
        tpl_channel_set_account (self, g_value_get_object (value));
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
  };
}

static void
tpl_channel_dispose (GObject *obj)
{
  TplChannelPriv *priv = GET_PRIV (obj);

  tpl_object_unref_if_not_null (priv->account);
  priv->account = NULL;

  G_OBJECT_CLASS (tpl_channel_parent_class)->dispose (obj);
}

static void
tpl_channel_finalize (GObject *obj)
{
  G_OBJECT_CLASS (tpl_channel_parent_class)->finalize (obj);
}

static void
tpl_channel_class_init (TplChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_channel_dispose;
  object_class->finalize = tpl_channel_finalize;
  object_class->get_property = get_prop;
  object_class->set_property = set_prop;

  klass->call_when_ready = tpl_channel_call_when_ready;

  param_spec = g_param_spec_object ("account",
      "Account", "TpAccount instance associated with TplChannel",
      TP_TYPE_ACCOUNT, G_PARAM_READABLE | G_PARAM_WRITABLE |
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  g_type_class_add_private (object_class, sizeof (TplChannelPriv));
}


static void
tpl_channel_init (TplChannel *self)
{
  TplChannelPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TPL_TYPE_CHANNEL,
      TplChannelPriv);
  self->priv = priv;
}


TpAccount *
tpl_channel_get_account (TplChannel *self)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);

  return priv->account;
}


static void
tpl_channel_set_account (TplChannel *self,
    TpAccount *data)
{
  TplChannelPriv *priv = GET_PRIV(self);

  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_ACCOUNT (data));
  g_return_if_fail (priv->account == NULL);

  priv->account = data;
  g_object_ref (data);
}


TplChannelConstructor *
tpl_channel_factory (const gchar *channel_type)
{
  guint i;

  if (G_N_ELEMENTS (channel_types) != G_N_ELEMENTS (channel_constructors))
    g_critical ("channel_types and channel_constructors have different sizes");

  for(i=0; i < G_N_ELEMENTS (channel_types); ++i)
    if (tp_strdiff (channel_type, channel_types[i]))
      return channel_constructors[i];

  /* if the flow reaches here, it means that channel_type is not among the
     recognized ones */
  g_debug ("%s: channel type not handled by this logger", channel_type);
  return NULL;
}


/**
 * It has to be called by all the child classes in order to prepare all the
 * objects involved in the logging process.
 *
 * It internally calls _call_when_ready for TpAccount TpConnection and
 * TpChannel itself. When everything is ready calls the user passed callback.
 */
static void
tpl_channel_call_when_ready (TplChannel *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *actions;

  g_debug ("tp_connection_call_when_ready");
  actions = tpl_actionchain_new (G_OBJECT(self), cb, user_data);
  tpl_actionchain_append (actions, pendingproc_get_ready_tp_connection);
  tpl_actionchain_append (actions, pendingproc_get_ready_tp_channel);
  tpl_actionchain_append (actions, pendingproc_register_tpl_channel);
  tpl_actionchain_continue (actions);
}


static void
pendingproc_get_ready_tp_connection (TplActionChain *ctx)
{
  TplChannel *tpl_chan = tpl_actionchain_get_object (ctx);
  TpConnection *tp_conn = tp_channel_borrow_connection (TP_CHANNEL (
      tpl_chan));

  g_debug ("Preparing TpConnection");
  tp_connection_call_when_ready (tp_conn, got_ready_tp_connection_cb, ctx);
}

static void
got_ready_tp_connection_cb (TpConnection *connection,
    const GError *error,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  TplChannel *tpl_chan = tpl_actionchain_get_object (ctx);
  gchar *chan_path;

  if (error != NULL)
    {
      g_object_get (G_OBJECT (tpl_chan), "object-path", &chan_path, NULL);
      g_debug ("%s. Giving up channel '%s' observation", error->message,
          chan_path);

      g_free (chan_path);
      g_object_unref (tpl_chan);
      return;
    }
  g_debug ("2");

  tpl_actionchain_continue (ctx);
}

static void
pendingproc_get_ready_tp_channel (TplActionChain *ctx)
{
  TplChannel *tpl_chan = tpl_actionchain_get_object (ctx);

  g_debug ("Preparing TpChannel");

  /* user_data is a TplChannel instance */
  tp_channel_call_when_ready (TP_CHANNEL (tpl_chan), got_ready_tp_channel_cb,
      ctx);
}


static void
got_ready_tp_channel_cb (TpChannel *channel,
    const GError *error,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  TplChannel *tpl_chan = tpl_actionchain_get_object (ctx);

  if (error != NULL)
    {
      gchar *chan_path;
      TpConnection *tp_conn;

      g_object_get (channel, "connection", &tp_conn, NULL);
      g_object_get (G_OBJECT (channel), "object-path", &chan_path, NULL);
      g_debug ("%s. Giving up channel '%s' observation", error->message,
          chan_path);

      g_free (chan_path);
      g_object_unref (tpl_chan);
      g_object_unref (tp_conn);
      return;
    }

  tpl_actionchain_continue (ctx);
}


static void
pendingproc_register_tpl_channel (TplActionChain *ctx)
{
  /* singleton */
  TplObserver *observer = tpl_observer_new ();
  TplChannel *tpl_chan = tpl_actionchain_get_object (ctx);

  g_debug ("Registering TplChannel into TplObserver");
  tpl_observer_register_channel (observer, tpl_chan);
  g_object_unref (observer);

  tpl_actionchain_continue (ctx);
}
