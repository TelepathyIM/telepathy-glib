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

/*
 * This object acts as a Text Channel context, handling a automaton to
 * set up all the needed information before connect to Text iface
 * signals.
 */

#include "tpl-channel-test.h"

#include <telepathy-logger/action-chain.h>
#include <telepathy-logger/channel.h>

static void call_when_ready_wrapper (TplChannel *tpl_chan, GAsyncReadyCallback
    cb, gpointer user_data);
static void pendingproc_prepare_tpl_channel (TplActionChain *ctx,
    gpointer user_data);
static void got_tpl_chan_ready_cb (GObject *obj, GAsyncResult *result,
    gpointer user_data);


#define GET_PRIV(obj)    TPL_GET_PRIV (obj, TplChannelTest)
struct _TplChannelTestPriv
{
  gpointer nonempty;
};

G_DEFINE_TYPE (TplChannelTest, tpl_channel_test, TPL_TYPE_CHANNEL)

static void
dispose (GObject *obj)
{
  G_OBJECT_CLASS (tpl_channel_test_parent_class)->dispose (obj);
}

static void
finalize (GObject *obj)
{
  G_OBJECT_CLASS (tpl_channel_test_parent_class)->finalize (obj);
}


static void
tpl_channel_test_class_init (TplChannelTestClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  TplChannelClass *tpl_chan_class = TPL_CHANNEL_CLASS (klass);

  object_class->dispose = dispose;
  object_class->finalize = finalize;

  tpl_chan_class->call_when_ready = call_when_ready_wrapper;

  g_type_class_add_private (object_class, sizeof (TplChannelTestPriv));
}


static void
tpl_channel_test_init (TplChannelTest *self)
{
  TplChannelTestPriv *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TPL_TYPE_CHANNEL_TEST, TplChannelTestPriv);
  self->priv = priv;
}


/**
 * @conn: TpConnection instance owning the channel
 * @object_path: the channel's DBus path
 * @tp_chan_props: channel's immutable properties, obtained for example by
 * %tp_channel_borrow_immutable_properties()
 * @error: location of the GError, used in case a problem is raised while
 * creating the channel
 *
 * Convenience function to create a new TPL Channel Text proxy. The returned
 * #TplChannelTest is not guaranteed to be ready at the point of return. Use #TpChannel
 * methods casting the #TplChannelTest instance to a TpChannel
 *
 * TplChannelTest instances are subclasses or the abstract TplChannel which is
 * subclass of TpChannel.
 *
 * Returns: the TplChannelTest instance or %NULL in @object_path is not valid
 */
TplChannelTest *
tpl_channel_test_new (TpConnection *conn,
    const gchar *object_path,
    GHashTable *tp_chan_props,
    TpAccount *account,
    GError **error)
{
  return g_object_new (TPL_TYPE_CHANNEL_TEST,
      "account", account,
      NULL);
}


static void
call_when_ready_wrapper (TplChannel *tpl_chan,
    GAsyncReadyCallback cb,
    gpointer user_data)
{
  tpl_channel_test_call_when_ready (TPL_CHANNEL_TEST (tpl_chan), cb, user_data);
}

void
tpl_channel_test_call_when_ready (TplChannelTest *self,
    GAsyncReadyCallback cb, gpointer user_data)
{
  TplActionChain *actions;

  /* first: connect signals, so none are lost
   * second: prepare all TplChannel
   * then: us TpContact to cache my contact and the remote one.
   * If for any reason, the order is changed, it's need to check what objects
   * are unreferenced by g_object_unref: after the order change, it might
   * happend that an object still has to be created after the change */
  actions = tpl_action_chain_new (G_OBJECT (self), cb, user_data);
  tpl_action_chain_append (actions, pendingproc_prepare_tpl_channel, NULL);
  /* start the queue consuming */
  tpl_action_chain_continue (actions);
}


static void
pendingproc_prepare_tpl_channel (TplActionChain *ctx,
    gpointer user_data)
{
  TplChannel *tpl_chan = TPL_CHANNEL (tpl_action_chain_get_object (ctx));

  g_debug ("prepare tpl");
  TPL_CHANNEL_GET_CLASS (tpl_chan)->call_when_ready_protected (tpl_chan,
      got_tpl_chan_ready_cb, ctx);
}


static void
got_tpl_chan_ready_cb (GObject *obj,
    GAsyncResult *result,
    gpointer user_data)
{
  TplActionChain *ctx = user_data;
  g_debug ("PREPARE");

  if (tpl_action_chain_finish (result) == TRUE)
    tpl_action_chain_continue (ctx);
  return;
}
