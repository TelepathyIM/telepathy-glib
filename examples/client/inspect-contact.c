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

#include <telepathy-glib/telepathy-glib.h>

static void
display_contact (TpContact *contact)
{
  const gchar *avatar_token;

  g_message ("Handle %u, \"%s\":", tp_contact_get_handle (contact),
      tp_contact_get_identifier (contact));
  g_message ("\tAlias: \"%s\"", tp_contact_get_alias (contact));

  avatar_token = tp_contact_get_avatar_token (contact);

  if (avatar_token == NULL)
    g_message ("\tAvatar token not known");
  else
    g_message ("\tAvatar token: \"%s\"", avatar_token);

  g_message ("\tPresence: type #%i \"%s\": \"%s\"",
      tp_contact_get_presence_type (contact),
      tp_contact_get_presence_status (contact),
      tp_contact_get_presence_message (contact));
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

      for (i = 0; i < n_contacts; i++)
        {
          display_contact (contacts[i]);
        }

      for (i = 0; i < n_invalid; i++)
        {
          g_warning ("Invalid handle %u", invalid[i]);
        }
    }
  else
    {
      g_warning ("Error getting contacts: %s", error->message);
    }

  g_main_loop_quit (mainloop);
}

static void
got_contacts_by_id (TpConnection *connection,
                    guint n_contacts,
                    TpContact * const *contacts,
                    const gchar * const *good_ids,
                    GHashTable *bad_ids,
                    const GError *error,
                    gpointer user_data,
                    GObject *weak_object)
{
  GMainLoop *mainloop = user_data;

  if (error == NULL)
    {
      guint i;
      GHashTableIter hash_iter;
      gpointer key, value;

      for (i = 0; i < n_contacts; i++)
        {
          display_contact (contacts[i]);
        }

      g_hash_table_iter_init (&hash_iter, bad_ids);

      while (g_hash_table_iter_next (&hash_iter, &key, &value))
        {
          gchar *id = key;
          GError *e = value;

          g_warning ("Invalid ID \"%s\": %s", id, e->message);
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
  TpConnection *connection = NULL;
  GMainLoop *mainloop = NULL;
  TpDBusDaemon *daemon = NULL;
  GError *error = NULL;
  static TpContactFeature features[] = {
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN,
      TP_CONTACT_FEATURE_PRESENCE
  };
  int ret = 1;

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

  daemon = tp_dbus_daemon_dup (&error);

  if (daemon == NULL)
    {
      g_warning ("%s", error->message);
      goto out;
    }

  connection = tp_connection_new (daemon, bus_name, object_path, &error);

  if (connection == NULL ||
      !tp_connection_run_until_ready (connection, FALSE, &error, NULL))
    {
      g_warning ("%s", error->message);
      goto out;
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
          goto out;
        }

      tp_connection_get_contacts_by_handle (connection,
          1, &self_handle,
          sizeof (features) / sizeof (features[0]), features,
          got_contacts_by_handle,
          g_main_loop_ref (mainloop),
          (GDestroyNotify) g_main_loop_unref, NULL);
    }
  else
    {
      const gchar *contacts[] = { argv[2], NULL };

      tp_connection_get_contacts_by_id (connection,
          1, contacts,
          sizeof (features) / sizeof (features[0]), features,
          got_contacts_by_id,
          g_main_loop_ref (mainloop),
          (GDestroyNotify) g_main_loop_unref, NULL);
    }

  g_main_loop_run (mainloop);
  ret = 0;

out:
  if (error != NULL)
    g_error_free (error);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

  if (connection != NULL)
    g_object_unref (connection);

  if (daemon != NULL)
    g_object_unref (daemon);

  return ret;
}
