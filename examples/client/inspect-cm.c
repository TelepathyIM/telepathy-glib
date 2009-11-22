/*
 * telepathy-example-inspect-cm - inspect a connection manager
 *
 * Usage:
 *
 * telepathy-example-inspect-cm gabble
 *    Inspect the Gabble connection manager, by reading the installed
 *    .manager file if available, and introspecting the running CM if not
 *
 * telepathy-example-inspect-cm gabble data/gabble.manager
 *    As above, but assume the given filename is correct
 *
 * telepathy-example-inspect-cm gabble ""
 *    Don't read any .manager file, just introspect the running CM
 *
 * Copyright (C) 2007 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

static void
ready (TpConnectionManager *cm,
       const GError *error,
       gpointer user_data,
       GObject *weak_object G_GNUC_UNUSED)
{
  GMainLoop *mainloop = user_data;

  if (error != NULL)
    {
      g_assert (!tp_connection_manager_is_ready (cm));

      g_warning ("Error getting CM info: %s", error->message);
    }
  else
    {
      gchar **protocols;
      guint i;

      g_assert (tp_connection_manager_is_ready (cm));

      g_message ("Connection manager name: %s",
          tp_connection_manager_get_name (cm));
      g_message ("Is running: %s",
          tp_connection_manager_is_running (cm) ? "yes" : "no");
      g_message ("Source of information: %s",
          tp_connection_manager_get_info_source (cm) == TP_CM_INFO_SOURCE_LIVE
            ? "D-Bus" : ".manager file");

      protocols = tp_connection_manager_dup_protocol_names (cm);

      for (i = 0; protocols != NULL && protocols[i] != NULL; i++)
        {
          const TpConnectionManagerProtocol *protocol;
          gchar **params;
          guint j;

          g_message ("Protocol: %s", protocols[i]);
          protocol = tp_connection_manager_get_protocol (cm, protocols[i]);
          g_assert (protocol != NULL);

          g_message ("\tCan register accounts via Telepathy: %s",
              tp_connection_manager_protocol_can_register (protocol) ?
                "yes" : "no");

          params = tp_connection_manager_protocol_dup_param_names (protocol);

          for (j = 0; params != NULL && params[j] != NULL; j++)
            {
              const TpConnectionManagerParam *param;
              GValue value = { 0 };

              g_message ("\tParameter: %s", params[j]);
              param = tp_connection_manager_protocol_get_param (protocol,
                  params[j]);
              g_message ("\t\tD-Bus signature: %s",
                  tp_connection_manager_param_get_dbus_signature (param));
              g_message ("\t\tIs required: %s",
                  tp_connection_manager_param_is_required (param) ?
                    "yes" : "no");

              if (tp_connection_manager_protocol_can_register (protocol))
                {
                  g_message ("\t\tIs required for registration: %s",
                    tp_connection_manager_param_is_required_for_registration (
                        param) ?  "yes" : "no");
                }

              g_message ("\t\tIs secret (password etc.): %s",
                  tp_connection_manager_param_is_secret (param) ?
                    "yes" : "no");
              g_message ("\t\tIs a D-Bus property: %s",
                  tp_connection_manager_param_is_dbus_property (param) ?
                    "yes" : "no");

              if (tp_connection_manager_param_get_default (param, &value))
                {
                  gchar *s = g_strdup_value_contents (&value);

                  g_message ("\t\tDefault value: %s", s);
                  g_free (s);
                  g_value_unset (&value);
                }
              else
                {
                  g_message ("\t\tNo default value");
                }
            }

          g_strfreev (params);
        }

      g_strfreev (protocols);
    }

  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  const gchar *cm_name, *manager_file;
  TpConnectionManager *cm = NULL;
  GMainLoop *mainloop = NULL;
  GError *error = NULL;
  TpDBusDaemon *daemon = NULL;
  int ret = 1;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (argc < 2)
    return 2;

  daemon = tp_dbus_daemon_dup (&error);

  if (daemon == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      goto out;
    }

  mainloop = g_main_loop_new (NULL, FALSE);

  cm_name = argv[1];
  manager_file = argv[2];   /* possibly NULL */

  cm = tp_connection_manager_new (daemon, cm_name, manager_file, &error);

  if (cm == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      goto out;
    }

  tp_connection_manager_call_when_ready (cm, ready, mainloop, NULL, NULL);
  g_main_loop_run (mainloop);
  ret = 0;

out:
  if (cm != NULL)
    g_object_unref (cm);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

  if (daemon != NULL)
    g_object_unref (daemon);

  return ret;
}
