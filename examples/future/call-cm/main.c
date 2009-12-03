/*
 * main.c - entry point for an example Telepathy connection manager
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
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

#include <telepathy-glib/debug.h>
#include <telepathy-glib/run.h>

#include "extensions/extensions.h"

#include "cm.h"

static TpBaseConnectionManager *
construct_cm (void)
{
  return (TpBaseConnectionManager *) g_object_new (
      EXAMPLE_TYPE_CALL_CONNECTION_MANAGER,
      NULL);
}

int
main (int argc,
      char **argv)
{
#ifdef ENABLE_DEBUG
  tp_debug_divert_messages (g_getenv ("EXAMPLE_CM_LOGFILE"));
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (g_getenv ("EXAMPLE_TIMING") != NULL)
    g_log_set_default_handler (tp_debug_timestamped_log_handler, NULL);

  if (g_getenv ("EXAMPLE_PERSIST") != NULL)
    tp_debug_set_persistent (TRUE);
#endif

  /* strictly speaking, this is only necessary for client code, but it's
   * harmless here */
  g_type_init ();
  future_cli_init ();

  return tp_run_connection_manager ("telepathy-example-cm-call",
      VERSION, construct_cm, argc, argv);
}
