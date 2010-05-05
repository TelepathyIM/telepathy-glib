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

#include <string.h>

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
  GHashTable *protocol_properties;
};

enum
{
    PROP_PROTOCOL_NAME = 1,
    PROP_PROTOCOL_PROPERTIES,
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

    case PROP_PROTOCOL_PROPERTIES:
      g_value_set_boxed (value, self->priv->protocol_properties);
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

    case PROP_PROTOCOL_PROPERTIES:
      g_assert (self->priv->protocol_properties == NULL);
      self->priv->protocol_properties = g_value_dup_boxed (value);
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

  if (self->priv->protocol_properties != NULL)
    g_hash_table_unref (self->priv->protocol_properties);

  if (finalize != NULL)
    finalize (object);
}

static void
tp_protocol_constructed (GObject *object)
{
  TpProtocol *self = (TpProtocol *) object;
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_protocol_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->protocol_struct.name != NULL);

  if (self->priv->protocol_properties == NULL)
    self->priv->protocol_properties = g_hash_table_new_full (g_str_hash,
        g_str_equal, g_free, (GDestroyNotify) tp_g_value_slice_free);
}

static void
tp_protocol_class_init (TpProtocolClass *klass)
{
  TpProxyClass *proxy_class = (TpProxyClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;

  g_type_class_add_private (klass, sizeof (TpProtocolPrivate));

  object_class->constructed = tp_protocol_constructed;
  object_class->get_property = tp_protocol_get_property;
  object_class->set_property = tp_protocol_set_property;
  object_class->finalize = tp_protocol_finalize;

  g_object_class_install_property (object_class, PROP_PROTOCOL_NAME,
      g_param_spec_string ("protocol-name",
        "Name of this protocol",
        "The Protocol from telepathy-spec, such as 'jabber' or 'local-xmpp'",
        NULL,
        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_PROTOCOL_PROPERTIES,
      g_param_spec_boxed ("protocol-properties",
        "Protocol properties",
        "The immutable properties of this Protocol",
        TP_HASH_TYPE_QUALIFIED_PROPERTY_VALUE_MAP,
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
 * @immutable_properties: the immutable D-Bus properties for this protocol
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
    const GHashTable *immutable_properties,
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
        "protocol-properties", immutable_properties,
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

static gboolean
init_gvalue_from_dbus_sig (const gchar *sig,
                           GValue *value)
{
  g_assert (!G_IS_VALUE (value));

  switch (sig[0])
    {
    case 'b':
      g_value_init (value, G_TYPE_BOOLEAN);
      return TRUE;

    case 's':
      g_value_init (value, G_TYPE_STRING);
      return TRUE;

    case 'q':
    case 'u':
      g_value_init (value, G_TYPE_UINT);
      return TRUE;

    case 'y':
      g_value_init (value, G_TYPE_UCHAR);
      return TRUE;

    case 'n':
    case 'i':
      g_value_init (value, G_TYPE_INT);
      return TRUE;

    case 'x':
      g_value_init (value, G_TYPE_INT64);
      return TRUE;

    case 't':
      g_value_init (value, G_TYPE_UINT64);
      return TRUE;

    case 'o':
      g_value_init (value, DBUS_TYPE_G_OBJECT_PATH);
      g_value_set_static_boxed (value, "/");
      return TRUE;

    case 'd':
      g_value_init (value, G_TYPE_DOUBLE);
      return TRUE;

    case 'v':
      g_value_init (value, G_TYPE_VALUE);
      return TRUE;

    case 'a':
      switch (sig[1])
        {
        case 's':
          g_value_init (value, G_TYPE_STRV);
          return TRUE;

        case 'y':
          g_value_init (value, DBUS_TYPE_G_UCHAR_ARRAY);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
parse_default_value (GValue *value,
                     const gchar *sig,
                     gchar *string,
                     GKeyFile *file,
                     const gchar *group,
                     const gchar *key)
{
  GError *error = NULL;
  gchar *s, *p;

  switch (sig[0])
    {
    case 'b':
      g_value_set_boolean (value, g_key_file_get_boolean (file, group, key,
            &error));

      if (error == NULL)
        return TRUE;

      /* In telepathy-glib < 0.7.26 we accepted true and false in
       * any case combination, 0, and 1. The desktop file spec specifies
       * "true" and "false" only, while GKeyFile currently accepts 0 and 1 too.
       * So, on error, let's fall back to more lenient parsing that explicitly
       * allows everything we historically allowed. */
      g_error_free (error);
      s = g_key_file_get_value (file, group, key, NULL);

      if (s == NULL)
        return FALSE;

      for (p = s; *p != '\0'; p++)
        {
          *p = g_ascii_tolower (*p);
        }

      if (!tp_strdiff (s, "1") || !tp_strdiff (s, "true"))
        {
          g_value_set_boolean (value, TRUE);
        }
      else if (!tp_strdiff (s, "0") || !tp_strdiff (s, "false"))
        {
          g_value_set_boolean (value, TRUE);
        }
      else
        {
          g_free (s);
          return FALSE;
        }

      g_free (s);
      return TRUE;

    case 's':
      s = g_key_file_get_string (file, group, key, NULL);

      g_value_take_string (value, s);
      return (s != NULL);

    case 'y':
    case 'q':
    case 'u':
    case 't':
        {
          guint64 v = tp_g_key_file_get_uint64 (file, group, key, &error);

          if (error != NULL)
            {
              g_error_free (error);
              return FALSE;
            }

          if (sig[0] == 't')
            {
              g_value_set_uint64 (value, v);
              return TRUE;
            }

          if (sig[0] == 'y')
            {
              if (v > G_MAXUINT8)
                {
                  return FALSE;
                }

              g_value_set_uchar (value, v);
              return TRUE;
            }

          if (v > G_MAXUINT32 || (sig[0] == 'q' && v > G_MAXUINT16))
            return FALSE;

          g_value_set_uint (value, v);
          return TRUE;
        }

    case 'n':
    case 'i':
    case 'x':
      if (string[0] == '\0')
        {
          return FALSE;
        }
      else
        {
          gint64 v = tp_g_key_file_get_int64 (file, group, key, &error);

          if (error != NULL)
            {
              g_error_free (error);
              return FALSE;
            }

          if (sig[0] == 'x')
            {
              g_value_set_int64 (value, v);
              return TRUE;
            }

          if (v > G_MAXINT32 || (sig[0] == 'q' && v > G_MAXINT16))
            return FALSE;

          if (v < G_MININT32 || (sig[0] == 'n' && v < G_MININT16))
            return FALSE;

          g_value_set_int (value, v);
          return TRUE;
        }

    case 'o':
      s = g_key_file_get_string (file, group, key, NULL);

      if (s == NULL || !tp_dbus_check_valid_object_path (s, NULL))
        {
          g_free (s);
          return FALSE;
        }

      g_value_take_boxed (value, s);

      return TRUE;

    case 'd':
      g_value_set_double (value, g_key_file_get_double (file, group, key,
            &error));

      if (error != NULL)
        {
          g_error_free (error);
          return FALSE;
        }

      return TRUE;

    case 'a':
      switch (sig[1])
        {
        case 's':
            {
              g_value_take_boxed (value,
                  g_key_file_get_string_list (file, group, key, NULL, &error));

              if (error != NULL)
                {
                  g_error_free (error);
                  return FALSE;
                }

              return TRUE;
            }
        }
    }

  if (G_IS_VALUE (value))
    g_value_unset (value);

  return FALSE;
}

#define PROTOCOL_PREFIX "Protocol "
#define PROTOCOL_PREFIX_LEN 9
tp_verify (sizeof (PROTOCOL_PREFIX) == PROTOCOL_PREFIX_LEN + 1);

static gchar *
replace_null_with_empty (gchar *in)
{
  return (in == NULL ? g_strdup ("") : in);
}

static GHashTable *
_tp_protocol_parse_channel_class (GKeyFile *file,
    const gchar *group)
{
  GHashTable *ret;
  gchar **keys, **key;

  ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) tp_g_value_slice_free);

  keys = g_key_file_get_keys (file, group, NULL, NULL);

  for (key = keys; key != NULL && *key != NULL; key++)
    {
      gchar *space = strchr (*key, ' ');
      gchar *value = NULL;
      gchar *property = NULL;
      const gchar *dbus_type;
      GValue *v = NULL;

      value = g_key_file_get_string (file, group, *key, NULL);

      /* keys without a space are reserved */
      if (space == NULL)
        goto cleanup;

      property = g_strndup (*key, space - *key);
      dbus_type = space + 1;

      if (!init_gvalue_from_dbus_sig (dbus_type, v))
        goto cleanup;

      if (!parse_default_value (v, dbus_type, value, file, group, *key))
        goto cleanup;

      /* transfer ownership to @ret */
      g_hash_table_insert (ret, property, v);
      property = NULL;
      v = NULL;

cleanup:
      if (v != NULL)
        tp_g_value_slice_free (v);

      g_free (property);
      g_free (value);
    }

  return ret;
}

static GValueArray *
_tp_protocol_parse_rcc (GKeyFile *file,
    const gchar *group)
{
  GHashTable *fixed;
  GStrv allowed;
  GValueArray *ret;

  fixed = _tp_protocol_parse_channel_class (file, group);
  allowed = g_key_file_get_string_list (file, group, "allowed", NULL, NULL);

  ret = tp_value_array_build (2,
      TP_HASH_TYPE_CHANNEL_CLASS, fixed,
      G_TYPE_STRV, allowed,
      NULL);

  g_hash_table_unref (fixed);
  g_strfreev (allowed);

  return ret;
}

GHashTable *
_tp_protocol_parse_manager_file (GKeyFile *file,
    const gchar *cm_name,
    const gchar *group,
    gchar **protocol_name)
{
  GHashTable *immutables;
  GPtrArray *param_specs, *rccs;
  const gchar *name;
  gchar **rcc_groups, **rcc_group;
  gchar **keys, **key;
  guint i;

  if (!g_str_has_prefix (group, PROTOCOL_PREFIX))
    return NULL;

  name = group + PROTOCOL_PREFIX_LEN;

  if (!tp_connection_manager_check_valid_protocol_name (name, NULL))
    {
      DEBUG ("Protocol '%s' has an invalid name", name);
      return NULL;
    }

  keys = g_key_file_get_keys (file, group, NULL, NULL);

  i = 0;

  for (key = keys; key != NULL && *key != NULL; key++)
    {
      if (g_str_has_prefix (*key, "param-"))
        i++;
    }

  param_specs = g_ptr_array_sized_new (i);

  for (key = keys; key != NULL && *key != NULL; key++)
    {
      if (g_str_has_prefix (*key, "param-"))
        {
          gchar **strv, **iter;
          gchar *value, *def;
          TpConnectionManagerParam param = { NULL };

          value = g_key_file_get_string (file, group, *key, NULL);

          if (value == NULL)
            continue;

          /* strlen ("param-") == 6 */
          param.name = *key + 6;

          strv = g_strsplit (value, " ", 0);
          g_free (value);

          param.dbus_signature = strv[0];

          param.flags = 0;

          for (iter = strv + 1; *iter != NULL; iter++)
            {
              if (!tp_strdiff (*iter, "required"))
                param.flags |= TP_CONN_MGR_PARAM_FLAG_REQUIRED;
              if (!tp_strdiff (*iter, "register"))
                param.flags |= TP_CONN_MGR_PARAM_FLAG_REGISTER;
              if (!tp_strdiff (*iter, "secret"))
                param.flags |= TP_CONN_MGR_PARAM_FLAG_SECRET;
              if (!tp_strdiff (*iter, "dbus-property"))
                param.flags |= TP_CONN_MGR_PARAM_FLAG_DBUS_PROPERTY;
            }

          if ((!tp_strdiff (param.name, "password") ||
              g_str_has_suffix (param.name, "-password")) &&
              (param.flags & TP_CONN_MGR_PARAM_FLAG_SECRET) == 0)
            {
              DEBUG ("\tTreating %s as secret due to its name (please "
                  "fix %s.manager)", param.name, cm_name);
              param.flags |= TP_CONN_MGR_PARAM_FLAG_SECRET;
            }

          def = g_strdup_printf ("default-%s", param.name);
          value = g_key_file_get_string (file, group, def, NULL);

          init_gvalue_from_dbus_sig (param.dbus_signature,
              &param.default_value);

          if (value != NULL && parse_default_value (&param.default_value,
                param.dbus_signature, value, file, group, def))
            param.flags |= TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT;

          DEBUG ("\tParam name: %s", param.name);
          DEBUG ("\tParam flags: 0x%x", param.flags);
          DEBUG ("\tParam sig: %s", param.dbus_signature);

#ifdef ENABLE_DEBUG
          if (G_IS_VALUE (&param.default_value))
            {
              gchar *repr = g_strdup_value_contents (&(param.default_value));

              DEBUG ("\tParam default value: %s of type %s", repr,
                  G_VALUE_TYPE_NAME (&(param.default_value)));
              g_free (repr);
            }
          else
            {
              DEBUG ("\tParam default value: not set");
            }
#endif

          g_ptr_array_add (param_specs, tp_value_array_build (4,
                G_TYPE_STRING, param.name,
                G_TYPE_UINT, param.flags,
                G_TYPE_STRING, param.dbus_signature,
                G_TYPE_VALUE, &param.default_value,
                G_TYPE_INVALID));

          if (G_IS_VALUE (&param.default_value))
            g_value_unset (&param.default_value);

          g_free (value);
          g_free (def);
          g_strfreev (strv);
        }
    }

  g_strfreev (keys);

  immutables = tp_asv_new (
      TP_PROP_PROTOCOL_PARAMETERS, TP_ARRAY_TYPE_PARAM_SPEC_LIST, param_specs,
      NULL);

  tp_asv_take_boxed (immutables, TP_PROP_PROTOCOL_INTERFACES, G_TYPE_STRV,
      g_key_file_get_string_list (file, group, "Interfaces", NULL, NULL));
  tp_asv_take_boxed (immutables, TP_PROP_PROTOCOL_CONNECTION_INTERFACES,
      G_TYPE_STRV,
      g_key_file_get_string_list (file, group, "ConnectionInterfaces",
        NULL, NULL));
  tp_asv_take_string (immutables, TP_PROP_PROTOCOL_VCARD_FIELD,
      replace_null_with_empty (
        g_key_file_get_string (file, group, "VCardField", NULL)));
  tp_asv_take_string (immutables, TP_PROP_PROTOCOL_ENGLISH_NAME,
      replace_null_with_empty (
        g_key_file_get_string (file, group, "EnglishName", NULL)));
  tp_asv_take_string (immutables, TP_PROP_PROTOCOL_ICON,
      replace_null_with_empty (
        g_key_file_get_string (file, group, "Icon", NULL)));

  rccs = g_ptr_array_new ();

  rcc_groups = g_key_file_get_string_list (file, group,
      "RequestableChannelClasses", NULL, NULL);

  if (rcc_groups != NULL)
    {
      for (rcc_group = rcc_groups; *rcc_group != NULL; rcc_group++)
        g_ptr_array_add (rccs, _tp_protocol_parse_rcc (file, *rcc_group));
    }

  g_strfreev (rcc_groups);

  if (protocol_name != NULL)
    *protocol_name = g_strdup (name);

  return immutables;
}
