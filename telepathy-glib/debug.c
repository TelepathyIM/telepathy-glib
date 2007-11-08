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
 * <listitem><literal>connection</literal> - output debug messages regarding
 * #TpBaseConnection</listitem>
 * <listitem><literal>im</literal> - output debug messages regarding
 * (text) instant messaging</listitem>
 * <listitem><literal>properties</literal> - output debug messages regarding
 * #TpPropertiesMixin</listitem>
 * <listitem><literal>params</literal> - output debug messages regarding
 * connection manager parameters</listitem>
 * <listitem><literal>all</literal> - all of the above</listitem>
 * </itemizedlist>
 */
#include "config.h"

#include <glib.h>

#ifdef ENABLE_DEBUG

#include <telepathy-glib/debug.h>

#include "internal-debug.h"

#include <stdarg.h>

static TpDebugFlags flags = 0;

static gboolean tp_debug_persistent = FALSE;

/**
 * tp_debug_set_all_flags:
 *
 * Activate all possible debug modes. This also activates persistent mode,
 * which should have been orthogonal.
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
  { 0, },
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
