/*
 * debug.c - Common debug support
 * Copyright (C) 2007 Collabora Ltd.
 * Copyright (C) 2007 Nokia Corporation
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

/**
 * SECTION:debug
 * @title: Common debug support
 * @short_description: API to activate debugging messages from telepathy-glib
 *
 * telepathy-glib has an internal mechanism for debug messages and filtering.
 * Connection managers written with telepathy-glib are expected to connect
 * this to their own debugging mechanisms: when the CM's debugging mechanism
 * is activated, it should call tp_debug_set_flags_from_string(),
 * tp_debug_set_flags_from_env() or tp_debug_set_all_flags().
 *
 * The supported debug-mode keywords are subject to change, but currently
 * include:
 *
 * <itemizedlist>
 * <listitem><literal>manager</literal> - output debug messages regarding
 * #TpConnectionManager (client)</listitem>
 * <listitem><literal>connection</literal> - output debug messages regarding
 * #TpBaseConnection (service) and #TpConnection (client)</listitem>
 * <listitem><literal>channel</literal> - output debug messages regarding
 * #TpChannel (client)</listitem>
 * <listitem><literal>im</literal> - output debug messages regarding
 * (text) instant messaging (service)</listitem>
 * <listitem><literal>properties</literal> - output debug messages regarding
 * #TpPropertiesMixin (service)</listitem>
 * <listitem><literal>params</literal> - output debug messages regarding
 * connection manager parameters (service)</listitem>
 * <listitem><literal>all</literal> - all of the above</listitem>
 * </itemizedlist>
 */
#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <telepathy-glib/debug.h>

#include "debug-internal.h"

#ifdef ENABLE_DEBUG

static TpDebugFlags flags = 0;

static gboolean tp_debug_persistent = FALSE;

/**
 * tp_debug_set_all_flags:
 *
 * Activate all possible debug modes. This also activates persistent mode,
 * which should have been orthogonal.
 *
 * If telepathy-glib was compiled with --disable-debug (not recommended),
 * this function does nothing.
 *
 * @deprecated since 0.6.1. Use tp_debug_set_flags ("all") and
 * tp_debug_set_persistent() instead.
 */
void
tp_debug_set_all_flags (void)
{
  flags = 0xffff;
  tp_debug_persistent = TRUE;
}

static GDebugKey keys[] = {
  { "groups",        TP_DEBUG_GROUPS },
  { "properties",    TP_DEBUG_PROPERTIES },
  { "connection",    TP_DEBUG_CONNECTION },
  { "im",            TP_DEBUG_IM },
  { "params",        TP_DEBUG_PARAMS },
  { "presence",      TP_DEBUG_PRESENCE },
  { "manager",       TP_DEBUG_MANAGER },
  { "channel",       TP_DEBUG_CHANNEL },
  { "proxy",         TP_DEBUG_PROXY },
  { 0, }
};

static GDebugKey persist_keys[] = {
  { "persist",       1 },
  { 0, },
};

/**
 * tp_debug_set_flags:
 * @flags_string: The flags to set, comma-separated. If %NULL or empty,
 *  no additional flags are set.
 *
 * Set the debug flags indicated by @flags_string, in addition to any already
 * set.
 *
 * The parsing matches that of g_parse_debug_string().
 *
 * If telepathy-glib was compiled with --disable-debug (not recommended),
 * this function does nothing.
 *
 * @since 0.6.1
 */
void
tp_debug_set_flags (const gchar *flags_string)
{
  guint nkeys;

  for (nkeys = 0; keys[nkeys].value; nkeys++);

  if (flags_string != NULL)
    _tp_debug_set_flags (g_parse_debug_string (flags_string, keys, nkeys));
}

/**
 * tp_debug_set_flags_from_string:
 * @flags_string: The flags to set, comma-separated. If %NULL or empty,
 *  no additional flags are set.
 *
 * Set the debug flags indicated by @flags_string, in addition to any already
 * set. Unlike tp_debug_set_flags(), this enables persistence like
 * tp_debug_set_persistent() if the "persist" flag is present or the string
 * is "all" - this turns out to be unhelpful, as persistence should be
 * orthogonal.
 *
 * The parsing matches that of g_parse_debug_string().
 *
 * If telepathy-glib was compiled with --disable-debug (not recommended),
 * this function does nothing.
 *
 * @deprecated since 0.6.1. Use tp_debug_set_flags() and
 * tp_debug_set_persistent() instead
 */
void
tp_debug_set_flags_from_string (const gchar *flags_string)
{
  tp_debug_set_flags (flags_string);

  if (flags_string != NULL &&
      g_parse_debug_string (flags_string, persist_keys, 1) != 0)
    tp_debug_set_persistent (TRUE);
}

/**
 * tp_debug_set_flags_from_env:
 * @var: The name of the environment variable to parse
 *
 * Equivalent to
 * <literal>tp_debug_set_flags_from_string (g_getenv (var))</literal>,
 * and has the same problem with persistence being included in "all".
 *
 * If telepathy-glib was compiled with --disable-debug (not recommended),
 * this function does nothing.
 *
 * @deprecated since 0.6.1. Use tp_debug_set_flags(g_getenv(...)) and
 * tp_debug_set_persistent() instead
 */
void
tp_debug_set_flags_from_env (const gchar *var)
{
  const gchar *val = g_getenv (var);

  tp_debug_set_flags (val);
  if (val != NULL && g_parse_debug_string (val, persist_keys, 1) != 0)
    tp_debug_set_persistent (TRUE);
}

/**
 * tp_debug_set_persistent:
 * @persistent: TRUE prevents the connection manager mainloop from exiting,
 *              FALSE enables exiting if there are no connections
 *              (the default behavior).
 *
 * Used to enable persistent operation of the connection manager process for
 * debugging purposes.
 *
 * If telepathy-glib was compiled with --disable-debug (not recommended),
 * this function does nothing.
 */
void
tp_debug_set_persistent (gboolean persistent)
{
  tp_debug_persistent = persistent;
}

/*
 * _tp_debug_set_flags:
 * @new_flags More flags to set
 *
 * Set extra flags. For internal use only
 */
void
_tp_debug_set_flags (TpDebugFlags new_flags)
{
  flags |= new_flags;
}

/*
 * _tp_debug_set_flags:
 * @flag: Flag to test
 *
 * Returns: %TRUE if the flag is set. For use via DEBUGGING() only.
 */
gboolean
_tp_debug_flag_is_set (TpDebugFlags flag)
{
  return (flag & flags) != 0;
}

/*
 * _tp_debug_set_flags:
 * @flag: Flag to test
 * @format: Format string for g_logv
 *
 * Emit a debug message with the given format and arguments, but only if
 * the given debug flag is set. For use via DEBUG() only.
 */
void _tp_debug (TpDebugFlags flag,
                const gchar *format,
                ...)
{
  if (flag & flags)
    {
      va_list args;
      va_start (args, format);
      g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
      va_end (args);
    }
}

/*
 * _tp_debug_is_persistent:
 *
 * Returns: %TRUE if persistent mainloop behavior has been enabled with
 * tp_debug_set_persistent().
 */
gboolean
_tp_debug_is_persistent (void)
{
  return tp_debug_persistent;
}

#else /* !ENABLE_DEBUG */

void
tp_debug_set_all_flags (void)
{
}

void
tp_debug_set_flags (const gchar *flags_string)
{
}

void
tp_debug_set_flags_from_string (const gchar *flags_string)
{
}

void
tp_debug_set_flags_from_env (const gchar *var)
{
}

void
tp_debug_set_persistent (gboolean persistent)
{
}

#endif /* !ENABLE_DEBUG */

/**
 * tp_debug_divert_messages:
 * @filename: A file to which to divert stdout and stderr, or %NULL to
 *  do nothing
 *
 * Open the given file for writing and duplicate its file descriptor to
 * be used for stdout and stderr. This has the effect of closing the previous
 * stdout and stderr, and sending all messages that would have gone there
 * to the given file instead.
 *
 * By default the file is truncated and hence overwritten each time the
 * process is executed.
 * If the filename is prefixed with '+' then the file is not truncated
 * and output is added at the end of the file.
 *
 * Passing %NULL to this function is guaranteed to have no effect. This is
 * so you can call it with the recommended usage
 * <literal>tp_debug_divert_messages (g_getenv ("MYAPP_LOGFILE"))</literal>
 * and it won't do anything if the environment variable is not set.
 *
 * This function still works if telepathy-glib was compiled without debug
 * support.
 *
 * Since: 0.7.1
 */
void
tp_debug_divert_messages (const gchar *filename)
{
  int fd;

  if (filename == NULL)
    return;

  if (g_str_has_prefix (filename, "+"))
    {
      /* open in append mode */
      fd = g_open (filename + 1, O_WRONLY | O_CREAT | O_APPEND, 0644);
    }
  else
    {
      /* open in trunc mode */
      fd = g_open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

  if (fd == -1)
    {
      g_warning ("Can't open logfile '%s': %s", filename,
          g_strerror (errno));
      return;
    }

  if (dup2 (fd, STDOUT_FILENO) == -1)
    {
      g_warning ("Error duplicating stdout file descriptor: %s",
          g_strerror (errno));
      return;
    }

  if (dup2 (fd, STDERR_FILENO) == -1)
    {
      g_warning ("Error duplicating stderr file descriptor: %s",
          g_strerror (errno));
    }

  /* avoid leaking the fd */
  if (close (fd) != 0)
    {
      g_warning ("Error closing temporary logfile fd: %s", g_strerror (errno));
    }
}

/**
 * tp_debug_timestamped_log_handler:
 * @log_domain: the message's log domain
 * @log_level: the log level of the message
 * @message: the message to process
 * @ignored: not used
 *
 * A #GLogFunc that prepends the local time (currently in
 * YYYY-MM-DD HH:MM:SS.SSSSSS format, with microsecond resolution) to the
 * message, then calls g_log_default_handler.
 *
 * Intended usage is:
 *
 * <informalexample><programlisting>if (g_getenv ("MYPROG_TIMING") != NULL)
 *   g_log_set_default_handler (tp_debug_timestamped_log_handler, NULL);
 * </programlisting></informalexample>
 *
 * If telepathy-glib was compiled with --disable-debug (not recommended),
 * this function is equivalent to g_log_default_handler().
 *
 * Since: 0.7.1
 */
void
tp_debug_timestamped_log_handler (const gchar *log_domain,
                                  GLogLevelFlags log_level,
                                  const gchar *message,
                                  gpointer ignored)
{
#ifdef ENABLE_DEBUG
  GTimeVal now;
  gchar now_str[32];
  gchar *tmp;
  struct tm tm;
  time_t sec;

  g_get_current_time (&now);
  sec = now.tv_sec;
  localtime_r (&sec, &tm);
  strftime (now_str, 32, "%Y-%m-%d %H:%M:%S", &tm);
  tmp = g_strdup_printf ("%s.%06ld: %s", now_str, now.tv_usec, message);
  message = tmp;
#endif

  g_log_default_handler (log_domain, log_level, message, NULL);

#ifdef ENABLE_DEBUG
  g_free (tmp);
#endif
}
