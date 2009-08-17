/*
 * connection.c - proxy for a Telepathy connection
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
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

#include "telepathy-glib/connection.h"

#include <string.h>

#include <telepathy-glib/connection-manager.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/errors.h>
#include <telepathy-glib/handle.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "telepathy-glib/connection-internal.h"
#include "telepathy-glib/dbus-internal.h"
#include "telepathy-glib/debug-internal.h"

#include "_gen/tp-cli-connection-body.h"

/**
 * SECTION:connection
 * @title: TpConnection
 * @short_description: proxy object for a Telepathy connection
 * @see_also: #TpConnectionManager, #TpChannel
 *
 * #TpConnection objects represent Telepathy instant messaging connections
 * accessed via D-Bus.
 *
 * Compared with a simple proxy for method calls, they add the following
 * features:
 *
 * <itemizedlist>
 * <listitem>connection status tracking</listitem>
 * <listitem>calling GetInterfaces() automatically</listitem>
 * </itemizedlist>
 *
 * Since: 0.7.1
 */

/**
 * TP_ERRORS_DISCONNECTED:
 *
 * #GError domain representing a Telepathy connection becoming disconnected.
 * The @code in a #GError with this domain must be a member of
 * #TpConnectionStatusReason.
 *
 * This macro expands to a function call returning a #GQuark.
 *
 * Since: 0.7.1
 */
GQuark
tp_errors_disconnected_quark (void)
{
  static GQuark q = 0;

  if (q == 0)
    q = g_quark_from_static_string ("tp_errors_disconnected_quark");

  return q;
}

/**
 * TP_UNKNOWN_CONNECTION_STATUS:
 *
 * An invalid connection status used in #TpConnection to indicate that the
 * status has not yet been discovered.
 *
 * Since: 0.7.1
 */

/**
 * TpConnectionClass:
 * @parent_class: the parent class
 *
 * The class of a #TpConnection. In addition to @parent_class there are four
 * pointers reserved for possible future use.
 *
 * (Changed in 0.7.12: the layout of the structure is visible, allowing
 * subclassing.)
 *
 * Since: 0.7.1
 */

/**
 * TpConnection:
 * @parent: the parent class instance
 * @priv: pointer to opaque private data
 *
 * A proxy object for a Telepathy connection.
 *
 * (Changed in 0.7.12: the layout of the structure is visible, allowing
 * subclassing.)
 *
 * Since: 0.7.1
 */

enum
{
  PROP_STATUS = 1,
  PROP_STATUS_REASON,
  PROP_CONNECTION_READY,
  PROP_SELF_HANDLE,
  N_PROPS
};

G_DEFINE_TYPE (TpConnection,
    tp_connection,
    TP_TYPE_PROXY);

static void
tp_connection_get_property (GObject *object,
                            guint property_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  TpConnection *self = TP_CONNECTION (object);

  switch (property_id)
    {
    case PROP_CONNECTION_READY:
      g_value_set_boolean (value, self->priv->ready);
      break;
    case PROP_STATUS:
      g_value_set_uint (value, self->priv->status);
      break;
    case PROP_STATUS_REASON:
      g_value_set_uint (value, self->priv->status_reason);
      break;
    case PROP_SELF_HANDLE:
      g_value_set_uint (value, self->priv->self_handle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
tp_connection_continue_introspection (TpConnection *self)
{
  g_assert (self->priv->introspect_needed != NULL);

  if (self->priv->introspect_needed->len == 0)
    {
      g_array_free (self->priv->introspect_needed, TRUE);
      self->priv->introspect_needed = NULL;

      DEBUG ("%p: connection ready", self);
      self->priv->ready = TRUE;
      g_object_notify ((GObject *) self, "connection-ready");
    }
  else
    {
      guint i = self->priv->introspect_needed->len - 1;
      TpConnectionProc next = g_array_index (self->priv->introspect_needed,
          TpConnectionProc, i);

      g_array_remove_index (self->priv->introspect_needed, i);
      next (self);
    }
}

static void
got_contact_attribute_interfaces (TpProxy *proxy,
                                  const GValue *value,
                                  const GError *error,
                                  gpointer user_data G_GNUC_UNUSED,
                                  GObject *weak_object G_GNUC_UNUSED)
{
  TpConnection *self = TP_CONNECTION (proxy);

  if (error == NULL)
    {
      if (G_VALUE_HOLDS (value, G_TYPE_STRV))
        {
          GArray *arr;
          gchar **interfaces = g_value_get_boxed (value);
          gchar **iter;

          arr = g_array_sized_new (FALSE, FALSE, sizeof (GQuark),
              interfaces == NULL ? 0 : g_strv_length (interfaces));

          if (interfaces != NULL)
            {
              for (iter = interfaces; *iter != NULL; iter++)
                {
                  if (tp_dbus_check_valid_interface_name (*iter, NULL))
                    {
                      GQuark q = g_quark_from_string (*iter);

                      DEBUG ("%p: ContactAttributeInterfaces has %s", self,
                          *iter);
                      g_array_append_val (arr, q);
                    }
                  else
                    {
                      DEBUG ("%p: ignoring invalid interface: %s", self,
                          *iter);
                    }
                }
            }

          g_assert (self->priv->contact_attribute_interfaces == NULL);
          self->priv->contact_attribute_interfaces = arr;
        }
      else
        {
          DEBUG ("%p: ContactAttributeInterfaces had wrong type %s, "
              "ignoring", self, G_VALUE_TYPE_NAME (value));
        }
    }
  else
    {
      DEBUG ("%p: Get(Contacts, ContactAttributeInterfaces) failed with "
          "%s %d: %s", self, g_quark_to_string (error->domain), error->code,
          error->message);
    }

  tp_connection_continue_introspection (self);
}

static void
introspect_contacts (TpConnection *self)
{
  g_assert (self->priv->introspect_needed != NULL);

  tp_cli_dbus_properties_call_get (self, -1,
       TP_IFACE_CONNECTION_INTERFACE_CONTACTS, "ContactAttributeInterfaces",
       got_contact_attribute_interfaces, NULL, NULL, NULL);
}

static void
_tp_connection_set_self_handle (TpConnection *self,
                 guint self_handle)
{
  if (self_handle != self->priv->self_handle)
    {
      self->priv->self_handle = self_handle;
      g_object_notify ((GObject *) self, "self-handle");
    }
}

static void
got_self_handle (TpConnection *self,
                 guint self_handle,
                 const GError *error,
                 gpointer user_data G_GNUC_UNUSED,
                 GObject *user_object G_GNUC_UNUSED)
{

  if (error != NULL)
    {
      DEBUG ("%p: GetSelfHandle() failed: %s", self, error->message);
      self_handle = 0;
      /* FIXME: abort the readying process */
    }

  _tp_connection_set_self_handle (self, self_handle);
  tp_connection_continue_introspection (self);
}

static void
on_self_handle_changed (TpConnection *self,
                        guint self_handle,
                        gpointer user_data G_GNUC_UNUSED,
                        GObject *user_object G_GNUC_UNUSED)
{
  _tp_connection_set_self_handle (self, self_handle);
}

static void
get_self_handle (TpConnection *self)
{
  g_assert (self->priv->introspect_needed != NULL);

  tp_cli_connection_connect_to_self_handle_changed (self,
      on_self_handle_changed, NULL, NULL, NULL, NULL);

  /* GetSelfHandle is deprecated in favour of the SelfHandle property,
   * but until Connection has other interesting properties, there's no point in
   * trying to implement a fast path; GetSelfHandle is the only one guaranteed
   * to work, so we'll sometimes have to call it anyway */
  tp_cli_connection_call_get_self_handle (self, -1,
       got_self_handle, NULL, NULL, NULL);
}

static void
tp_connection_got_interfaces_cb (TpConnection *self,
                                 const gchar **interfaces,
                                 const GError *error,
                                 gpointer user_data,
                                 GObject *user_object)
{
  TpConnectionProc func;

  if (error != NULL)
    {
      DEBUG ("%p: GetInterfaces() failed, assuming no interfaces: %s",
          self, error->message);
      interfaces = NULL;
    }

  DEBUG ("%p: Introspected interfaces", self);

  if (tp_proxy_get_invalidated (self) != NULL)
    {
      DEBUG ("%p: already invalidated, not trying to become ready: %s",
          self, tp_proxy_get_invalidated (self)->message);
      return;
    }

  g_assert (self->priv->introspect_needed == NULL);
  self->priv->introspect_needed = g_array_new (FALSE, FALSE,
      sizeof (TpConnectionProc));

  func = get_self_handle;
  g_array_append_val (self->priv->introspect_needed, func);

  if (interfaces != NULL)
    {
      const gchar **iter;

      for (iter = interfaces; *iter != NULL; iter++)
        {
          if (tp_dbus_check_valid_interface_name (*iter, NULL))
            {
              GQuark q = g_quark_from_string (*iter);

              tp_proxy_add_interface_by_id ((TpProxy *) self,
                  g_quark_from_string (*iter));

              if (q == TP_IFACE_QUARK_CONNECTION_INTERFACE_CONTACTS)
                {
                  func = introspect_contacts;
                  g_array_append_val (self->priv->introspect_needed, func);
                }
            }
          else
            {
              DEBUG ("\t\tInterface %s not valid", *iter);
            }
        }
    }

  /* FIXME: give subclasses a chance to influence the definition of "ready"
   * now that we have our interfaces? */

  tp_connection_continue_introspection (self);
}

static void
tp_connection_status_changed (TpConnection *self,
                              guint status,
                              guint reason)
{
  DEBUG ("%p: %d -> %d because %d", self, self->priv->status, status, reason);

  self->priv->status = status;
  self->priv->status_reason = reason;
  g_object_notify ((GObject *) self, "status");
  g_object_notify ((GObject *) self, "status-reason");

  if (status == TP_CONNECTION_STATUS_CONNECTED &&
      !self->priv->called_get_interfaces)
    {
      tp_cli_connection_call_get_interfaces (self, -1,
          tp_connection_got_interfaces_cb, NULL, NULL, NULL);
      self->priv->called_get_interfaces = TRUE;
    }
}

static void
tp_connection_connection_error_cb (TpConnection *self,
                                   const gchar *error_name,
                                   GHashTable *details,
                                   gpointer user_data,
                                   GObject *weak_object)
{
  if (self->priv->connection_error != NULL)
    {
      g_error_free (self->priv->connection_error);
      self->priv->connection_error = NULL;
    }

  tp_proxy_dbus_error_to_gerror (self, error_name,
        tp_asv_get_string (details, "debug-message"),
        &(self->priv->connection_error));
}

static void
tp_connection_status_reason_to_gerror (TpConnectionStatusReason reason,
                                       TpConnectionStatus prev_status,
                                       GError **error)
{
  TpError code;
  const gchar *message;

  switch (reason)
    {
    case TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED:
      code = TP_ERROR_DISCONNECTED;
      message = "Disconnected for unspecified reason";
      break;

    case TP_CONNECTION_STATUS_REASON_REQUESTED:
      code = TP_ERROR_CANCELLED;
      message = "User requested disconnection";
      break;

    case TP_CONNECTION_STATUS_REASON_NETWORK_ERROR:
      code = TP_ERROR_NETWORK_ERROR;
      message = "Network error";
      break;

    case TP_CONNECTION_STATUS_REASON_ENCRYPTION_ERROR:
      code = TP_ERROR_ENCRYPTION_ERROR;
      message = "Encryption error";
      break;

    case TP_CONNECTION_STATUS_REASON_NAME_IN_USE:
      if (prev_status == TP_CONNECTION_STATUS_CONNECTED)
        {
          code = TP_ERROR_CONNECTION_REPLACED;
          message = "Connection replaced";
        }
      else
        {
          /* If the connection was with register=TRUE, we should ideally use
           * REGISTRATION_EXISTS; but we can't actually tell that from here,
           * so we'll have to rely on CMs supporting in-band registration
           * (Gabble) to emit ConnectionError */
          code = TP_ERROR_ALREADY_CONNECTED;
          message = "Already connected (or if registering, registration "
            "already exists)";
        }
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_NOT_PROVIDED:
      code = TP_ERROR_CERT_NOT_PROVIDED;
      message = "Server certificate not provided";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_UNTRUSTED:
      code = TP_ERROR_CERT_UNTRUSTED;
      message = "Server certificate CA not trusted";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_EXPIRED:
      code = TP_ERROR_CERT_EXPIRED;
      message = "Server certificate expired";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_NOT_ACTIVATED:
      code = TP_ERROR_CERT_NOT_ACTIVATED;
      message = "Server certificate not valid yet";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_HOSTNAME_MISMATCH:
      code = TP_ERROR_CERT_HOSTNAME_MISMATCH;
      message = "Server certificate has wrong hostname";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_FINGERPRINT_MISMATCH:
      code = TP_ERROR_CERT_FINGERPRINT_MISMATCH;
      message = "Server certificate fingerprint mismatch";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_SELF_SIGNED:
      code = TP_ERROR_CERT_SELF_SIGNED;
      message = "Server certificate is self-signed";
      break;

    case TP_CONNECTION_STATUS_REASON_CERT_OTHER_ERROR:
      code = TP_ERROR_CERT_INVALID;
      message = "Unspecified server certificate error";
      break;

    default:
      g_set_error (error, TP_ERRORS_DISCONNECTED, reason,
          "Unknown disconnection reason");
      return;
    }

  g_set_error (error, TP_ERRORS, code, "%s", message);
}

static void
tp_connection_status_changed_cb (TpConnection *self,
                                 guint status,
                                 guint reason,
                                 gpointer user_data,
                                 GObject *weak_object)
{
  TpConnectionStatus prev_status = self->priv->status;

  /* GetStatus is called in the TpConnection constructor. If we don't have the
   * reply for this GetStatus call yet, ignore this signal StatusChanged in
   * order to run the interface introspection only one time. We will get the
   * GetStatus reply later anyway. */
  if (self->priv->status != TP_UNKNOWN_CONNECTION_STATUS)
    {
      tp_connection_status_changed (self, status, reason);
    }

  /* we only want to run this in response to a StatusChanged signal,
   * not if the initial status is DISCONNECTED */

  if (status == TP_CONNECTION_STATUS_DISCONNECTED)
    {
      if (self->priv->connection_error == NULL)
        {
          tp_connection_status_reason_to_gerror (reason, prev_status,
              &(self->priv->connection_error));
        }

      tp_proxy_invalidate ((TpProxy *) self, self->priv->connection_error);

      g_error_free (self->priv->connection_error);
      self->priv->connection_error = NULL;
    }
}

static void
tp_connection_got_status_cb (TpConnection *self,
                             guint status,
                             const GError *error,
                             gpointer unused,
                             GObject *user_object)
{
  DEBUG ("%p", self);

  if (error == NULL)
    {
      DEBUG ("%p: Initial status is %d", self, status);
      tp_connection_status_changed (self, status,
          TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED);
    }
  else
    {
      DEBUG ("%p: GetStatus() failed with %s %d \"%s\"",
          self, g_quark_to_string (error->domain), error->code,
          error->message);
    }
}

static void
tp_connection_invalidated (TpConnection *self)
{
  _tp_connection_set_self_handle (self, 0);
  _tp_connection_clean_up_handle_refs (self);
}

static GObject *
tp_connection_constructor (GType type,
                           guint n_params,
                           GObjectConstructParam *params)
{
  GObjectClass *object_class = (GObjectClass *) tp_connection_parent_class;
  TpConnection *self = TP_CONNECTION (object_class->constructor (type,
        n_params, params));

  /* Connect to my own StatusChanged signal.
   * The connection hasn't had a chance to become invalid yet, so we can
   * assume that this signal connection will work */
  DEBUG ("Connecting to StatusChanged and ConnectionError");
  tp_cli_connection_connect_to_status_changed (self,
      tp_connection_status_changed_cb, NULL, NULL, NULL, NULL);
  tp_cli_connection_connect_to_connection_error (self,
      tp_connection_connection_error_cb, NULL, NULL, NULL, NULL);

  /* get my initial status */
  DEBUG ("Calling GetStatus");
  tp_cli_connection_call_get_status (self, -1,
      tp_connection_got_status_cb, NULL, NULL, NULL);

  _tp_connection_init_handle_refs (self);

  g_signal_connect (self, "invalidated",
      G_CALLBACK (tp_connection_invalidated), NULL);

  DEBUG ("Returning %p", self);
  return (GObject *) self;
}

static void
tp_connection_init (TpConnection *self)
{
  DEBUG ("%p", self);

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, TP_TYPE_CONNECTION,
      TpConnectionPrivate);

  self->priv->status = TP_UNKNOWN_CONNECTION_STATUS;
  self->priv->status_reason = TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED;
  self->priv->contacts = g_hash_table_new (g_direct_hash, g_direct_equal);
}

static void
tp_connection_finalize (GObject *object)
{
  TpConnection *self = TP_CONNECTION (object);

  DEBUG ("%p", self);

  /* not true unless we were finalized before we were ready */
  if (self->priv->introspect_needed != NULL)
    {
      g_array_free (self->priv->introspect_needed, TRUE);
      self->priv->introspect_needed = NULL;
    }

  if (self->priv->contact_attribute_interfaces != NULL)
    {
      g_array_free (self->priv->contact_attribute_interfaces, TRUE);
      self->priv->contact_attribute_interfaces = NULL;
    }

  if (self->priv->connection_error != NULL)
    {
      g_error_free (self->priv->connection_error);
      self->priv->connection_error = NULL;
    }

  ((GObjectClass *) tp_connection_parent_class)->finalize (object);
}


static void
contact_notify_invalidated (gpointer k G_GNUC_UNUSED,
                            gpointer v,
                            gpointer d G_GNUC_UNUSED)
{
  _tp_contact_connection_invalidated (v);
}


static void
tp_connection_dispose (GObject *object)
{
  TpConnection *self = TP_CONNECTION (object);

  DEBUG ("%p", object);

  if (self->priv->contacts != NULL)
    {
      g_hash_table_foreach (self->priv->contacts, contact_notify_invalidated,
          NULL);
      g_hash_table_destroy (self->priv->contacts);
      self->priv->contacts = NULL;
    }

  ((GObjectClass *) tp_connection_parent_class)->dispose (object);
}

static void
tp_connection_class_init (TpConnectionClass *klass)
{
  GParamSpec *param_spec;
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  tp_connection_init_known_interfaces ();

  g_type_class_add_private (klass, sizeof (TpConnectionPrivate));

  object_class->constructor = tp_connection_constructor;
  object_class->get_property = tp_connection_get_property;
  object_class->dispose = tp_connection_dispose;
  object_class->finalize = tp_connection_finalize;

  proxy_class->interface = TP_IFACE_QUARK_CONNECTION;
  /* If you change this, you must also change TpChannel to stop asserting
   * that its connection has a unique name */
  proxy_class->must_have_unique_name = TRUE;

  /**
   * TpConnection:status:
   *
   * This connection's status, or TP_UNKNOWN_CONNECTION_STATUS if we don't
   * know yet.
   */
  param_spec = g_param_spec_uint ("status", "Status",
      "The status of this connection", 0, G_MAXUINT32,
      TP_UNKNOWN_CONNECTION_STATUS,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_STATUS,
      param_spec);

  /**
   * TpConnection:self-handle:
   *
   * The %TP_HANDLE_TYPE_CONTACT handle of the local user on this connection,
   * or 0 if we don't know yet or if the connection has become invalid.
   */
  param_spec = g_param_spec_uint ("self-handle", "Self handle",
      "The local user's Contact handle on this connection", 0, G_MAXUINT32,
      0,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SELF_HANDLE,
      param_spec);

  /**
   * TpConnection:status-reason:
   *
   * The reason why #TpConnection:status changed to its current value,
   * or TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED if unknown.
   * know yet.
   */
  param_spec = g_param_spec_uint ("status-reason", "Last status change reason",
      "The reason why #TpConnection:status changed to its current value",
      0, G_MAXUINT32, TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_STATUS_REASON,
      param_spec);

  /**
   * TpConnection:connection-ready:
   *
   * Initially %FALSE; changes to %TRUE when the connection has gone to
   * CONNECTED status, introspection has finished and it's ready for use.
   *
   * By the time this property becomes %TRUE, any extra interfaces will
   * have been set up and the #TpProxy:interfaces property will have been
   * populated.
   */
  param_spec = g_param_spec_boolean ("connection-ready", "Connection ready?",
      "Initially FALSE; changes to TRUE when introspection finishes", FALSE,
      G_PARAM_READABLE
      | G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB | G_PARAM_STATIC_NICK);
  g_object_class_install_property (object_class, PROP_CONNECTION_READY,
      param_spec);
}

/**
 * tp_connection_new:
 * @dbus: a D-Bus daemon; may not be %NULL
 * @bus_name: the well-known or unique name of the connection process;
 *  if well-known, this function will make a blocking call to the bus daemon
 *  to resolve the unique name. May be %NULL if @object_path is not, in which
 *  case a well-known name will be derived from @object_path.
 * @object_path: the object path of the connection process. May be %NULL
 *  if @bus_name is a well-known name, in which case the object path will
 *  be derived from @bus_name.
 * @error: used to indicate the error if %NULL is returned
 *
 * <!-- -->
 *
 * Returns: a new connection proxy, or %NULL if unique-name resolution
 *  fails or on invalid arguments
 *
 * Since: 0.7.1
 */
TpConnection *
tp_connection_new (TpDBusDaemon *dbus,
                   const gchar *bus_name,
                   const gchar *object_path,
                   GError **error)
{
  gchar *dup_path = NULL;
  gchar *dup_name = NULL;
  gchar *dup_unique_name = NULL;
  TpConnection *ret = NULL;

  g_return_val_if_fail (TP_IS_DBUS_DAEMON (dbus), NULL);
  g_return_val_if_fail (object_path != NULL ||
                        (bus_name != NULL && bus_name[0] != ':'), NULL);

  if (object_path == NULL)
    {
      dup_path = g_strdelimit (g_strdup_printf ("/%s", bus_name), ".", '/');
      object_path = dup_path;
    }
  else if (bus_name == NULL)
    {
      dup_name = g_strdelimit (g_strdup (object_path + 1), "/", '.');
      bus_name = dup_name;
    }

  if (!tp_dbus_check_valid_bus_name (bus_name,
        TP_DBUS_NAME_TYPE_NOT_BUS_DAEMON, error))
    goto finally;

  /* Resolve unique name if necessary */
  if (bus_name[0] != ':')
    {
      if (!_tp_dbus_daemon_get_name_owner (dbus, 2000, bus_name,
          &dup_unique_name, error))
        goto finally;

      bus_name = dup_unique_name;

      if (!tp_dbus_check_valid_bus_name (bus_name,
          TP_DBUS_NAME_TYPE_UNIQUE, error))
        goto finally;
    }

  if (!tp_dbus_check_valid_object_path (object_path, error))
    goto finally;

  ret = TP_CONNECTION (g_object_new (TP_TYPE_CONNECTION,
        "dbus-daemon", dbus,
        "bus-name", bus_name,
        "object-path", object_path,
        NULL));

finally:
  g_free (dup_path);
  g_free (dup_name);
  g_free (dup_unique_name);

  return ret;
}

/**
 * tp_connection_get_self_handle:
 * @self: a connection
 *
 * Return the %TP_HANDLE_TYPE_CONTACT handle of the local user on this
 * connection, or 0 if the connection is not ready (the
 * TpConnection:connection-ready property is false) or has become invalid
 * (the TpProxy::invalidated signal).
 *
 * The returned handle is not necessarily valid forever (the
 * notify::self-handle signal will be emitted if it changes, which can happen
 * on protocols such as IRC). Construct a #TpContact object if you want to
 * track the local user's identifier in the protocol, or other information
 * like their presence status, over time.
 *
 * Returns: the value of the TpConnection:self-handle property
 *
 * Since: 0.7.26
 */
TpHandle
tp_connection_get_self_handle (TpConnection *self)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), 0);
  return self->priv->self_handle;
}

/**
 * tp_connection_get_status:
 * @self: a connection
 * @reason: a TpConnectionStatusReason, or %NULL
 *
 * If @reason is not %NULL it is set to the reason why "status" changed to its
 * current value, or %TP_CONNECTION_STATUS_REASON_NONE_SPECIFIED if unknown.
 *
 * Returns: This connection's status, or %TP_UNKNOWN_CONNECTION_STATUS if we
 * don't know yet.
 *
 * Since: 0.7.14
 */
TpConnectionStatus
tp_connection_get_status (TpConnection *self,
                          TpConnectionStatusReason *reason)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), TP_UNKNOWN_CONNECTION_STATUS);

  if (reason != NULL)
    *reason = self->priv->status_reason;

  return self->priv->status;
}

/**
 * tp_connection_run_until_ready:
 * @self: a connection
 * @connect: if %TRUE, call Connect() if it appears to be necessary;
 *  if %FALSE, rely on Connect() to be called by another client
 * @error: if not %NULL and %FALSE is returned, used to raise an error
 * @loop: if not %NULL, a #GMainLoop is placed here while it is being run
 *  (so calling code can call g_main_loop_quit() to abort), and %NULL is
 *  placed here after the loop has been run
 *
 * If @self is connected and ready for use, return immediately. Otherwise,
 * call Connect() (unless @connect is %FALSE) and re-enter the main loop
 * until the connection becomes invalid, the connection connects successfully
 * and is introspected, or the main loop stored via @loop is cancelled.
 *
 * Returns: %TRUE if the connection is now connected and ready for use,
 *  %FALSE if the connection has become invalid.
 *
 * Since: 0.7.1
 */

typedef struct {
    GMainLoop *loop;
    TpProxyPendingCall *pc;
    GError *connect_error /* gets initialized */;
} RunUntilReadyData;

static void
run_until_ready_ret (TpConnection *self,
                     const GError *error,
                     gpointer user_data,
                     GObject *weak_object)
{
  RunUntilReadyData *data = user_data;

  if (error != NULL)
    {
      g_main_loop_quit (data->loop);
      data->connect_error = g_error_copy (error);
    }
}

static void
run_until_ready_destroy (gpointer p)
{
  RunUntilReadyData *data = p;

  data->pc = NULL;
}

gboolean
tp_connection_run_until_ready (TpConnection *self,
                               gboolean connect,
                               GError **error,
                               GMainLoop **loop)
{
  TpProxy *as_proxy = (TpProxy *) self;
  gulong invalidated_id, ready_id;
  RunUntilReadyData data = { NULL, NULL, NULL };

  g_return_val_if_fail (TP_IS_CONNECTION (self), FALSE);

  if (as_proxy->invalidated)
    goto raise_invalidated;

  if (self->priv->ready)
    return TRUE;

  data.loop = g_main_loop_new (NULL, FALSE);

  invalidated_id = g_signal_connect_swapped (self, "invalidated",
      G_CALLBACK (g_main_loop_quit), data.loop);
  ready_id = g_signal_connect_swapped (self, "notify::connection-ready",
      G_CALLBACK (g_main_loop_quit), data.loop);

  if (self->priv->status != TP_CONNECTION_STATUS_CONNECTED &&
      connect)
    {
      data.pc = tp_cli_connection_call_connect (self, -1,
          run_until_ready_ret, &data,
          run_until_ready_destroy, NULL);
    }

  if (data.connect_error == NULL)
    {
      if (loop != NULL)
        *loop = data.loop;

      g_main_loop_run (data.loop);

      if (loop != NULL)
        *loop = NULL;
    }

  if (data.pc != NULL)
    tp_proxy_pending_call_cancel (data.pc);

  g_signal_handler_disconnect (self, invalidated_id);
  g_signal_handler_disconnect (self, ready_id);
  g_main_loop_unref (data.loop);

  if (data.connect_error != NULL)
    {
      g_propagate_error (error, data.connect_error);
      return FALSE;
    }

  if (as_proxy->invalidated != NULL)
    goto raise_invalidated;

  if (self->priv->ready)
    return TRUE;

  g_set_error (error, TP_DBUS_ERRORS, TP_DBUS_ERROR_CANCELLED,
      "tp_connection_run_until_ready() cancelled");
  return FALSE;

raise_invalidated:
  if (error != NULL)
    {
      g_return_val_if_fail (*error == NULL, FALSE);
      *error = g_error_copy (as_proxy->invalidated);
    }

  return FALSE;
}

/**
 * TpConnectionNameListCb:
 * @names: %NULL-terminated array of @n connection bus names,
 *   or %NULL on error
 * @n: number of names (not including the final %NULL), or 0 on error
 * @cms: %NULL-terminated array of @n connection manager names
 *   (e.g. "gabble") in the same order as @names, or %NULL on error
 * @protocols: %NULL-terminated array of @n protocol names as defined in the
 *   Telepathy spec (e.g. "jabber") in the same order as @names, or %NULL on
 *   error
 * @error: %NULL on success, or an error that occurred
 * @user_data: user-supplied data
 * @weak_object: user-supplied weakly referenced object
 *
 * Signature of the callback supplied to tp_list_connection_names().
 *
 * Since: 0.7.1
 */

typedef struct {
    TpConnectionNameListCb callback;
    gpointer user_data;
    GDestroyNotify destroy;
} _ListContext;

static gboolean
_tp_connection_parse (const gchar *path_or_bus_name,
                      char delimiter,
                      gchar **protocol,
                      gchar **cm_name)
{
  const gchar *prefix;
  const gchar *cm_name_start;
  const gchar *protocol_start;
  const gchar *account_start;
  gchar *dup_cm_name = NULL;
  gchar *dup_protocol = NULL;

  g_return_val_if_fail (delimiter == '.' || delimiter == '/', FALSE);

  /* If CM respects the spec, object path and bus name should be in the form:
   * /org/freedesktop/Telepathy/Connection/cmname/proto/account
   * org.freedesktop.Telepathy.Connection.cmname.proto.account
   */
  if (delimiter == '.')
    prefix = TP_CONN_BUS_NAME_BASE;
  else
    prefix = TP_CONN_OBJECT_PATH_BASE;

  if (!g_str_has_prefix (path_or_bus_name, prefix))
    goto OUT;

  cm_name_start = path_or_bus_name + strlen (prefix);
  protocol_start = strchr (cm_name_start, delimiter);
  if (protocol_start == NULL)
    goto OUT;
  protocol_start++;

  account_start = strchr (protocol_start, delimiter);
  if (account_start == NULL)
    goto OUT;
  account_start++;

  dup_cm_name = g_strndup (cm_name_start, protocol_start - cm_name_start - 1);
  if (!tp_connection_manager_check_valid_name (dup_cm_name, NULL))
    {
      g_free (dup_cm_name);
      dup_cm_name = NULL;
      goto OUT;
    }

  dup_protocol = g_strndup (protocol_start, account_start - protocol_start - 1);
  if (!tp_strdiff (dup_protocol, "local_2dxmpp"))
    {
      /* the CM's telepathy-glib is older than 0.7.x, work around it.
       * FIXME: Remove this workaround in 0.9.x */
      g_free (dup_protocol);
      dup_protocol = g_strdup ("local-xmpp");
    }
  else
    {
      /* the real protocol name may have "-" in; bus names may not, but
       * they may have "_", so the Telepathy spec specifies replacement.
       * Here we need to undo that replacement */
      g_strdelimit (dup_protocol, "_", '-');
    }

  if (!tp_connection_manager_check_valid_protocol_name (dup_protocol, NULL))
    {
      g_free (dup_protocol);
      dup_protocol = NULL;
      goto OUT;
    }

OUT:

  if (dup_protocol == NULL || dup_cm_name == NULL)
    {
      g_free (dup_protocol);
      g_free (dup_cm_name);
      return FALSE;
    }

  if (cm_name != NULL)
    *cm_name = dup_cm_name;
  else
    g_free (dup_cm_name);

  if (protocol != NULL)
    *protocol = dup_protocol;
  else
    g_free (dup_protocol);

  return TRUE;
}

static void
tp_list_connection_names_helper (TpDBusDaemon *bus_daemon,
                                 const gchar * const *names,
                                 const GError *error,
                                 gpointer user_data,
                                 GObject *user_object)
{
  _ListContext *list_context = user_data;
  const gchar * const *iter;
  /* array of borrowed strings */
  GPtrArray *bus_names;
  /* array of dup'd strings */
  GPtrArray *cms;
  /* array of borrowed strings */
  GPtrArray *protocols;

  if (error != NULL)
    {
      list_context->callback (NULL, 0, NULL, NULL, error,
          list_context->user_data, user_object);
      return;
    }

  bus_names = g_ptr_array_new ();
  cms = g_ptr_array_new ();
  protocols = g_ptr_array_new ();

  for (iter = names; iter != NULL && *iter != NULL; iter++)
    {
      gchar *proto, *cm_name;

      if (_tp_connection_parse (*iter, '.', &proto, &cm_name))
        {
          /* the casts here are because g_ptr_array contains non-const pointers -
           * but in this case I'll only be passing pdata to a callback with const
           * arguments, so it's fine */
          g_ptr_array_add (bus_names, (gpointer) *iter);
          g_ptr_array_add (cms, cm_name);
          g_ptr_array_add (protocols, proto);
          continue;
        }

      DEBUG ("invalid name: %s", *iter);
    }

  g_ptr_array_add (bus_names, NULL);
  g_ptr_array_add (cms, NULL);
  g_ptr_array_add (protocols, NULL);

  list_context->callback ((const gchar * const *) bus_names->pdata,
      bus_names->len - 1, (const gchar * const *) cms->pdata,
      (const gchar * const *) protocols->pdata,
      NULL, list_context->user_data, user_object);

  g_ptr_array_free (bus_names, TRUE);
  g_strfreev ((char **) g_ptr_array_free (cms, FALSE));
  g_strfreev ((char **) g_ptr_array_free (protocols, FALSE));
}

static void
list_context_free (gpointer p)
{
  _ListContext *list_context = p;

  if (list_context->destroy != NULL)
    list_context->destroy (list_context->user_data);

  g_slice_free (_ListContext, list_context);
}

/**
 * tp_list_connection_names:
 * @bus_daemon: proxy for the D-Bus daemon
 * @callback: callback to be called when listing the connections succeeds or
 *   fails; not called if the D-Bus connection fails completely or if the
 *   @weak_object goes away
 * @user_data: user-supplied data for the callback
 * @destroy: callback to destroy the user-supplied data, called after
 *   @callback, but also if the D-Bus connection fails or if the @weak_object
 *   goes away
 * @weak_object: if not %NULL, will be weakly referenced; the callback will
 *   not be called if the object has vanished
 *
 * List the bus names of all the connections that currently exist, together
 * with the connection manager name and the protocol name for each connection.
 * Call the callback when done.
 *
 * The bus names passed to the callback can be used to construct #TpConnection
 * objects for any connections that are of interest.
 *
 * Since: 0.7.1
 */
void
tp_list_connection_names (TpDBusDaemon *bus_daemon,
                          TpConnectionNameListCb callback,
                          gpointer user_data,
                          GDestroyNotify destroy,
                          GObject *weak_object)
{
  _ListContext *list_context = g_slice_new0 (_ListContext);

  g_return_if_fail (TP_IS_DBUS_DAEMON (bus_daemon));
  g_return_if_fail (callback != NULL);

  list_context->callback = callback;
  list_context->user_data = user_data;

  tp_dbus_daemon_list_names (bus_daemon, 2000,
      tp_list_connection_names_helper, list_context,
      list_context_free, weak_object);
}

static gpointer
tp_connection_once (gpointer data G_GNUC_UNUSED)
{
  GType type = TP_TYPE_CONNECTION;

  tp_proxy_init_known_interfaces ();

  tp_proxy_or_subclass_hook_on_interface_add (type,
      tp_cli_connection_add_signals);
  tp_proxy_subclass_add_error_mapping (type,
      TP_ERROR_PREFIX, TP_ERRORS, TP_TYPE_ERROR);

  return NULL;
}

/**
 * tp_connection_init_known_interfaces:
 *
 * Ensure that the known interfaces for TpConnection have been set up.
 * This is done automatically when necessary, but for correct
 * overriding of library interfaces by local extensions, you should
 * call this function before calling
 * tp_proxy_or_subclass_hook_on_interface_add() with first argument
 * %TP_TYPE_CONNECTION.
 *
 * Since: 0.7.6
 */
void
tp_connection_init_known_interfaces (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, tp_connection_once, NULL);
}

typedef struct {
    TpConnectionWhenReadyCb callback;
    gpointer user_data;
    gulong invalidated_id;
    gulong ready_id;
} CallWhenReadyContext;

static void
cwr_invalidated (TpConnection *self,
                 guint domain,
                 gint code,
                 gchar *message,
                 gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;
  GError e = { domain, code, message };

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, &e, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

static void
cwr_ready (TpConnection *self,
           GParamSpec *unused G_GNUC_UNUSED,
           gpointer user_data)
{
  CallWhenReadyContext *ctx = user_data;

  DEBUG ("enter");

  g_assert (ctx->callback != NULL);

  ctx->callback (self, NULL, ctx->user_data);

  g_signal_handler_disconnect (self, ctx->invalidated_id);
  g_signal_handler_disconnect (self, ctx->ready_id);

  ctx->callback = NULL;   /* poison it to detect errors */
  g_slice_free (CallWhenReadyContext, ctx);
}

/**
 * TpConnectionWhenReadyCb:
 * @connection: the connection (which may be in the middle of being disposed,
 *  if error is non-%NULL, error->domain is TP_DBUS_ERRORS and error->code is
 *  TP_DBUS_ERROR_PROXY_UNREFERENCED)
 * @error: %NULL if the connection is ready for use, or the error with which
 *  it was invalidated if it is now invalid
 * @user_data: whatever was passed to tp_connection_call_when_ready()
 *
 * Signature of a callback passed to tp_connection_call_when_ready(), which
 * will be called exactly once, when the connection becomes ready or
 * invalid (whichever happens first)
 */

/**
 * tp_connection_call_when_ready:
 * @self: a connection
 * @callback: called when the connection becomes ready or invalidated,
 *  whichever happens first
 * @user_data: arbitrary user-supplied data passed to the callback
 *
 * If @self is ready for use or has been invalidated, call @callback
 * immediately, then return. Otherwise, arrange
 * for @callback to be called when @self either becomes ready for use
 * or becomes invalid.
 *
 * Note that if the connection is not in state CONNECTED, the callback will
 * not be called until the connection either goes to state CONNECTED
 * or is invalidated (e.g. by going to state DISCONNECTED or by becoming
 * unreferenced). In particular, this method does not call Connect().
 * Call tp_cli_connection_call_connect() too, if you want to do that.
 *
 * Since: 0.7.7
 */
void
tp_connection_call_when_ready (TpConnection *self,
                               TpConnectionWhenReadyCb callback,
                               gpointer user_data)
{
  TpProxy *as_proxy = (TpProxy *) self;

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (callback != NULL);

  if (self->priv->ready || as_proxy->invalidated != NULL)
    {
      DEBUG ("already ready or invalidated");
      callback (self, as_proxy->invalidated, user_data);
    }
  else
    {
      CallWhenReadyContext *ctx = g_slice_new (CallWhenReadyContext);

      DEBUG ("arranging callback later");

      ctx->callback = callback;
      ctx->user_data = user_data;
      ctx->invalidated_id = g_signal_connect (self, "invalidated",
          G_CALLBACK (cwr_invalidated), ctx);
      ctx->ready_id = g_signal_connect (self, "notify::connection-ready",
          G_CALLBACK (cwr_ready), ctx);
    }
}

static guint
get_presence_type_availability (TpConnectionPresenceType type)
{
  switch (type)
    {
      case TP_CONNECTION_PRESENCE_TYPE_UNSET:
        return 0;
      case TP_CONNECTION_PRESENCE_TYPE_UNKNOWN:
        return 1;
      case TP_CONNECTION_PRESENCE_TYPE_ERROR:
        return 2;
      case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
        return 3;
      case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
        return 4;
      case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
        return 5;
      case TP_CONNECTION_PRESENCE_TYPE_AWAY:
        return 6;
      case TP_CONNECTION_PRESENCE_TYPE_BUSY:
        return 7;
      case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
        return 8;
    }

  /* This is an unexpected presence type, treat it like UNKNOWN */
  return 1;
}

/**
 * tp_connection_presence_type_cmp_availability:
 * @p1: a #TpConnectionPresenceType
 * @p2: a #TpConnectionPresenceType
 *
 * Compares @p1 and @p2 like strcmp(). @p1 > @p2 means @p1 is more available
 * than @p2.
 *
 * The order used is: available > busy > away > xa > hidden > offline > error >
 * unknown > unset
 *
 * Returns: -1, 0 or 1, if @p1 is <, == or > than @p2.
 *
 * Since: 0.7.16
 */
gint
tp_connection_presence_type_cmp_availability (TpConnectionPresenceType p1,
                                              TpConnectionPresenceType p2)
{
  guint availability1;
  guint availability2;

  availability1 = get_presence_type_availability (p1);
  availability2 = get_presence_type_availability (p2);

  if (availability1 < availability2)
    return -1;

  if (availability1 > availability2)
    return +1;

  return 0;
}


/**
 * tp_connection_parse_object_path:
 * @self: a connection
 * @protocol: If not NULL, used to return the protocol of the connection
 * @cm_name: If not NULL, used to return the connection manager name of the
 * connection
 *
 * If the object path of @connection is in the correct form, set
 * @protocol and @cm_name, return TRUE. Otherwise leave them unchanged and
 * return FALSE.
 *
 * Returns: TRUE if the object path was correctly parsed, FALSE otherwise.
 *
 * Since: 0.7.27
 */
gboolean
tp_connection_parse_object_path (TpConnection *self,
                                 gchar **protocol,
                                 gchar **cm_name)
{
  const gchar *object_path;

  g_return_val_if_fail (TP_IS_CONNECTION (self), FALSE);

  object_path = tp_proxy_get_object_path (TP_PROXY (self));
  return _tp_connection_parse (object_path, '/', protocol, cm_name);
}

TpContact *
_tp_connection_lookup_contact (TpConnection *self,
                               TpHandle handle)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), NULL);

  return g_hash_table_lookup (self->priv->contacts, GUINT_TO_POINTER (handle));
}


/* this could be done with proper weak references, but we know that every
 * connection will weakly reference all its contacts, so we can just do this
 * explicitly in tp_contact_dispose */
void
_tp_connection_remove_contact (TpConnection *self,
                               TpHandle handle,
                               TpContact *contact)
{
  TpContact *mine;

  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (TP_IS_CONTACT (contact));

  mine = g_hash_table_lookup (self->priv->contacts, GUINT_TO_POINTER (handle));
  g_return_if_fail (mine == contact);
  g_hash_table_remove (self->priv->contacts, GUINT_TO_POINTER (handle));
}


void
_tp_connection_add_contact (TpConnection *self,
                            TpHandle handle,
                            TpContact *contact)
{
  g_return_if_fail (TP_IS_CONNECTION (self));
  g_return_if_fail (TP_IS_CONTACT (contact));
  g_return_if_fail (g_hash_table_lookup (self->priv->contacts,
        GUINT_TO_POINTER (handle)) == NULL);

  g_hash_table_insert (self->priv->contacts, GUINT_TO_POINTER (handle),
      contact);
}


/**
 * tp_connection_is_ready:
 * @self: a connection
 *
 * Returns the same thing as the #TpConnection:connection-ready property.
 *
 * Returns: %TRUE if introspection has completed
 * Since: 0.7.17
 */
gboolean
tp_connection_is_ready (TpConnection *self)
{
  g_return_val_if_fail (TP_IS_CONNECTION (self), FALSE);

  return self->priv->ready;
}
