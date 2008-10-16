/*
 * telepathy-example-inspect-contact - inspect a contact on a connection
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <stdio.h>

#include <telepathy-glib/contact.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>

static const gchar *
nonnull (const gchar *s)
{
  return s == NULL ? "(null)" : s;
}

static void
got_contacts_by_handle (TpConnection *connection,
                        guint n_contacts,
                        TpContact * const *contacts,
                        guint n_invalid,
                        const TpHandle *invalid,
                        const GError *error,
                        gpointer user_data,
                        GObject *weak_object)
{
  GMainLoop *mainloop = user_data;

  if (error == NULL)
    {
      guint i;

      g_message ("Got %u contact(s)", n_contacts);

      for (i = 0; i < n_contacts; i++)
        {
          TpContact *contact = contacts[i];

          g_message ("Handle %u, %s:", tp_contact_get_handle (contact),
              tp_contact_get_identifier (contact));
          g_message ("\tAlias: %s", nonnull (tp_contact_get_alias (contact)));
          g_message ("\tAvatar token: %s",
              nonnull (tp_contact_get_avatar_token (contact)));
          g_message ("\tPresence: type #%i %s: %s",
              tp_contact_get_presence_type (contact),
              nonnull (tp_contact_get_presence_status (contact)),
              nonnull (tp_contact_get_avatar_token (contact)));
        }
    }
  else
    {
      g_warning ("Error getting contacts: %s", error->message);
    }

  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  const gchar *bus_name, *object_path;
  TpConnection *connection;
  GMainLoop *mainloop;
  TpDBusDaemon *daemon;
  GError *error = NULL;
#define n_features 1
  static TpContactFeature features[n_features] = {
      TP_CONTACT_FEATURE_ALIAS };

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 2)
    {
      fputs ("Usage:\n"
          "    telepathy-example-inspect-connection OBJECT_PATH [CONTACT_ID]\n"
          "or\n"
          "    telepathy-example-inspect-connection BUS_NAME [CONTACT_ID]\n",
          stderr);
      return 2;
    }

  /* Cope with the first argument being a bus name or an object path */
  if (argv[1][0] == '/')
    {
      object_path = argv[1];
      bus_name = NULL;
    }
  else
    {
      object_path = NULL;
      bus_name = argv[1];
    }

  daemon = tp_dbus_daemon_new (tp_get_bus ());
  connection = tp_connection_new (daemon, bus_name, object_path, &error);

  if (connection == NULL ||
      !tp_connection_run_until_ready (connection, FALSE, &error, NULL))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      g_object_unref (daemon);

      if (connection != NULL)
        g_object_unref (connection);

      return 1;
    }

  g_message ("Connection ready\n");

  mainloop = g_main_loop_new (NULL, FALSE);

  if (argv[2] == NULL)
    {
      guint self_handle;

      if (!tp_cli_connection_run_get_self_handle (connection, -1,
            &self_handle, &error, NULL))
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          g_main_loop_unref (mainloop);
          g_object_unref (connection);
          g_object_unref (daemon);
          return 1;
        }

      tp_connection_get_contacts_by_handle (connection,
          1, &self_handle,
          n_features, features,
          got_contacts_by_handle,
          g_main_loop_ref (mainloop),
          (GDestroyNotify) g_main_loop_unref, NULL);
    }
  else
    {
      g_warning ("Getting contacts by ID not yet implemented");
      g_main_loop_unref (mainloop);
      g_object_unref (connection);
      g_object_unref (daemon);
      return 1;
    }

  g_main_loop_run (mainloop);

  g_main_loop_unref (mainloop);
  g_object_unref (connection);
  g_object_unref (daemon);

  return 0;
}
