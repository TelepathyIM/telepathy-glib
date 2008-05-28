/*
 * manager.c - an example connection manager
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "manager.h"

#include <string.h>

#include <dbus/dbus-protocol.h>
#include <dbus/dbus-glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/errors.h>

#include "conn.h"

G_DEFINE_TYPE (ExampleCSHConnectionManager,
    example_csh_connection_manager,
    TP_TYPE_BASE_CONNECTION_MANAGER)

/* type definition stuff */

static void
example_csh_connection_manager_init (ExampleCSHConnectionManager *self)
{
}

/* private data */

typedef struct {
    gchar *account;
} ExampleParams;


/* See example_csh_normalize_contact in conn.c. */
static gboolean
account_param_filter (const TpCMParamSpec *paramspec,
                      GValue *value,
                      GError **error)
{
  const gchar *id = g_value_get_string (value);
  const gchar *at;

  if (id[0] == '\0')
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "account must not be empty");
      return FALSE;
    }

  at = strchr (id, '@');

  if (at == NULL || at == id || at[1] == '\0')
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "account must look like aaa@bbb");
      return FALSE;
    }

  if (strchr (at, '#') != NULL)
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_INVALID_ARGUMENT,
          "realm cannot contain '#' except at the beginning");
      return FALSE;
    }

  return TRUE;
}


static const TpCMParamSpec example_params[] = {
  { "account", DBUS_TYPE_STRING_AS_STRING, G_TYPE_STRING,
    TP_CONN_MGR_PARAM_FLAG_REQUIRED | TP_CONN_MGR_PARAM_FLAG_REGISTER, NULL,
    G_STRUCT_OFFSET (ExampleParams, account),
    account_param_filter, NULL },

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
  { "example", example_params, alloc_params, free_params },
  { NULL, NULL }
};

static TpBaseConnection *
new_connection (TpBaseConnectionManager *self,
                const gchar *proto,
                TpIntSet *params_present,
                gpointer parsed_params,
                GError **error)
{
  ExampleParams *params = parsed_params;
  ExampleCSHConnection *conn;

  conn = EXAMPLE_CSH_CONNECTION
      (g_object_new (EXAMPLE_TYPE_CSH_CONNECTION,
          "account", params->account,
          "protocol", proto,
          NULL));

  return (TpBaseConnection *) conn;
}

static void
example_csh_connection_manager_class_init (
    ExampleCSHConnectionManagerClass *klass)
{
  TpBaseConnectionManagerClass *base_class =
      (TpBaseConnectionManagerClass *) klass;

  base_class->new_connection = new_connection;
  base_class->cm_dbus_name = "example_csh";
  base_class->protocol_params = example_protocols;
}
