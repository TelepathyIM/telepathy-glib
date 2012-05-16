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
contacts_upgraded_cb (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) object;
  InspectContactData *data = user_data;
  GPtrArray *contacts;
  GError *error = NULL;

  if (!tp_connection_upgrade_contacts_finish (connection, result,
          &contacts, &error))
    {
      g_warning ("Error getting contacts: %s", error->message);
      data->exit_status = 1;
      g_clear_error (&error);
    }
  else
    {
      guint i;

      data->exit_status = 0;

      for (i = 0; i < contacts->len; i++)
        {
          display_contact (g_ptr_array_index (contacts, i));
        }
      g_ptr_array_unref (contacts);
    }

  g_main_loop_quit (data->main_loop);
}

static void
got_contacts_by_id (GObject *object,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnection *connection = (TpConnection *) object;
  InspectContactData *data = user_data;
  TpContact *contact;
  GError *error = NULL;

  contact = tp_connection_dup_contact_by_id_finish (connection, result, &error);

  if (contact == NULL)
    {
      g_warning ("Error getting contacts: %s", error->message);
      data->exit_status = 1;
      g_clear_error (&error);
    }
  else
    {
      data->exit_status = 0;

      display_contact (contact);
      g_object_unref (contact);
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
      TpContact *self_contact = tp_connection_get_self_contact (connection);

      tp_connection_upgrade_contacts_async (connection,
          1, &self_contact,
          G_N_ELEMENTS (features), features,
          contacts_upgraded_cb,
          data);
    }
  else
    {
      tp_connection_dup_contact_by_id_async (connection,
          data->to_inspect,
          G_N_ELEMENTS (features), features,
          got_contacts_by_id,
          data);
    }
}

int
main (int argc,
      char **argv)
{
  TpConnection *connection = NULL;
  InspectContactData data = { NULL, 1, NULL };
  TpSimpleClientFactory *factory;
  GError *error = NULL;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 2)
    {
      fputs ("Usage:\n"
          "    telepathy-example-inspect-connection OBJECT_PATH [CONTACT_ID]\n",
          stderr);
      return 2;
    }

  data.to_inspect = argv[2];

  factory = tp_simple_client_factory_new (NULL);
  connection = tp_simple_client_factory_ensure_connection (factory,
      argv[1], NULL, &error);

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

  g_object_unref (factory);

  return data.exit_status;
}
