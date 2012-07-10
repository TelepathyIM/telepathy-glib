/*
 * protocol.c - an example Protocol
 *
 * Copyright © 2007-2010 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "protocol.h"

#include <telepathy-glib/telepathy-glib.h>

#include "conn.h"

G_DEFINE_TYPE (ExampleExtendedProtocol,
    example_extended_protocol,
    TP_TYPE_BASE_PROTOCOL)

static void
example_extended_protocol_init (
    ExampleExtendedProtocol *self)
{
}

static const TpCMParamSpec example_extended_example_params[] = {
  { "account", "s", G_TYPE_STRING,
    TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER,
    NULL, /* no default */
    0, /* formerly struct offset, now unused */
    tp_cm_param_filter_string_nonempty, /* filter - empty strings disallowed */
    NULL, /* filter data, unused for our filter */
    NULL /* setter data, now unused */ },
  { NULL }
};

static const TpCMParamSpec *
get_parameters (TpBaseProtocol *self)
{
  return example_extended_example_params;
}

static TpBaseConnection *
new_connection (TpBaseProtocol *protocol,
    GHashTable *asv,
    GError **error)
{
  ExampleExtendedConnection *conn;
  const gchar *account;

  account = tp_asv_get_string (asv, "account");

  if (account == NULL || account[0] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_ARGUMENT,
          "The 'account' parameter is required");
      return NULL;
    }

  conn = EXAMPLE_EXTENDED_CONNECTION (
      g_object_new (EXAMPLE_TYPE_EXTENDED_CONNECTION,
        "account", account,
        "protocol", tp_base_protocol_get_name (protocol),
        NULL));

  return (TpBaseConnection *) conn;
}

gchar *
example_extended_protocol_normalize_contact (const gchar *id,
    GError **error)
{
  if (id[0] == '\0')
    {
      g_set_error (error, TP_ERROR, TP_ERROR_INVALID_HANDLE,
          "ID must not be empty");
      return NULL;
    }

  return g_utf8_strdown (id, -1);
}

static gchar *
normalize_contact (TpBaseProtocol *self G_GNUC_UNUSED,
    const gchar *contact,
    GError **error)
{
  return example_extended_protocol_normalize_contact (contact, error);
}

static gchar *
identify_account (TpBaseProtocol *self G_GNUC_UNUSED,
    GHashTable *asv,
    GError **error)
{
  const gchar *account = tp_asv_get_string (asv, "account");

  if (account != NULL)
    return example_extended_protocol_normalize_contact (account, error);

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
          (GStrv) example_extended_connection_get_possible_interfaces ());
    }

  if (channel_managers != NULL)
    {
      /* we don't have any channel managers */
      GType types[] = { G_TYPE_INVALID };

      *channel_managers = g_memdup (types, sizeof (types));
    }

  if (icon_name != NULL)
    {
      /* a real protocol would use its own icon name, probably im-something -
       * for this example we use an emoticon instead */
      *icon_name = g_strdup ("face-smile");
    }

  if (english_name != NULL)
    {
      /* in a real protocol this would be "ICQ" or
       * "Windows Live Messenger (MSN)" or something */
      *english_name = g_strdup ("Extended (hats) example");
    }

  if (vcard_field != NULL)
    {
      /* in a real protocol this would be "tel" or "x-jabber" or something */
      *vcard_field = g_strdup ("x-telepathy-example");
    }
}

static void
example_extended_protocol_class_init (
    ExampleExtendedProtocolClass *klass)
{
  TpBaseProtocolClass *base_class =
      (TpBaseProtocolClass *) klass;

  base_class->get_parameters = get_parameters;
  base_class->new_connection = new_connection;

  base_class->normalize_contact = normalize_contact;
  base_class->identify_account = identify_account;
  base_class->get_connection_details = get_connection_details;
}
