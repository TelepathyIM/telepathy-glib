/*
 * approver
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <glib.h>
#include <stdio.h>

#include <telepathy-glib/telepathy-glib.h>

GMainLoop *mainloop = NULL;

static void
cdo_finished_cb (TpProxy *self,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  g_print ("ChannelDispatchOperation has been invalidated\n");

  g_object_unref (self);
  g_main_loop_quit (mainloop);
}

static void
handle_with_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TpChannelDispatchOperation *cdo = TP_CHANNEL_DISPATCH_OPERATION (source);
  GError *error;

  if (!tp_channel_dispatch_operation_handle_with_finish (cdo, result, &error))
    {
      g_print ("HandleWith() failed: %s\n", error->message);
      g_error_free (error);
      return;
    }

  g_print ("HandleWith() succeeded\n");
}

static void
close_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)

{
  TpChannelDispatchOperation *cdo = TP_CHANNEL_DISPATCH_OPERATION (source);
  GError *error;

  if (!tp_channel_dispatch_operation_close_channels_finish (cdo, result, &error))
    {
      g_print ("Rejecting channels failed: %s\n", error->message);
      g_error_free (error);
      return;
    }

  g_print ("Rejected all the things!\n");
}


static void
add_dispatch_operation_cb (TpSimpleApprover *self,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *cdo,
    TpAddDispatchOperationContext *context,
    gpointer user_data)
{
  GList *l;
  GStrv possible_handlers;
  gchar c;

  g_print ("Approving this batch of channels:\n");

  g_signal_connect (cdo, "invalidated", G_CALLBACK (cdo_finished_cb), NULL);

  for (l = channels; l != NULL; l = g_list_next (l))
    {
      TpChannel *channel = l->data;

      g_print ("%s channel with %s\n", tp_channel_get_channel_type (channel),
          tp_channel_get_identifier (channel));
    }

  possible_handlers = tp_channel_dispatch_operation_get_possible_handlers (
      cdo);
  if (possible_handlers[0] == NULL)
    {
      g_print ("\nNo possible handler suggested\n");
    }
  else
    {
      guint i;

      g_print ("\npossible handlers:\n");
      for (i = 0; possible_handlers[i] != NULL; i++)
        g_print ("  %s\n", possible_handlers[i]);
    }

  g_object_ref (cdo);

  tp_add_dispatch_operation_context_accept (context);

  g_print ("Approve? [y/n]\n");

  c = fgetc (stdin);

  if (c == 'y' || c == 'Y')
    {
      g_print ("Approve channels\n");

      tp_channel_dispatch_operation_handle_with_async (cdo, NULL,
          handle_with_cb, NULL);
    }
  else if (c == 'n' || c == 'N')
    {
      g_print ("Reject channels\n");

      tp_channel_dispatch_operation_close_channels_async (cdo, close_cb, NULL);
    }
  else
    {
      g_print ("Ignore channels\n");
    }
}

int
main (int argc,
      char **argv)
{
  TpAccountManager *manager;
  GError *error = NULL;
  TpBaseClient *approver;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  manager = tp_account_manager_dup ();
  approver = tp_simple_approver_new_with_am (manager, "ExampleApprover",
      FALSE, add_dispatch_operation_cb, NULL, NULL);

  /* contact text chat */
  tp_base_client_take_approver_filter (approver, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  /* call */
  tp_base_client_take_approver_filter (approver, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_CALL,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  /* room text chat */
  tp_base_client_take_approver_filter (approver, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_ROOM,
        NULL));

  /* file transfer */
  tp_base_client_take_approver_filter (approver, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_FILE_TRANSFER,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  if (!tp_base_client_register (approver, &error))
    {
      g_warning ("Failed to register Approver: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  g_print ("Start approving\n");

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

out:
  g_object_unref (manager);
  g_object_unref (approver);

  return 0;
}
