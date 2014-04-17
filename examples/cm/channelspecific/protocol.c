/*
 * protocol.c - an example Protocol
 *
 * Copyright © 2007-2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "protocol.h"

#include <string.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "conn.h"
#include "room-manager.h"

struct _ExampleCSHProtocolPrivate
{
  GPtrArray *params;
};

G_DEFINE_TYPE (ExampleCSHProtocol,
    example_csh_protocol,
    TP_TYPE_BASE_PROTOCOL)

static void
example_csh_protocol_init (
    ExampleCSHProtocol *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, EXAMPLE_TYPE_CSH_PROTOCOL,
      ExampleCSHProtocolPrivate);
}

/* For this example, we imagine that global handles look like
 * username@realm and channel-specific handles look like nickname@#chatroom,
 * where username and nickname contain any UTF-8 except "@", and realm
 * and chatroom contain any UTF-8 except "@" and "#".
 *
 * Additionally, we imagine that everything is case-sensitive but is
 * required to be in NFKC.
 */
gboolean
example_csh_protocol_check_contact_id (const gchar *id,
    gchar **normal,
    GError **error)
{
  const gchar *at;

  g_return_val_if_fail (id != NULL, FALSE);

  if (id[0] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "ID must not be empty");
      return FALSE;
    }

  at = strchr (id, '@');

  if (at == NULL || at == id || at[1] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "ID must look like aaa@bbb");
      return FALSE;
    }

  if (strchr (at + 1, '@') != NULL)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "ID cannot contain more than one '@'");
      return FALSE;
    }

  if (at[1] == '#' && at[2] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "chatroom name cannot be empty");
      return FALSE;
    }

  if (strchr (at + 2, '#') != NULL)
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "realm/chatroom cannot contain '#' except at the beginning");
      return FALSE;
    }

  if (normal != NULL)
    *normal = g_utf8_normalize (id, -1, G_NORMALIZE_ALL_COMPOSE);

  return TRUE;
}

static GVariant *
account_param_filter (const TpCMParamSpec *paramspec,
                      GVariant *value,
                      gpointer user_data,
                      GError **error)
{
  const gchar *id = g_variant_get_string (value, NULL);

  if (example_csh_protocol_check_contact_id (id, NULL, error))
    return value;

  return NULL;
}

static GPtrArray *
dup_parameters (TpBaseProtocol *base)
{
  ExampleCSHProtocol *self = (ExampleCSHProtocol *) base;

  if (self->priv->params == NULL)
    {
      self->priv->params = g_ptr_array_new_full (2,
          (GDestroyNotify) tp_cm_param_spec_unref);

      g_ptr_array_add (self->priv->params,
          tp_cm_param_spec_new ("account",
              TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER,
              g_variant_new_string (""),
              account_param_filter, NULL, NULL));

      g_ptr_array_add (self->priv->params,
          tp_cm_param_spec_new ("simulation-delay",
              TP_CONN_MGR_PARAM_FLAG_HAS_DEFAULT,
              g_variant_new_uint32 (500),
              NULL, NULL, NULL));
    }

  return g_ptr_array_ref (self->priv->params);
}

static TpBaseConnection *
new_connection (TpBaseProtocol *protocol,
    GHashTable *asv,
    GError **error)
{
  ExampleCSHConnection *conn;
  const gchar *account;
  guint sim_delay;

  account = tp_asv_get_string (asv, "account");
  /* telepathy-glib checked this for us */
  g_assert (account != NULL);

  sim_delay = tp_asv_get_uint32 (asv, "simulation-delay", NULL);

  conn = EXAMPLE_CSH_CONNECTION (
      g_object_new (EXAMPLE_TYPE_CSH_CONNECTION,
        "account", account,
        "protocol", tp_base_protocol_get_name (protocol),
        "simulation-delay", sim_delay,
        NULL));

  return (TpBaseConnection *) conn;
}

static gchar *
normalize_contact (TpBaseProtocol *self G_GNUC_UNUSED,
    const gchar *contact,
    GError **error)
{
  gchar *normal;

  if (example_csh_protocol_check_contact_id (contact, &normal, error))
    return normal;
  else
    return NULL;
}

static gchar *
identify_account (TpBaseProtocol *self G_GNUC_UNUSED,
    GHashTable *asv,
    GError **error)
{
  const gchar *account = tp_asv_get_string (asv, "account");

  if (account != NULL)
    return normalize_contact (self, account, error);

  g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
      "'account' parameter not given");
  return NULL;
}

static void
get_connection_details (TpBaseProtocol *self G_GNUC_UNUSED,
    GStrv *connection_interfaces,
    GType **channel_managers,
    gchar **icon_name,
    gchar **english_name,
    gchar **vcard_field)
{
  if (connection_interfaces != NULL)
    {
      *connection_interfaces = g_strdupv (
          (GStrv) example_csh_connection_get_possible_interfaces ());
    }

  if (channel_managers != NULL)
    {
      GType types[] = { EXAMPLE_TYPE_CSH_ROOM_MANAGER, G_TYPE_INVALID };

      *channel_managers = g_memdup (types, sizeof (types));
    }

  if (icon_name != NULL)
    *icon_name = g_strdup ("face-smile");

  if (english_name != NULL)
    *english_name = g_strdup ("Example with channel-specific handles");

  if (vcard_field != NULL)
    *vcard_field = g_strdup ("x-telepathy-example");
}

static void
finalize (GObject *object)
{
  ExampleCSHProtocol *self = (ExampleCSHProtocol *) object;

  g_clear_pointer (&self->priv->params, g_ptr_array_unref);

  G_OBJECT_CLASS (example_csh_protocol_parent_class)->finalize (object);
}

static void
example_csh_protocol_class_init (
    ExampleCSHProtocolClass *klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;
  TpBaseProtocolClass *base_class = (TpBaseProtocolClass *) klass;

  g_type_class_add_private (klass, sizeof (ExampleCSHProtocolPrivate));

  oclass->finalize = finalize;

  base_class->dup_parameters = dup_parameters;
  base_class->new_connection = new_connection;

  base_class->normalize_contact = normalize_contact;
  base_class->identify_account = identify_account;
  base_class->get_connection_details = get_connection_details;
}
