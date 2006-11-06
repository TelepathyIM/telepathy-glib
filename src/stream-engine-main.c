/*
 * stream-engine-main.c - startup and shutdown of stream-engine
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

#include "config.h"

#define USE_REALTIME
#ifdef USE_REALTIME
#include <sched.h>
#include <sys/mman.h>
#endif /* USE_REALTIME */

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif /* HAVE_EXECINFO_H */

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <dbus/dbus-glib.h>
#include <gst/gst.h>
#include "tp-stream-engine.h"
#include "common/telepathy-errors.h"
#include "common/telepathy-errors-enumtypes.h"

GSource *timeout = NULL;
GMainLoop *mainloop = NULL;
TpStreamEngine *stream_engine = NULL;
gboolean connections_exist = FALSE;
guint timeout_id;
gboolean forced_exit_in_progress = FALSE;

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
kill_stream_engine (gpointer data)
{
  if (!g_getenv ("STREAM_ENGINE_PERSIST") && !connections_exist)
    {
      g_debug("no channels are being handled, and timed out");
      g_object_unref (stream_engine);
      g_main_loop_quit (mainloop);
    }

  return FALSE;
}

static void
handling_channel (TpStreamEngine *stream_engine)
{
  connections_exist = TRUE;
  g_source_remove (timeout_id);
}

static void
no_more_channels (TpStreamEngine *stream_engine)
{
  if (g_main_context_find_source_by_id (g_main_loop_get_context (mainloop),
                                        timeout_id))
    {
      g_source_remove (timeout_id);
    }
  connections_exist = FALSE;
  timeout_id = g_timeout_add(DIE_TIME, kill_stream_engine, NULL);
}

static void
shutdown (TpStreamEngine *stream_engine)
{
  g_debug ("Unrefing stream_engine and quitting");
  g_object_unref (stream_engine);
  g_main_loop_quit (mainloop);
}

static void
dsp_crashed (gpointer dummy)
{
  if (stream_engine)
  {
    tp_stream_engine_error (stream_engine, 0, "DSP Crash");
    g_object_unref (stream_engine);
    g_main_loop_quit (mainloop);
  }
}

static void
got_sigbus (int i)
{
  g_warning ("DSP Crashed");
  if (!forced_exit_in_progress)
    {
      forced_exit_in_progress =TRUE;
      g_idle_add ((GSourceFunc) dsp_crashed, NULL);
    }
}

static void
got_segv (int id)
{
  signal (SIGSEGV, SIG_IGN);
  g_warning ("Stream Engine caught SIGSEGV!");
  /* FIXME
  _tp_stream_engine_stop_stream(stream_engine);
  */
  g_object_unref (stream_engine);
  g_main_loop_quit (mainloop);
}

static void
print_backtrace (void)
{
#if defined (HAVE_BACKTRACE) && defined (HAVE_BACKTRACE_SYMBOLS_FD)
  void *array[20];
  size_t size;

#define MSG "\n########## Backtrace (version " VERSION ") ##########\n"
  write (STDERR_FILENO, MSG, strlen (MSG));
#undef MSG

  size = backtrace (array, 20);
  backtrace_symbols_fd (array, size, STDERR_FILENO);
#endif /* HAVE_BACKTRACE && HAVE_BACKTRACE_SYMBOLS_FD */
}

static void
critical_handler (const gchar *log_domain,
                  GLogLevelFlags log_level,
                  const gchar *message,
                  gpointer user_data)
{
  g_log_default_handler (log_domain, log_level, message, user_data);
  print_backtrace ();
}

int main(int argc, char **argv) {
  signal (SIGBUS, got_sigbus);
  signal (SIGSEGV, got_segv);
  g_type_init();
  gst_init (&argc, &argv);

  {
    GLogLevelFlags fatal_mask;

    fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
    fatal_mask |= G_LOG_LEVEL_CRITICAL;
    g_log_set_always_fatal (fatal_mask);

    g_log_set_handler ("GLib-GObject",
        G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR |
        G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        critical_handler, NULL);
    g_log_set_handler ("GLib",
        G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR |
        G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        critical_handler, NULL);
    g_log_set_handler (NULL,
        G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR |
        G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
        critical_handler, NULL);
  }

  g_set_prgname("telepathy-stream-engine");

  mainloop = g_main_loop_new (NULL, FALSE);

  dbus_g_error_domain_register (TELEPATHY_ERRORS, "org.freedesktop.Telepathy.Error", TELEPATHY_TYPE_ERRORS);

  stream_engine = tp_stream_engine_get ();

  g_signal_connect (stream_engine, "handling-channel", 
                    (GCallback) handling_channel, NULL);

  g_signal_connect (stream_engine, "no-more-channels", 
                    (GCallback) no_more_channels, NULL);

  g_signal_connect (stream_engine, "shutdown-requested",
                    (GCallback) shutdown, NULL);

  tp_stream_engine_register (stream_engine);

  timeout_id = g_timeout_add(DIE_TIME, kill_stream_engine, NULL);

#ifdef USE_REALTIME
  {
    int rt_mode;
    char *rt_env;

    /* Here we don't yet have any media threads running, so the to-be-created
     * threads will inherit the scheduling parameters, as glib doesn't know
     * anything about that... */
    rt_env = getenv("STREAM_ENGINE_REALTIME");
    if (rt_env != NULL) {
      if ((rt_mode = atoi(rt_env))) {
        g_debug("realtime scheduling enabled");
        set_realtime(argv[0], rt_mode);
      } else {
        g_debug("realtime scheduling disabled");
      }
    } else {
      g_debug("not using realtime scheduling, enable through STREAM_ENGINE_REALTIME env");
    }
  }
#endif /* USE_REALTIME */

#ifdef MAEMO_OSSO_SUPPORT
  g_debug ("maemo support enabled");
#endif

  g_debug("started");
  g_main_loop_run (mainloop);
  g_debug("finished");

  return 0;
}
