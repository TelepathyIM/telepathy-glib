#include "constants.h"

#include <glib.h>

#include <telepathy-logger/log-manager.h>
#include <telepathy-logger/log-store.h>
#include <telepathy-logger/log-store-xml.h>


static GMainLoop *loop = NULL;

static void
got_dates_cb (GObject *obj, GAsyncResult *result, gpointer user_data)
{
  GList *ret = tpl_log_manager_get_dates_async_finish (result, NULL);

  for (; ret != NULL; ret = g_list_next (ret))
    {
      gchar *date = ret->data;
      /* g_assert (!tp_strdiff (date, "12345678")); */
      g_free (date);
    }
    g_list_free (ret);
    g_main_loop_quit (loop);
}



int
main (int argc, char *argv[])
{
  GError *error = NULL;
  TpDBusDaemon *dbus;
  TpAccount *acc;
  TplLogManager *manager;
  TplLogStore *logstore;
  GList *ret, *loc;

  g_type_init ();

  dbus = tp_dbus_daemon_dup (&error);
  if (error != NULL)
    {
      g_debug ("%s", error->message);
    }
  acc = tp_account_new (dbus, ACCOUNT_PATH, NULL);
  if (acc == NULL)
    {
      g_debug ("error during TpAccount creation");
    }

  /* get the LogManager and register a non-default LogManager */
  manager = tpl_log_manager_dup_singleton ();
  logstore = g_object_new (TPL_TYPE_LOG_STORE_XML,
      "name", "Empathy", NULL);
  tpl_log_manager_register_log_store (manager, logstore);

  /* we are using the blocking API, the non-blocking one is a wrap around, so
   * it's assured to work as long as the blocking API and the wapper work */

  /* we do not want duplicates */
  ret = tpl_log_manager_get_dates (manager, acc, ID, FALSE);
  ret = g_list_sort (ret, (GCompareFunc) g_strcmp0);
  for (loc = ret; loc; loc = g_list_next (loc))
    if (loc->next)
      g_assert (g_strcmp0 (loc->data, loc->next->data) != 0);
  g_list_foreach (ret, (GFunc) g_free, NULL);
  g_list_free (ret);


  /* we do not want duplicates */
  ret = tpl_log_manager_get_chats (manager, acc);
  ret = g_list_sort (ret, (GCompareFunc) tpl_log_manager_search_hit_compare);
  for (loc = ret; loc; loc = g_list_next (loc))
    if (loc->next)
      g_assert (tpl_log_manager_search_hit_compare (loc->data,
            loc->next->data) != 0);
  g_list_foreach (ret, (GFunc) tpl_log_manager_search_hit_free, NULL);
  g_list_free (ret);


  tpl_log_manager_get_dates_async (manager, acc, ID, FALSE, got_dates_cb, NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
}
