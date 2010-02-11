/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include "debug.h"

#include <telepathy-glib/debug.h>
#include <telepathy-glib/debug-sender.h>

#ifdef ENABLE_DEBUG

static TplDebugFlags flags = 0;

static GDebugKey keys[] = {
  { "observer",     TPL_DEBUG_OBSERVER },
  { "channel",      TPL_DEBUG_CHANNEL },
  { "log-manager",  TPL_DEBUG_LOG_MANAGER },
  { "log-store",    TPL_DEBUG_LOG_STORE },
  { "conf",         TPL_DEBUG_CONF },
  { "contact",      TPL_DEBUG_CONTACT },
  { "main",         TPL_DEBUG_MAIN },
  { "dbus-service", TPL_DEBUG_DBUS_SERVICE },
  { 0, },
};

void tpl_debug_set_flags_from_env ()
{
  guint nkeys;
  const gchar *flags_string;

  for (nkeys = 0; keys[nkeys].value; nkeys++);

  flags_string = g_getenv ("TPL_DEBUG");

  tp_debug_set_flags (flags_string);

  if (flags_string != NULL)
    tpl_debug_set_flags (g_parse_debug_string (flags_string, keys,
          nkeys));
}

void tpl_debug_set_flags (TplDebugFlags new_flags)
{
  flags |= new_flags;
}

gboolean tpl_debug_flag_is_set (TplDebugFlags flag)
{
  return flag & flags;
}

GHashTable *flag_to_domains = NULL;

static const gchar *
debug_flag_to_domain (TplDebugFlags flag)
{
  if (G_UNLIKELY (flag_to_domains == NULL))
    {
      guint i;

      flag_to_domains = g_hash_table_new_full (g_direct_hash, g_direct_equal,
          NULL, g_free);

      for (i = 0; keys[i].value; i++)
        {
          GDebugKey key = (GDebugKey) keys[i];
          gchar *val;

          val = g_strdup_printf ("%s/%s", G_LOG_DOMAIN, key.key);
          g_hash_table_insert (flag_to_domains,
              GUINT_TO_POINTER (key.value), val);
        }
    }

  return g_hash_table_lookup (flag_to_domains, GUINT_TO_POINTER (flag));
}


void
tpl_debug_free (void)
{
  if (flag_to_domains == NULL)
    return;

  g_hash_table_destroy (flag_to_domains);
  flag_to_domains = NULL;
}

static void
log_to_debug_sender (TplDebugFlags flag,
    const gchar *message)
{
  TpDebugSender *dbg;
  GTimeVal now;

  dbg = tp_debug_sender_dup ();

  g_get_current_time (&now);

  tp_debug_sender_add_message (dbg, &now, debug_flag_to_domain (flag),
      G_LOG_LEVEL_DEBUG, message);

  g_object_unref (dbg);
}

void tpl_debug (TplDebugFlags flag,
    const gchar *format,
    ...)
{
  gchar *message;
  va_list args;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  log_to_debug_sender (flag, message);

  if (flag & flags)
    g_log (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, "%s", message);

  g_free (message);
}

#endif /* ENABLE_DEBUG */
