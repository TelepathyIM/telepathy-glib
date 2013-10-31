/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#include "config.h"
#include "telepathy-logger/debug-internal.h"

#include <telepathy-glib/telepathy-glib.h>

#ifdef ENABLE_DEBUG

static TplDebugFlags flags = 0;

static GDebugKey keys[] = {
  { "action-chain", TPL_DEBUG_ACTION_CHAIN },
  { "channel",      TPL_DEBUG_CHANNEL },
  { "conf",         TPL_DEBUG_CONF },
  { "entity",       TPL_DEBUG_ENTITY },
  { "dbus-service", TPL_DEBUG_DBUS_SERVICE },
  { "log-event",    TPL_DEBUG_LOG_EVENT },
  { "log-manager",  TPL_DEBUG_LOG_MANAGER },
  { "log-store",    TPL_DEBUG_LOG_STORE },
  { "main",         TPL_DEBUG_MAIN },
  { "observer",     TPL_DEBUG_OBSERVER },
  { "testsuite",    TPL_DEBUG_TESTSUITE },
  { 0, },
};

void
_tpl_debug_set_flags_from_env (void)
{
  guint nkeys;
  const gchar *flags_string;

  for (nkeys = 0; keys[nkeys].value; nkeys++);

  flags_string = g_getenv ("TPL_DEBUG");

  if (flags_string != NULL)
    _tpl_debug_set_flags (g_parse_debug_string (flags_string, keys, nkeys));

  tp_debug_set_flags (g_getenv ("TP_DEBUG"));
}


void
_tpl_debug_set_flags (TplDebugFlags new_flags)
{
  flags |= new_flags;
}


gboolean
_tpl_debug_flag_is_set (TplDebugFlags flag)
{
  return flag & flags;
}

GHashTable *flag_to_domains = NULL;


void
_tpl_debug_free (void)
{
  if (flag_to_domains == NULL)
    return;

  g_hash_table_unref (flag_to_domains);
  flag_to_domains = NULL;
}


void _tpl_debug (TplDebugFlags flag,
    const gchar *format,
    ...)
{
  gchar *message;
  va_list args;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  if (flag & flags)
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", message);

  g_free (message);
}

#endif /* ENABLE_DEBUG */

/* The following function has to be always define or CRITICAL messages won't
 * be shown */

void _tpl_critical (TplDebugFlags flag,
    const gchar *format,
    ...)
{
  gchar *message;
  va_list args;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  if (flag & flags)
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "%s", message);

  g_free (message);
}
