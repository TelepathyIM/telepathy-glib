/*
 * voip-engine-main.c - startup and shutdown of voip-engine
 * Copyright (C) 2005 Collabora Ltd.
 * Copyright (C) 2005 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <dbus/dbus-glib.h>
#include <gst/gst.h>
#include "tp-voip-engine.h"
#include "common/telepathy-errors.h"
#include "common/telepathy-errors-enumtypes.h"

GSource *timeout = NULL;
GMainLoop *mainloop = NULL;
TpVoipEngine *voip_engine = NULL;
gboolean connections_exist = FALSE;
guint timeout_id;

#define DIE_TIME 5000

static gboolean
kill_voip_engine (gpointer data)
{
  if (!connections_exist)
    {
      g_debug("no channels are being handled, and timed out");
      g_object_unref (voip_engine);
      g_main_loop_quit (mainloop);
    }

  return FALSE;
}

static void
handling_channel (TpVoipEngine *voip_engine)
{
  connections_exist = TRUE;
  g_source_remove (timeout_id);
}

static void
no_more_channels (TpVoipEngine *voip_engine)
{
  if (g_main_context_find_source_by_id (g_main_loop_get_context (mainloop),
                                        timeout_id))
    {
      g_source_remove (timeout_id);
    }
  connections_exist = FALSE;
  timeout_id = g_timeout_add(DIE_TIME, kill_voip_engine, NULL);
}

int main(int argc, char **argv) {
  g_type_init();
  gst_init (&argc, &argv);

  {
    GLogLevelFlags fatal_mask;

    fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
    fatal_mask |= G_LOG_LEVEL_CRITICAL;
    g_log_set_always_fatal (fatal_mask);
  }

  g_set_prgname("telepathy-voip-engine");

  mainloop = g_main_loop_new (NULL, FALSE);

  dbus_g_error_domain_register (TELEPATHY_ERRORS, "org.freedesktop.Telepathy.Error", TELEPATHY_TYPE_ERRORS);

  voip_engine = g_object_new (TP_TYPE_VOIP_ENGINE, NULL);

  g_signal_connect (voip_engine, "handling-channel", 
                    (GCallback) handling_channel, NULL);

  g_signal_connect (voip_engine, "no-more-channels", 
                    (GCallback) no_more_channels, NULL);

  _tp_voip_engine_register (voip_engine);

  timeout_id = g_timeout_add(DIE_TIME, kill_voip_engine, NULL);

  g_debug("started");
  g_main_loop_run (mainloop);

  return 0;
}
