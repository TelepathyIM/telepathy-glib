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

#include <signal.h>
#define USE_REALTIME
#ifdef USE_REALTIME
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mman.h>
#endif /* USE_REALTIME */
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

#ifdef USE_REALTIME
#define DEF_PRIORITY_POLICY SCHED_RR
#define PRIORITY_DELTA 1

static void
set_realtime (const char *argv0, int policy) {
  int orig_uid, orig_euid;
  int prio_policy;
  int prio_delta = PRIORITY_DELTA;
  struct sched_param schedp;

  /* get original uid */
  orig_uid = getuid();
  orig_euid = geteuid();
  /* set uid to root */
  if (setreuid(orig_uid, 0) == -1) {
    perror("setreuid()");
    g_warning("unable to setreuid(,0), maybe you should: \n");
    g_warning("\tchown root %s ; chmod u+s %s\n", argv0, argv0);
  }
  /* set scheduling parameters, scheduler either SCHED_RR or SCHED_FIFO */
  switch (policy) {
    case 1:
      prio_policy = SCHED_RR;
      break;
    case 2:
      prio_policy = SCHED_FIFO;
      break;
    default:
      prio_policy = DEF_PRIORITY_POLICY;
  }
  memset(&schedp, 0x00, sizeof(schedp));
  schedp.sched_priority = sched_get_priority_min(prio_policy) + prio_delta;
  /* 0 pid equals to getpid() ie. current process */
  if (sched_setscheduler(0, prio_policy, &schedp) == -1) {
    perror("sched_setscheduler()");
  }
  /* nail everything to RAM, needed for realtime on systems with swap,
   * also avoids extra calls to vm subsystem */
  /*if (mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
    perror("mlockall()");
  }*/
  /* restore original uid */
  setreuid(orig_uid, orig_euid);
}
#endif /* USE_REALTIME */

static gboolean
kill_voip_engine (gpointer data)
{
  if (!g_getenv ("VOIP_ENGINE_PERSIST") && !connections_exist)
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

static void
got_sigbus (int i)
{
  g_warning ("DSP Crashed");
  if (voip_engine)
  {
    _tp_voip_engine_stop_stream(voip_engine);
    _tp_voip_engine_signal_stream_error (voip_engine, 0, "DSP Crash");
    g_object_unref (voip_engine);
    g_main_loop_quit (mainloop);
  }
}

static void
got_segv (int id)
{
  g_warning ("VoIP Engine caught SIGSEGV!");
  _tp_voip_engine_stop_stream(voip_engine);
  g_object_unref (voip_engine);
  g_main_loop_quit (mainloop);
}

int main(int argc, char **argv) {
  int rt_mode;
  char *rt_env;

  signal (SIGBUS, got_sigbus);
  signal (SIGSEGV, got_segv);
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

#ifdef USE_REALTIME
  /* Here we don't yet have any media threads running, so the to-be-created
   * threads will inherit the scheduling parameters, as glib doesn't know
   * anything about that... */
  rt_env = getenv("VOIP_ENGINE_REALTIME");
  if (rt_env != NULL) {
    if ((rt_mode = atoi(rt_env))) {
      g_debug("realtime scheduling enabled");
      set_realtime(argv[0], rt_mode);
    } else {
      g_debug("realtime scheduling disabled");
    }
  } else {
    g_debug("not using realtime scheduling, enable through VOIP_ENGINE_REALTIME env");
  }
#endif /* USE_REALTIME */
  g_debug("started");
  g_main_loop_run (mainloop);
  g_debug("finished");

  return 0;
}
