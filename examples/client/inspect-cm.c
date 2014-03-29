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
 * Copyright (C) 2013 Intel Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

static const gchar *
nonnull (const gchar *s)
{
  if (s == NULL)
    return "(null)";

  return s;
}

static void
show_cm (TpConnectionManager *cm)
{
  GList *protocols;

  g_message ("Connection manager name: %s",
      tp_connection_manager_get_name (cm));
  g_message ("Is running: %s",
      tp_connection_manager_is_running (cm) ? "yes" : "no");
  g_message ("Source of information: %s",
      tp_connection_manager_get_info_source (cm) == TP_CM_INFO_SOURCE_LIVE
        ? "D-Bus" : ".manager file");

  protocols = tp_connection_manager_dup_protocols (cm);
  while (protocols)
    {
      TpProtocol *protocol = protocols->data;
      GList *params;
      const gchar *const *strv;
      const gchar *const *strv_iter;
      TpAvatarRequirements *avatar_reqs;

      g_message ("Protocol: %s", tp_protocol_get_name (protocol));

      g_message ("\tEnglish name: %s", tp_protocol_get_english_name (protocol));
      g_message ("\tIcon name: %s", tp_protocol_get_icon_name (protocol));
      g_message ("\tvCard field: %s",
          nonnull (tp_protocol_get_vcard_field (protocol)));
      g_message ("\tCan register accounts via Telepathy: %s",
          tp_protocol_can_register (protocol) ? "yes" : "no");

      strv = tp_protocol_get_authentication_types (protocol);

      for (strv_iter = strv;
          strv_iter != NULL && *strv_iter != NULL;
          strv_iter++)
        g_message ("\tAuthentication type: %s", *strv_iter);

      avatar_reqs = tp_protocol_get_avatar_requirements (protocol);

      if (avatar_reqs == NULL)
        {
          g_message ("\tNo known avatar requirements, or no avatar support");
        }
      else
        {
          gboolean first = TRUE;

          g_message ("\tAvatar requirements:");

          for (strv_iter =
                  (const gchar * const *) avatar_reqs->supported_mime_types;
              strv_iter != NULL && *strv_iter != NULL;
              strv_iter++)
            {
              g_message ("\t\t%s MIME type: %s",
                  (first ? "Recommended" : "Supported"),
                  *strv_iter);
              first = FALSE;
            }

          g_message ("\t\tMinimum: %ux%u px",
              avatar_reqs->minimum_width,
              avatar_reqs->minimum_height);
          g_message ("\t\tRecommended: %ux%u px",
              avatar_reqs->recommended_width,
              avatar_reqs->recommended_height);
          g_message ("\t\tMaximum: %ux%u px, %u bytes",
              avatar_reqs->maximum_width,
              avatar_reqs->maximum_height,
              avatar_reqs->maximum_bytes);
        }

      params = tp_protocol_dup_params (protocol);
      while (params)
        {
          TpConnectionManagerParam *param = params->data;
          GValue value = { 0 };

          g_message ("\tParameter: %s",
              tp_connection_manager_param_get_name (param));
          g_message ("\t\tD-Bus signature: %s",
              tp_connection_manager_param_get_dbus_signature (param));
          g_message ("\t\tIs required: %s",
              tp_connection_manager_param_is_required (param) ?
                "yes" : "no");

          if (tp_protocol_can_register (protocol))
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

          tp_connection_manager_param_free (param);
          params = g_list_delete_link (params, params);
        }

      g_object_unref (protocol);
      protocols = g_list_delete_link (protocols, protocols);
    }
}

static void
list_cb (GObject *source G_GNUC_UNUSED,
    GAsyncResult *result,
    gpointer user_data)
{
  GMainLoop *mainloop = user_data;
  GError *error = NULL;
  GList *cms = tp_list_connection_managers_finish (result, &error);

  if (error != NULL)
    {
      g_warning ("Error getting list of CMs: %s", error->message);
      g_error_free (error);
    }
  else if (cms == NULL)
    {
      g_message ("No Telepathy connection managers found");
    }
  else
    {
      while (cms != NULL)
        {
          show_cm (cms->data);
          g_object_unref (cms->data);
          cms = g_list_delete_link (cms, cms);

          if (cms != NULL)
            g_message ("----------------------------------------");
        }
    }

  g_main_loop_quit (mainloop);
}

static void
ready (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpConnectionManager *cm = TP_CONNECTION_MANAGER (source);
  GMainLoop *mainloop = user_data;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (cm, result, &error))
    {
      g_assert (!tp_proxy_is_prepared (cm,
            TP_CONNECTION_MANAGER_FEATURE_CORE));

      g_warning ("Error getting CM info: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_assert (tp_proxy_is_prepared (cm,
            TP_CONNECTION_MANAGER_FEATURE_CORE));

      show_cm (cm);
    }

  g_main_loop_quit (mainloop);
}

int
main (int argc,
      char **argv)
{
  const gchar *cm_name, *manager_file;
  TpClientFactory *factory = NULL;
  TpConnectionManager *cm = NULL;
  GMainLoop *mainloop = NULL;
  GError *error = NULL;
  int ret = 1;

  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (g_getenv ("EXAMPLE_TIMING") != NULL)
    g_log_set_default_handler (tp_debug_timestamped_log_handler, NULL);

  factory = tp_client_factory_dup (&error);
  if (factory == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      goto out;
    }

  mainloop = g_main_loop_new (NULL, FALSE);

  if (argc >= 2)
    {
      cm_name = argv[1];
      manager_file = argv[2];   /* possibly NULL */

      cm = tp_connection_manager_new (dbus, cm_name, manager_file, &error);

      if (cm == NULL)
        {
          g_warning ("%s", error->message);
          g_error_free (error);
          goto out;
        }

      tp_proxy_prepare_async (cm, NULL, ready, mainloop);
    }
  else
    {
      tp_list_connection_managers_async (factory, list_cb, mainloop);
    }

  g_main_loop_run (mainloop);
  ret = 0;

out:
  if (cm != NULL)
    g_object_unref (cm);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

  if (factory != NULL)
    g_object_unref (factory);

  return ret;
}
