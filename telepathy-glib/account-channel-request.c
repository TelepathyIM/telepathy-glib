/*
 * object used to request a channel from a TpAccount
 *
 * Copyright © 2010 Collabora Ltd.
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
 */

/**
 * SECTION: account-channel-request
 * @title: TpAccountChannelRequest
 * @short_description: Object used to request a channel from a #TpAccount
 *
 * A #TpAccountChannelRequest object is used to request a channel using the
 * ChannelDispatcher. Once created, use one of the create or ensure async
 * method to actually request the channel.
 *
 * Note that each #TpAccountChannelRequest object can only be used to create
 * one channel. You can't call a create or ensure method more than once on the
 * same #TpAccountChannelRequest.
 *
 * Once the channel has been created you can use the
 * TpAccountChannelRequest::re-handled: signal to be notified when the channel
 * has to be re-handled. This can be useful for example to move its window
 * to the foreground, if applicable.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpAccountChannelRequest:
 *
 * Data structure representing a #TpAccountChannelRequest object.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpAccountChannelRequestClass:
 *
 * The class of a #TpAccountChannelRequest.
 *
 * Since: 0.11.UNRELEASED
 */

#include "telepathy-glib/account-channel-request.h"
#include "telepathy-glib/account-channel-request-internal.h"

#include <telepathy-glib/channel.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CLIENT
#include "telepathy-glib/debug-internal.h"
#include "telepathy-glib/_gen/signals-marshal.h"

struct _TpAccountChannelRequestClass {
    /*<private>*/
    GObjectClass parent_class;
};

G_DEFINE_TYPE(TpAccountChannelRequest,
    tp_account_channel_request, G_TYPE_OBJECT)

enum {
    PROP_ACCOUNT = 1,
    PROP_REQUEST,
    PROP_USER_ACTION_TIME,
    N_PROPS
};

/*
enum {
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };
*/

struct _TpAccountChannelRequestPrivate
{
  TpAccount *account;
  GHashTable *request;
  gint64 user_action_time;
};

static void
tp_account_channel_request_init (TpAccountChannelRequest *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_ACCOUNT_CHANNEL_REQUEST,
      TpAccountChannelRequestPrivate);
}

static void
tp_account_channel_request_dispose (GObject *object)
{
  TpAccountChannelRequest *self = TP_ACCOUNT_CHANNEL_REQUEST (
      object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_account_channel_request_parent_class)->dispose;


  tp_clear_object (&self->priv->account);
  tp_clear_pointer (&self->priv->request, g_hash_table_unref);

  if (dispose != NULL)
    dispose (object);
}

static void
tp_account_channel_request_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpAccountChannelRequest *self = TP_ACCOUNT_CHANNEL_REQUEST (
      object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        g_value_set_object (value, self->priv->account);
        break;

      case PROP_REQUEST:
        g_value_set_object (value, self->priv->request);
        break;

      case PROP_USER_ACTION_TIME:
        g_value_set_int64 (value, self->priv->user_action_time);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_account_channel_request_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpAccountChannelRequest *self = TP_ACCOUNT_CHANNEL_REQUEST (
      object);

  switch (property_id)
    {
      case PROP_ACCOUNT:
        self->priv->account = g_value_dup_object (value);
        break;

      case PROP_REQUEST:
        self->priv->request = g_value_dup_boxed (value);
        break;

      case PROP_USER_ACTION_TIME:
        self->priv->user_action_time = g_value_get_int64 (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_account_channel_request_constructed (GObject *object)
{
  TpAccountChannelRequest *self = TP_ACCOUNT_CHANNEL_REQUEST (
      object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *)
      tp_account_channel_request_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->account != NULL);
  g_assert (self->priv->request != NULL);
}

static void
tp_account_channel_request_class_init (
    TpAccountChannelRequestClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (TpAccountChannelRequestPrivate));

  object_class->get_property = tp_account_channel_request_get_property;
  object_class->set_property = tp_account_channel_request_set_property;
  object_class->constructed = tp_account_channel_request_constructed;
  object_class->dispose = tp_account_channel_request_dispose;

  /**
   * TpAccountChannelRequest:account:
   *
   * The #TpAccount used to request the channel.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_object ("account", "TpAccount",
      "The TpAccount used to request the channel",
      TP_TYPE_ACCOUNT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT,
      param_spec);

  /**
   * TpAccountChannelRequest:request:
   *
   * The #TpAccount used to request the channel.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_boxed ("request", "GHashTable",
      "A dictionary containing desirable properties for the channel",
      TP_HASH_TYPE_STRING_VARIANT_MAP,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUEST,
      param_spec);

  /**
   * TpAccountChannelRequest:user-action-time:
   *
   * The user action time that will be passed to mission-control when
   * requesting the channel.
   *
   * This property can't be %NULL.
   *
   * Since: 0.11.UNRELEASED
   */
  param_spec = g_param_spec_int64 ("user-action-time", "user action time",
      "UserActionTime",
      G_MININT64, G_MAXINT64, 0,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_USER_ACTION_TIME,
      param_spec);
}

/**
 * tp_account_channel_request_new:
 * @account: a #TpAccount
 * @request: (transfer none) (element-type utf8 GObject.Value): the requested
 * properties of the channel
 * @user_action_time: the user action time to pass to the channel dispatcher
 * when requesting the channel
 *
 * Convenience function to create a new #TpAccountChannelRequest object.
 *
 * Returns: a new #TpAccountChannelRequest object
 *
 * Since: 0.11.UNRELEASED
 */
TpAccountChannelRequest *
tp_account_channel_request_new (
    TpAccount *account,
    GHashTable *request,
    gint64 user_action_time)
{
  return g_object_new (TP_TYPE_ACCOUNT_CHANNEL_REQUEST,
      "account", account,
      "request", request,
      "user-action-time", user_action_time,
      NULL);
}

/**
 * tp_account_channel_request_get_account:
 * @self: a #TpAccountChannelRequest
 *
 * Return the #TpAccountChannelRequest:account construct-only property
 *
 * Returns: the value of #TpAccountChannelRequest:account
 *
 * Since: 0.11.UNRELEASED
 */
TpAccount *
tp_account_channel_request_get_account (
    TpAccountChannelRequest *self)
{
  return self->priv->account;
}

/**
 * tp_account_channel_request_get_request:
 * @self: a #TpAccountChannelRequest
 *
 * Return the #TpAccountChannelRequest:request construct-only property
 *
 * Returns: the value of #TpAccountChannelRequest:request
 *
 * Since: 0.11.UNRELEASED
 */
GHashTable *
tp_account_channel_request_get_request (
    TpAccountChannelRequest *self)
{
  return self->priv->request;
}

/**
 * tp_account_channel_request_get_user_action_time:
 * @self: a #TpAccountChannelRequest
 *
 * Return the #TpAccountChannelRequest:user-action-time construct-only property
 *
 * Returns: the value of #TpAccountChannelRequest:user-action-time
 *
 * Since: 0.11.UNRELEASED
 */
gint64
tp_account_channel_request_get_user_action_time (
    TpAccountChannelRequest *self)
{
  return self->priv->user_action_time;
}
