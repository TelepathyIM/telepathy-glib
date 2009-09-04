#include <glib.h>

#include <telepathy-glib/util.h>

#include "telepathy-glib/debug-internal.h"

/* Only run test if ENABLE_DEBUG is defined, otherwise we won't have
 * _tp_debug and the TpDebugFlags enum. */
#ifdef ENABLE_DEBUG

typedef struct
{
  guint flag;
  const gchar *domain;
} TestItem;

static TestItem items[] = {
  { TP_DEBUG_GROUPS, "groups" },
  { TP_DEBUG_GROUPS | TP_DEBUG_PROPERTIES, "groups" },
  { TP_DEBUG_GROUPS | TP_DEBUG_DISPATCHER, "groups" },
  { TP_DEBUG_PROXY | TP_DEBUG_CHANNEL, "channel" },
  { 1 << 31, "misc" },
  { TP_DEBUG_ACCOUNTS, "accounts" },
  { TP_DEBUG_PROXY | TP_DEBUG_HANDLES | TP_DEBUG_PRESENCE, "presence" },
  { 0, NULL },
};
static guint item = 0;

static void
handler (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  TestItem i = items[item];
  gchar **parts;

  parts = g_strsplit (log_domain, "/", -1);

  g_assert_cmpuint (g_strv_length (parts), ==, 2);
  g_assert (!tp_strdiff (parts[0], "tp-glib"));
  g_assert (!tp_strdiff (parts[1], i.domain));
  g_assert (!tp_strdiff (message, "foo"));

  g_strfreev (parts);
}
#endif

int main (int argc, char **argv)
{
#ifdef ENABLE_DEBUG
  TestItem i;

  g_type_init ();

  tp_debug_set_flags ("all");

  g_log_set_default_handler (handler, NULL);

  for (; items[item].domain != NULL; item++)
    {
      i = items[item];
      _tp_debug (i.flag, "foo");
    }

#else
  g_print ("Not running test-debug-domain test as ENABLE_DEBUG is undefined\n");
#endif
  return 0;
}
