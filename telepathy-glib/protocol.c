/* TpProtocol
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
 * SECTION:protocol
 * @title: TpProtocol
 * @short_description: proxy for a Telepathy Protocol object
 * @see_also: #TpConnectionManager
 *
 * #TpProtocol objects represent the protocols implemented by Telepathy
 * connection managers. In modern connection managers, each protocol is
 * represented by a D-Bus object; in older connection managers, the protocols
 * are represented by data structures, and this object merely emulates a D-Bus
 * object.
 */

#include <telepathy-glib/protocol.h>
#include <telepathy-glib/protocol-internal.h>

#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/telepathy-glib.h>

#define DEBUG_FLAG TP_DEBUG_PARAMS
#include "telepathy-glib/debug-internal.h"

#include "telepathy-glib/_gen/signals-marshal.h"
#include "telepathy-glib/_gen/tp-cli-protocol.h"
#include "telepathy-glib/_gen/tp-cli-protocol-body.h"

struct _TpProtocolClass
{
  /*<private>*/
  TpProxyClass parent_class;
};

/**
 * TpProtocol:
 *
 * A base class for connection managers' protocols.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpProtocolClass:
 *
 * The class of a #TpProtocol.
 *
 * Since: 0.11.UNRELEASED
 */

G_DEFINE_TYPE(TpProtocol, tp_protocol, TP_TYPE_PROXY);

struct _TpProtocolPrivate
{
  TpConnectionManagerProtocol protocol_struct;
};

enum
{
    PROP_PROTOCOL_NAME = 1,
    N_PROPS
};

static void
tp_protocol_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpProtocol *self = (TpProtocol *) object;

  switch (property_id)
    {
    case PROP_PROTOCOL_NAME:
      g_value_set_string (value, self->priv->protocol_struct.name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_protocol_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpProtocol *self = (TpProtocol *) object;

  switch (property_id)
    {
    case PROP_PROTOCOL_NAME:
      g_assert (self->priv->protocol_struct.name == NULL);
      self->priv->protocol_struct.name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
_tp_connection_manager_param_free_contents (TpConnectionManagerParam *param)
{
  g_free (param->name);
  g_free (param->dbus_signature);

  if (G_IS_VALUE (&param->default_value))
    g_value_unset (&param->default_value);
}

void
_tp_connection_manager_protocol_free_contents (
    TpConnectionManagerProtocol *proto)
{
  g_free (proto->name);

  if (proto->params != NULL)
    {
      TpConnectionManagerParam *param;

      for (param = proto->params; param->name != NULL; param++)
        _tp_connection_manager_param_free_contents (param);
    }

  g_free (proto->params);
}

static void
tp_protocol_finalize (GObject *object)
{
  TpProtocol *self = TP_PROTOCOL (object);
  GObjectFinalizeFunc finalize =
    ((GObjectClass *) tp_protocol_parent_class)->finalize;

  _tp_connection_manager_protocol_free_contents (&self->priv->protocol_struct);

  if (finalize != NULL)
    finalize (object);
}

static void
tp_protocol_class_init (TpProtocolClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpProtocolPrivate));

  object_class->get_property = tp_protocol_get_property;
  object_class->set_property = tp_protocol_set_property;
  object_class->finalize = tp_protocol_finalize;

  g_object_class_install_property (object_class, PROP_PROTOCOL_NAME,
      g_param_spec_string ("protocol-name",
        "Name of this protocol",
        "The Protocol from telepathy-spec, such as 'jabber' or 'local-xmpp'",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  proxy_class->must_have_unique_name = FALSE;
  proxy_class->interface = TP_IFACE_QUARK_PROTOCOL;
  tp_protocol_init_known_interfaces ();
}

static void
tp_protocol_init (TpProtocol *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_PROTOCOL,
      TpProtocolPrivate);
}

/**
 * tp_protocol_new:
 * @dbus: proxy for the D-Bus daemon; may not be %NULL
 * @cm_name: the connection manager name (such as "gabble")
 * @protocol_name: the protocol name (such as "jabber")
 * @immutable_properties: the immutable D-Bus properties for this protocol,
 *  if available (currently unused)
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new protocol proxy, or %NULL on invalid arguments
 *
 * Since: 0.11.UNRELEASED
 */
TpProtocol *
tp_protocol_new (TpDBusDaemon *dbus,
    const gchar *cm_name,
    const gchar *protocol_name,
    const GHashTable *immutable_properties G_GNUC_UNUSED,
    GError **error)
{
  TpProtocol *ret = NULL;
  gchar *bus_name = NULL;
  gchar *object_path = NULL;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (dbus), NULL);

  if (!tp_connection_manager_check_valid_protocol_name (protocol_name, error))
    goto finally;

  if (!tp_connection_manager_check_valid_name (cm_name, error))
    goto finally;

  bus_name = g_strdup_printf ("%s%s", TP_CM_BUS_NAME_BASE, cm_name);
  object_path = g_strdup_printf ("%s%s/%s", TP_CM_OBJECT_PATH_BASE, cm_name,
      protocol_name);
  /* e.g. local-xmpp -> local_xmpp */
  g_strdelimit (object_path, "-", '_');

  ret = TP_PROTOCOL (g_object_new (TP_TYPE_PROTOCOL,
        "dbus-daemon", dbus,
        "bus-name", bus_name,
        "object-path", object_path,
        "protocol-name", protocol_name,
        NULL));

finally:
  g_free (bus_name);
  g_free (object_path);
  return ret;
}

/**
 * tp_protocol_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpProtocol have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_PROTOCOL.
 *
 * Since: 0.11.UNRELEASED
 */
void
tp_protocol_init_known_interfaces (void)
{
  static gsize once = 0;

  if (g_once_init_enter (&once))
    {
      GType type = TP_TYPE_PROTOCOL;

      tp_proxy_init_known_interfaces ();

      tp_proxy_or_subclass_hook_on_interface_add (type,
          tp_cli_protocol_add_signals);
      tp_proxy_subclass_add_error_mapping (type,
          TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

      g_once_init_leave (&once, 1);
    }
}

TpConnectionManagerProtocol *
_tp_protocol_get_struct (TpProtocol *self)
{
  return &self->priv->protocol_struct;
}

/**
 * tp_protocol_get_name:
 * @self: a protocol object
 *
 * Return the same thing as the protocol-name property, for convenient use
 * in C code. The returned string is valid for as long as @self exists.
 *
 * Returns: the value of the #TpProtocol:protocol-name property
 */
const gchar *
tp_protocol_get_name (TpProtocol *self)
{
  g_return_val_if_fail (TP_IS_PROTOCOL (self), NULL);
  return self->priv->protocol_struct.name;
}
