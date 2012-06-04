/*
 * telepathy-example-list-managers - list installed connection managers
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/telepathy-glib.h>

typedef struct {
    GMainLoop *mainloop;
    int exit_code;
} ExampleData;

static void
got_connection_managers (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  ExampleData *data = user_data;
  GList *cms;
  GError *error = NULL;

  cms = tp_list_connection_managers_finish (result, &error);
  if (error != NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      data->exit_code = 1;
    }
  else
    {
      g_message ("Found %u connection managers:", g_list_length (cms));

      while (cms != NULL)
        {
          TpConnectionManager *cm = cms->data;

          g_message ("- %s", tp_connection_manager_get_name (cm));

          g_object_unref (cm);
          cms = g_list_delete_link (cms, cms);
        }
    }

  g_main_loop_quit (data->mainloop);
}

int
main (int argc,
      char **argv)
{
  ExampleData data = { g_main_loop_new (NULL, FALSE), 0 };
  TpDBusDaemon *bus_daemon;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  bus_daemon = tp_dbus_daemon_dup (&error);

  if (bus_daemon == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      data.exit_code = 1;
      goto out;
    }

  tp_list_connection_managers_async (bus_daemon,
      got_connection_managers, &data);

  g_main_loop_run (data.mainloop);

out:
  if (data.mainloop != NULL)
    g_main_loop_unref (data.mainloop);

  if (bus_daemon != NULL)
    g_object_unref (bus_daemon);

  return data.exit_code;
}
