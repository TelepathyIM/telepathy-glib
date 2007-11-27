/*
 * main.c - entry point for an example Telepathy connection manager
 *
 * Copyright (C) 2007 Collabora Ltd.
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/debug.h>
#include <telepathy-glib/run.h>
#include "manager.h"

static TpBaseConnectionManager *
construct_cm (void)
{
  return (TpBaseConnectionManager *) g_object_new (
      EXAMPLE_TYPE_CONNECTION_MANAGER,
      NULL);
}

int
main (int argc,
      char **argv)
{
#ifdef ENABLE_DEBUG
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  if (g_getenv ("EXAMPLE_PERSIST") != NULL)
    tp_debug_set_persistent (TRUE);
#endif

  return tp_run_connection_manager ("telepathy-example-cm-echo", VERSION,
      construct_cm, argc, argv);
}
