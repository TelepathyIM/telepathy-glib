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

#include "config.h"
#include "channel.h"

#include <string.h>

#include <glib.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/channel-text.h>
#include <telepathy-logger/observer.h>

#define DEBUG_FLAG TPL_DEBUG_CHANNEL
#include <telepathy-logger/action-chain-internal.h>
#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/util-internal.h>

#define TPCHAN_PROP_PREFIX "org.freedesktop.Telepathy.Channel."
#define TPCHAN_PROP_PREFIX_LEN strlen(TPCHAN_PROP_PREFIX)

static void tpl_channel_set_account (TplChannel *self, TpAccount *data);
static void call_when_ready_protected (TplChannel *self,
    GAsyncReadyCallback cb, gpointer user_data);
static void pendingproc_get_ready_tp_connection (TplActionChain *ctx,
    gpointer user_data);
static void got_ready_tp_connection_cb (TpConnection *connection,
    const GError *error, gpointer user_data);
static void pendingproc_get_ready_tp_channel (TplActionChain *ctx,
    gpointer user_data);
static void got_ready_tp_channel_cb (TpChannel *channel,
    const GError *error, gpointer user_data);

G_DEFINE_ABSTRACT_TYPE (TplChannel, tpl_channel, TP_TYPE_CHANNEL)

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
tpl_channel_get_property (GObject *object,
    guint param_id,
    GValue *value,
    GParamSpec *pspec)
{
  TplChannelPriv *priv = TPL_CHANNEL (object)->priv;

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
tpl_channel_set_property (GObject *object,
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
  TplChannelPriv *priv = TPL_CHANNEL (obj)->priv;

  if (priv->account != NULL)
    {
      g_object_unref (priv->account);
      priv->account = NULL;
    }

  G_OBJECT_CLASS (tpl_channel_parent_class)->dispose (obj);
}


static void
tpl_channel_class_init (TplChannelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GParamSpec *param_spec;

  object_class->dispose = tpl_channel_dispose;
  object_class->get_property = tpl_channel_get_property;
  object_class->set_property = tpl_channel_set_property;

  klass->call_when_ready_protected = call_when_ready_protected;

  /**
   * TplChannel:account:
   *
   * the TpAccount instance associated with TplChannel
   */
  param_spec = g_param_spec_object ("account",
      "Account", "TpAccount instance associated with TplChannel",
      TP_TYPE_ACCOUNT, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
      G_PARAM_STATIC_STRINGS);
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
  TplChannelPriv *priv;

  g_return_val_if_fail (TPL_IS_CHANNEL (self), NULL);

  priv = self->priv;

  return priv->account;
}


static void
tpl_channel_set_account (TplChannel *self,
    TpAccount *data)
{
  TplChannelPriv *priv;

  g_return_if_fail (TPL_IS_CHANNEL (self));
  g_return_if_fail (TP_IS_ACCOUNT (data));

  priv = self->priv;
  g_return_if_fail (priv->account == NULL);

  priv->account = g_object_ref (data);
}


/**
 * tpl_channel_call_when_ready
 * @self: a TplChannel instance
 * @cb: a callback
 * @user_data: user's data passed to the callback
 *
 * The TplObserver has no idea of what TplChannel subclass intance it's
 * dealing with.
 * In order to prepare the subclass instance this method has to
 * be called, which will call #TplChannelClass.call_when_ready
 * virtual method, implemented (mandatory) by #TplChannel subclasses.
 * Such method has to call, internally,
 * #TplChannelClass.call_when_ready_protected  in order to prepare also the
 * #TplChannel instance.
 */
void
tpl_channel_call_when_ready (TplChannel *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  g_return_if_fail (TPL_IS_CHANNEL (self));
  /* Subclasses have to implement it */
  g_return_if_fail (TPL_CHANNEL_GET_CLASS (self)->call_when_ready != NULL);

  TPL_CHANNEL_GET_CLASS (self)->call_when_ready (self, cb, user_data);
}


/**
 * call_when_ready_protected
 * @self: a TplChannel instance
 * @cb: a callback
 * @user_data: user's data passed to the callback
 *
 * This static method is called from #TplChannelClass.call_when_ready, implemented by
 * #TplChannel subclasses.
 *
 * See also: tpl_channel_call_when_ready()
 */
static void
call_when_ready_protected (TplChannel *self,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  TplActionChain *actions;

  actions = _tpl_action_chain_new_async (G_OBJECT (self), cb, user_data);
  _tpl_action_chain_append (actions, pendingproc_get_ready_tp_connection, NULL);
  _tpl_action_chain_append (actions, pendingproc_get_ready_tp_channel, NULL);
  _tpl_action_chain_continue (actions);
}


static void
pendingproc_get_ready_tp_connection (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *tpl_chan = _tpl_action_chain_get_object (ctx);
  TpConnection *tp_conn = tp_channel_borrow_connection (TP_CHANNEL (
      tpl_chan));

  tp_connection_call_when_ready (tp_conn, got_ready_tp_connection_cb, ctx);
}

static void
got_ready_tp_connection_cb (TpConnection *connection,
    const GError *error,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;

  if (error != NULL)
    {
      const gchar *chan_path;
      TplChannel *tpl_chan;

      tpl_chan = _tpl_action_chain_get_object (ctx);
      chan_path = tp_proxy_get_object_path (TP_PROXY (tpl_chan));
      PATH_DEBUG (tpl_chan, "Giving up channel observation: %s",
          error->message);

      _tpl_action_chain_terminate (ctx);
      return;
    }

  _tpl_action_chain_continue (ctx);
}

static void
pendingproc_get_ready_tp_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *tpl_chan = _tpl_action_chain_get_object (ctx);

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
  TplChannel *tpl_chan = _tpl_action_chain_get_object (ctx);

  if (error != NULL)
    {
      PATH_DEBUG (tpl_chan, "Giving up channel observation: %s",
          error->message);

      _tpl_action_chain_terminate (ctx);
      return;
    }

  _tpl_action_chain_continue (ctx);
}
