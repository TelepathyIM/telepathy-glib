/*
 * media-observer - Observe media channels
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <glib.h>

#include <telepathy-glib/telepathy-glib.h>

static void
chan_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  TpChannel *channel = TP_CHANNEL (proxy);

  g_print ("Call with %s terminated\n", tp_channel_get_identifier (channel));

  g_object_unref (channel);
}

static void
observe_channels_cb (TpSimpleObserver *self,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context,
    gpointer user_data)
{
  GList *l;
  gboolean recovering;

  recovering = tp_observe_channels_context_is_recovering (context);

  for (l = channels; l != NULL; l = g_list_next (l))
    {
      TpChannel *channel = l->data;
      gboolean requested;

      if (tp_strdiff (tp_channel_get_channel_type (channel),
            TP_IFACE_CHANNEL_TYPE_CALL))
        continue;

      requested = tp_channel_get_requested (channel);

      g_print ("Observing %s %s call %s %s\n",
          recovering? "existing": "new",
          requested? "outgoing": "incoming",
          requested? "to": "from",
          tp_channel_get_identifier (channel));

      g_signal_connect (g_object_ref (channel), "invalidated",
          G_CALLBACK (chan_invalidated_cb), NULL);
    }

  tp_observe_channels_context_accept (context);
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpAccountManager *manager;
  GError *error = NULL;
  TpBaseClient *observer;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  manager = tp_account_manager_dup ();
  observer = tp_simple_observer_new_with_am (manager, FALSE,
      "ExampleMediaObserver", FALSE, observe_channels_cb, NULL, NULL);

  tp_base_client_take_observer_filter (observer, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_CALL,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  if (!tp_base_client_register (observer, &error))
    {
      g_warning ("Failed to register Observer: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  g_print ("Start observing\n");

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

out:
  g_object_unref (manager);
  g_object_unref (observer);

  return 0;
}
