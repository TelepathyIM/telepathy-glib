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
#include <telepathy-glib/telepathy-glib-dbus.h>

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
observe_channel_cb (TpSimpleObserver *self,
    TpAccount *account,
    TpConnection *connection,
    TpChannel *channel,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelContext *context,
    gpointer user_data)
{
  gboolean recovering;
  gboolean requested;

  recovering = tp_observe_channel_context_is_recovering (context);
  requested = tp_channel_get_requested (channel);

  g_print ("Observing %s %s call %s %s\n",
      recovering? "existing": "new",
      requested? "outgoing": "incoming",
      requested? "to": "from",
      tp_channel_get_identifier (channel));

  g_signal_connect (g_object_ref (channel), "invalidated",
      G_CALLBACK (chan_invalidated_cb), NULL);

  tp_observe_channel_context_accept (context);
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpAccountManager *manager;
  GError *error = NULL;
  TpBaseClient *observer;

  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  manager = tp_account_manager_dup ();
  observer = tp_simple_observer_new_with_am (manager, FALSE,
      "ExampleMediaObserver", FALSE, observe_channel_cb, NULL, NULL);

  tp_base_client_add_observer_filter (observer,
      g_variant_new_parsed ("{ %s: <%s>, %s: <%u> }",
        TP_PROP_CHANNEL_CHANNEL_TYPE, TP_IFACE_CHANNEL_TYPE_CALL1,
        TP_PROP_CHANNEL_TARGET_ENTITY_TYPE, (guint32) TP_ENTITY_TYPE_CONTACT,
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
