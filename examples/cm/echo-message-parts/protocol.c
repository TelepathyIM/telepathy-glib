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

#include <telepathy-glib/telepathy-glib.h>

#include "conn.h"

#include "_gen/param-spec-struct.h"

G_DEFINE_TYPE (ExampleEcho2Protocol,
    example_echo_2_protocol,
    TP_TYPE_BASE_PROTOCOL)

static void
example_echo_2_protocol_init (
    ExampleEcho2Protocol *self)
{
}

static const TpCMParamSpec *
get_parameters (TpBaseProtocol *self)
{
  return example_echo_2_example_params;
}

static TpBaseConnection *
new_connection (TpBaseProtocol *protocol,
    GHashTable *asv,
    GError **error)
{
  ExampleEcho2Connection *conn;
  const gchar *account;
  gchar *protocol_name;

  account = tp_asv_get_string (asv, "account");

  if (account == NULL || account[0] == '\0')
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "The 'account' parameter is required");
      return NULL;
    }

  g_object_get (protocol,
      "name", &protocol_name,
      NULL);

  conn = EXAMPLE_ECHO_2_CONNECTION (
      g_object_new (EXAMPLE_TYPE_ECHO_2_CONNECTION,
        "account", account,
        "protocol", protocol_name,
        NULL));
  g_free (protocol_name);

  return (TpBaseConnection *) conn;
}

static void
example_echo_2_protocol_class_init (
    ExampleEcho2ProtocolClass *klass)
{
  TpBaseProtocolClass *base_class =
      (TpBaseProtocolClass *) klass;

  base_class->get_parameters = get_parameters;
  base_class->new_connection = new_connection;
}
