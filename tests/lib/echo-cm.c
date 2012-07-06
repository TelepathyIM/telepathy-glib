/*
 * manager.c - an example connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include "echo-cm.h"

#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>

#include "echo-conn.h"

G_DEFINE_TYPE (TpTestsEchoConnectionManager,
    tp_tests_echo_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

/* type definition stuff */

static void
tp_tests_echo_connection_manager_init (TpTestsEchoConnectionManager *self)
{
}

/* private data */

typedef struct {
    gchar *account;
} ExampleParams;

static const TpCMParamSpec tp_tests_echo_example_params[] = {
  { "account", "s", G_TYPE_STRING,
    TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER,
    NULL, /* default */
    G_STRUCT_OFFSET (ExampleParams, account), /* struct offset */
    tp_cm_param_filter_string_nonempty, /* filter */
    NULL, /* filter data */
    NULL /* setter data */ },
  { NULL }
};

static gpointer
alloc_params (void)
{
  return g_slice_new0 (ExampleParams);
}

static void
free_params (gpointer p)
{
  ExampleParams *params = p;

  g_free (params->account);

  g_slice_free (ExampleParams, params);
}

static const TpCMProtocolSpec example_protocols[] = {
  { "example", tp_tests_echo_example_params, alloc_params, free_params },
  { NULL, NULL }
};

static TpBaseConnection *
new_connection (TpBaseConnectionManager *self,
                const gchar *proto,
                TpIntset *params_present,
                gpointer parsed_params,
                GError **error)
{
  ExampleParams *params = parsed_params;
  TpTestsEchoConnection *conn = TP_TESTS_ECHO_CONNECTION
      (g_object_new (TP_TESTS_TYPE_ECHO_CONNECTION,
          "account", params->account,
          "protocol", proto,
          NULL));

  return (TpBaseConnection *) conn;
}

static GPtrArray *
get_interfaces (TpBaseConnectionManager *self)
{
  GPtrArray *interfaces;

  interfaces = TP_BASE_CONNECTION_MANAGER_CLASS (
      tp_tests_echo_connection_manager_parent_class)->get_interfaces (self);

  g_ptr_array_add (interfaces, "im.telepathy.Tests.Example");

  return interfaces;
}

static void
tp_tests_echo_connection_manager_class_init (
    TpTestsEchoConnectionManagerClass *klass)
{
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  base_class->new_connection = new_connection;
  base_class->cm_dbus_name = "example_echo";
  base_class->protocol_params = example_protocols;
  base_class->get_interfaces = get_interfaces;
}
