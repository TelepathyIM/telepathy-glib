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
#include "channel-internal.h"

#include <string.h>

#include <glib.h>
#include <telepathy-glib/util.h>

#include <telepathy-logger/channel-text-internal.h>
#include <telepathy-logger/observer-internal.h>

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
static void pendingproc_get_ready_tp_channel (TplActionChain *ctx,
    gpointer user_data);

G_DEFINE_ABSTRACT_TYPE (TplChannel, _tpl_channel, TP_TYPE_CHANNEL)

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

  G_OBJECT_CLASS (_tpl_channel_parent_class)->dispose (obj);
}


static void
_tpl_channel_class_init (TplChannelClass *klass)
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
_tpl_channel_init (TplChannel *self)
{
  TplChannelPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TPL_TYPE_CHANNEL,
      TplChannelPriv);
  self->priv = priv;
}


TpAccount *
_tpl_channel_get_account (TplChannel *self)
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
 * The TplObserver has no idea of what TplChannel subclass instance it's
 * dealing with.
 * In order to prepare the subclass instance this method has to
 * be called, which will call #TplChannelClass.call_when_ready
 * virtual method, implemented (mandatory) by #TplChannel subclasses.
 * Such method has to call, internally,
 * #TplChannelClass.call_when_ready_protected  in order to prepare also the
 * #TplChannel instance.
 */
void
_tpl_channel_call_when_ready (TplChannel *self,
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
 * See also: %tpl_channel_call_when_ready
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
conn_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      TplChannel *tpl_chan;

      tpl_chan = _tpl_action_chain_get_object (ctx);
      PATH_DEBUG (tpl_chan, "Giving up channel observation: %s",
          error->message);

      _tpl_action_chain_terminate (ctx);
      g_error_free (error);
      return;
    }

  _tpl_action_chain_continue (ctx);
}

static void
pendingproc_get_ready_tp_connection (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *tpl_chan = _tpl_action_chain_get_object (ctx);
  TpConnection *tp_conn = tp_channel_borrow_connection (TP_CHANNEL (
      tpl_chan));
  GQuark features[] = { TP_CONNECTION_FEATURE_CORE, 0 };

  tp_proxy_prepare_async (tp_conn, features, conn_prepared_cb, ctx);
}

static void
channel_prepared_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  TplChannel *tpl_chan = _tpl_action_chain_get_object (ctx);
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (source, result, &error))
    {
      PATH_DEBUG (tpl_chan, "Giving up channel observation: %s",
          error->message);

      _tpl_action_chain_terminate (ctx);
      g_error_free (error);
      return;

    }

  _tpl_action_chain_continue (ctx);
}

static void
pendingproc_get_ready_tp_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *tpl_chan = _tpl_action_chain_get_object (ctx);
  GQuark features[] = { TP_CHANNEL_FEATURE_CORE, TP_CHANNEL_FEATURE_GROUP, 0 };

  /* user_data is a TplChannel instance */
  tp_proxy_prepare_async (TP_CHANNEL (tpl_chan), features, channel_prepared_cb,
      ctx);
}
