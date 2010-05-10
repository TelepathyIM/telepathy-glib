/* ContactList channel manager
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

#include <config.h>
#include <telepathy-glib/contact-list-manager.h>

/**
 * SECTION:contact-list-manager
 * @title: TpContactListManager
 * @short_description: channel manager for ContactList channels
 *
 * This class represents a connection's contact list (roster, buddy list etc.)
 * inside a connection manager. It can be used to implement the ContactList
 * D-Bus interface on the Connection.
 *
 * In versions of the Telepathy D-Bus Interface Specification prior to
 * 0.19.UNRELEASED, this functionality was provided as a collection of
 * individual ContactList channels. As a result, this object also implements
 * the #TpChannelManager interface, so that it can provide those channels.
 * The channel objects are internal to this object, and not considered to be
 * part of the API.
 */

/**
 * TpContactListManager:
 *
 * A connection's contact list (roster, buddy list) inside a connection
 * manager. Each #TpBaseConnection may have at most one #TpContactListManager.
 *
 * (FIXME: fill in more docs)
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpContactListManagerClass:
 *
 * The class of a #TpContactListManager.
 *
 * Since: 0.11.UNRELEASED
 */

#include <telepathy-glib/base-connection.h>

#define DEBUG_FLAG TP_DEBUG_CONTACT_LISTS
#include "telepathy-glib/debug-internal.h"

struct _TpContactListManagerPrivate
{
  TpBaseConnection *conn;
};

struct _TpContactListManagerClassPrivate
{
  char dummy;
};

static void channel_manager_iface_init (TpChannelManagerIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (TpContactListManager,
    tp_contact_list_manager,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_MANAGER,
      channel_manager_iface_init);
    g_type_add_class_private (g_define_type_id, sizeof (
        TpContactListManagerClassPrivate)))

enum {
    PROP_CONNECTION = 1,
    N_PROPS
};

static void
tp_contact_list_manager_init (TpContactListManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CONTACT_LIST_MANAGER,
      TpContactListManagerPrivate);
}

static void
tp_contact_list_manager_dispose (GObject *object)
{
  TpContactListManager *self = TP_CONTACT_LIST_MANAGER (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_contact_list_manager_parent_class)->dispose;

  if (self->priv->conn != NULL)
    {
      g_object_unref (self->priv->conn);
      self->priv->conn = NULL;
    }

  if (dispose != NULL)
    dispose (object);
}

static void
tp_contact_list_manager_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpContactListManager *self = TP_CONTACT_LIST_MANAGER (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, self->priv->conn);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_contact_list_manager_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpContactListManager *self = TP_CONTACT_LIST_MANAGER (object);

  switch (property_id)
    {
    case PROP_CONNECTION:
      g_assert (self->priv->conn == NULL);    /* construct-only */

      /* We don't ref the connection, because it owns a reference to the
       * manager, and it guarantees that the manager's lifetime is
       * less than its lifetime.
       *
       * FIXME: clean this up - weak ref? strong ref until disconnected? */
      self->priv->conn = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_contact_list_manager_constructed (GObject *object)
{
  TpContactListManager *self = TP_CONTACT_LIST_MANAGER (object);
  void (*chain_up) (GObject *) =
    G_OBJECT_CLASS (tp_contact_list_manager_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->conn != NULL);
}

static void
tp_contact_list_manager_class_init (TpContactListManagerClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);

  g_type_class_add_private (cls, sizeof (TpContactListManagerPrivate));

  cls->priv = G_TYPE_CLASS_GET_PRIVATE (cls, TP_TYPE_CONTACT_LIST_MANAGER,
      TpContactListManagerClassPrivate);

  object_class->get_property = tp_contact_list_manager_get_property;
  object_class->set_property = tp_contact_list_manager_set_property;
  object_class->constructed = tp_contact_list_manager_constructed;
  object_class->dispose = tp_contact_list_manager_dispose;

  /**
   * TpContactListManager:connection:
   *
   * The connection that owns this channel manager.
   * Read-only except during construction.
   *
   * Since: 0.11.UNRELEASED
   */
  g_object_class_install_property (object_class, PROP_CONNECTION,
      g_param_spec_object ("connection", "Connection",
        "The connection that owns this channel manager",
        TP_TYPE_BASE_CONNECTION,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
tp_contact_list_manager_foreach_channel (TpChannelManager *manager,
    TpExportableChannelFunc func,
    gpointer user_data)
{
  /* FIXME: stub */
}

static void
tp_contact_list_manager_foreach_channel_class (TpChannelManager *manager,
    TpChannelManagerChannelClassFunc func,
    gpointer user_data)
{
  /* FIXME: stub */
}

static gboolean
tp_contact_list_manager_request_helper (TpChannelManager *manager,
    gpointer request_token,
    GHashTable *request_properties,
    gboolean is_create)
{
  TpContactListManager *self = (TpContactListManager *) manager;

  g_return_val_if_fail (TP_IS_CONTACT_LIST_MANAGER (self), FALSE);

  /* FIXME: stub */
  return FALSE;
}

static gboolean
tp_contact_list_manager_create_channel (TpChannelManager *manager,
    gpointer request_token,
    GHashTable *request_properties)
{
  return tp_contact_list_manager_request_helper (manager, request_token,
      request_properties, TRUE);
}

static gboolean
tp_contact_list_manager_ensure_channel (TpChannelManager *manager,
    gpointer request_token,
    GHashTable *request_properties)
{
  return tp_contact_list_manager_request_helper (manager, request_token,
      request_properties, FALSE);
}

static void
channel_manager_iface_init (TpChannelManagerIface *iface)
{
  iface->foreach_channel = tp_contact_list_manager_foreach_channel;
  iface->foreach_channel_class =
      tp_contact_list_manager_foreach_channel_class;
  iface->create_channel = tp_contact_list_manager_create_channel;
  iface->ensure_channel = tp_contact_list_manager_ensure_channel;
  /* In this channel manager, Request has the same semantics as Ensure */
  iface->request_channel = tp_contact_list_manager_ensure_channel;
}
