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

#include "config.h"

#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

typedef struct {
    const gchar *to_inspect;
    int exit_status;
    GMainLoop *main_loop;
} InspectContactData;

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
  InspectContactData *data = user_data;

  if (error == NULL)
    {
      guint i;

      data->exit_status = 0;

      for (i = 0; i < n_contacts; i++)
        {
          display_contact (contacts[i]);
        }

      for (i = 0; i < n_invalid; i++)
        {
          g_warning ("Invalid handle %u", invalid[i]);
          data->exit_status = 1;
        }
    }
  else
    {
      g_warning ("Error getting contacts: %s", error->message);
      data->exit_status = 1;
    }

  g_main_loop_quit (data->main_loop);
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
  InspectContactData *data = user_data;

  if (error == NULL)
    {
      guint i;
      GHashTableIter hash_iter;
      gpointer key, value;

      data->exit_status = 0;

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
          data->exit_status = 1;
        }
    }
  else
    {
      g_warning ("Error getting contacts: %s", error->message);
      data->exit_status = 1;
    }

  g_main_loop_quit (data->main_loop);
}

static void
connection_ready_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  static TpContactFeature features[] = {
      TP_CONTACT_FEATURE_ALIAS,
      TP_CONTACT_FEATURE_AVATAR_TOKEN,
      TP_CONTACT_FEATURE_PRESENCE
  };
  InspectContactData *data = user_data;
  TpConnection *connection = TP_CONNECTION (source);
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (connection, result, &error))
    {
      g_warning ("%s", error->message);
      data->exit_status = 1;
      g_main_loop_quit (data->main_loop);
      g_clear_error (&error);
      return;
    }

  if (data->to_inspect == NULL)
    {
      TpHandle self_handle = tp_connection_get_self_handle (connection);

      tp_connection_get_contacts_by_handle (connection,
          1, &self_handle,
          G_N_ELEMENTS (features), features,
          got_contacts_by_handle,
          data, NULL, NULL);
    }
  else
    {
      const gchar *contacts[] = { data->to_inspect, NULL };

      tp_connection_get_contacts_by_id (connection,
          1, contacts,
          G_N_ELEMENTS (features), features,
          got_contacts_by_id,
          data, NULL, NULL);
    }
}

int
main (int argc,
      char **argv)
{
  const gchar *bus_name, *object_path;
  TpConnection *connection = NULL;
  InspectContactData data = { NULL, 1, NULL };
  TpDBusDaemon *dbus = NULL;
  GError *error = NULL;

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

  data.to_inspect = argv[2];

  dbus = tp_dbus_daemon_dup (&error);

  if (dbus == NULL)
    {
      g_warning ("%s", error->message);
      goto out;
    }

  connection = tp_connection_new (dbus, bus_name, object_path, &error);

  if (connection == NULL)
    {
      g_warning ("%s", error->message);
      goto out;
    }

  data.main_loop = g_main_loop_new (NULL, FALSE);

  /* for this example I assume it's an existing connection on which someone
   * else has called (or will call) Connect(), so we won't call Connect()
   * on it ourselves
   */
  tp_proxy_prepare_async (connection, NULL, connection_ready_cb, &data);

  g_main_loop_run (data.main_loop);

out:
  if (error != NULL)
    g_error_free (error);

  if (data.main_loop != NULL)
    g_main_loop_unref (data.main_loop);

  if (connection != NULL)
    g_object_unref (connection);

  if (dbus != NULL)
    g_object_unref (dbus);

  return data.exit_status;
}
