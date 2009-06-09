/* A very basic feature test for TpChannelRequest
 *
 * Copyright (C) 2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2009 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/channel-request.h>
#include <telepathy-glib/defs.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/svc-channel-request.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "tests/lib/util.h"

/* This object implements no methods and no properties - TpChannelRequest
 * doesn't actually use them yet */

static GType test_simple_cr_get_type (void);

typedef struct {
    GObject parent;
} TestSimpleCR;

typedef struct {
    GObjectClass parent;
} TestSimpleCRClass;

G_DEFINE_TYPE_WITH_CODE (TestSimpleCR, test_simple_cr, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL_REQUEST, NULL));

static void
test_simple_cr_init (TestSimpleCR *self)
{
}

static void
test_simple_cr_class_init (TestSimpleCRClass *klass)
{
}

typedef struct {
    GMainLoop *mainloop;
    TpDBusDaemon *dbus;

    DBusGConnection *private_conn;
    TpDBusDaemon *private_dbus;
    GObject *cr_service;

    TpChannelRequest *cr;
    GError *error /* initialized where needed */;

    guint succeeded;
} Test;

static void
setup (Test *test,
       gconstpointer data)
{
  DBusConnection *libdbus;

  g_type_init ();
  tp_debug_set_flags ("all");

  test->mainloop = g_main_loop_new (NULL, FALSE);
  test->dbus = tp_dbus_daemon_dup (NULL);
  g_assert (test->dbus != NULL);

  libdbus = dbus_bus_get_private (DBUS_BUS_STARTER, NULL);
  g_assert (libdbus != NULL);
  dbus_connection_setup_with_g_main (libdbus, NULL);
  dbus_connection_set_exit_on_disconnect (libdbus, FALSE);
  test->private_conn = dbus_connection_get_g_connection (libdbus);
  /* transfer ref */
  dbus_g_connection_ref (test->private_conn);
  dbus_connection_unref (libdbus);
  g_assert (test->private_conn != NULL);
  test->private_dbus = tp_dbus_daemon_new (test->private_conn);
  g_assert (test->private_dbus != NULL);

  test->cr = NULL;

  test->cr_service = g_object_new (test_simple_cr_get_type (),
      NULL);
  dbus_g_connection_register_g_object (test->private_conn, "/whatever",
      test->cr_service);
}

static void
teardown (Test *test,
          gconstpointer data)
{
  if (test->cr != NULL)
    {
      g_object_unref (test->cr);
      test->cr = NULL;
    }

  if (test->private_dbus != NULL)
    {
      tp_dbus_daemon_release_name (test->private_dbus,
          TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

      g_object_unref (test->private_dbus);
      test->private_dbus = NULL;
    }

#if 0
  /* not leaking this object would crash dbus-glib (fd.o #5688) */
  g_object_unref (test->cr_service);
  test->cr_service = NULL;
#endif

  if (test->private_conn != NULL)
    {
      dbus_connection_close (dbus_g_connection_get_connection (
            test->private_conn));

      dbus_g_connection_unref (test->private_conn);
      test->private_conn = NULL;
    }

  g_object_unref (test->dbus);
  test->dbus = NULL;

  g_main_loop_unref (test->mainloop);
  test->mainloop = NULL;
}

static void
test_new (Test *test,
          gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  /* CD not running */
  test->cr = tp_channel_request_new (test->dbus,
      "/whatever", NULL, NULL);
  g_assert (test->cr == NULL);

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus,
      "not even syntactically valid", NULL, NULL);
  g_assert (test->cr == NULL);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
}

static void
test_crash (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  tp_dbus_daemon_release_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, NULL);

  test_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  dbus_connection_close (dbus_g_connection_get_connection (
        test->private_conn));
  dbus_g_connection_unref (test->private_conn);
  test->private_conn = NULL;

  test_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code ==
      TP_DBUS_ERROR_NAME_OWNER_LOST);
}

static void
succeeded_cb (Test *test)
{
  test->succeeded++;
}

static void
test_succeeded (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  g_signal_connect_swapped (test->cr, "succeeded", G_CALLBACK (succeeded_cb),
      test);

  tp_svc_channel_request_emit_succeeded (test->cr_service);

  test_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_DBUS_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code ==
      TP_DBUS_ERROR_OBJECT_REMOVED);
  g_assert_cmpuint (test->succeeded, ==, 1);

  g_signal_handlers_disconnect_by_func (test->cr, G_CALLBACK (succeeded_cb),
      test);
}

static void
test_failed (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gboolean ok;

  ok = tp_dbus_daemon_request_name (test->private_dbus,
      TP_CHANNEL_DISPATCHER_BUS_NAME, FALSE, NULL);
  g_assert (ok);

  test->cr = tp_channel_request_new (test->dbus, "/whatever", NULL, NULL);
  g_assert (test->cr != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr) == NULL);

  g_signal_connect_swapped (test->cr, "succeeded", G_CALLBACK (succeeded_cb),
      test);

  tp_svc_channel_request_emit_failed (test->cr_service,
      TP_ERROR_STR_NOT_YOURS, "lalala");

  test_proxy_run_until_dbus_queue_processed (test->cr);

  g_assert (tp_proxy_get_invalidated (test->cr) != NULL);
  g_assert (tp_proxy_get_invalidated (test->cr)->domain == TP_ERRORS);
  g_assert (tp_proxy_get_invalidated (test->cr)->code == TP_ERROR_NOT_YOURS);
  g_assert_cmpstr (tp_proxy_get_invalidated (test->cr)->message, ==,
      "lalala");
  g_assert_cmpuint (test->succeeded, ==, 0);

  g_signal_handlers_disconnect_by_func (test->cr, G_CALLBACK (succeeded_cb),
      test);
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add ("/cr/new", Test, NULL, setup, test_new, teardown);
  g_test_add ("/cr/crash", Test, NULL, setup, test_crash, teardown);
  g_test_add ("/cr/succeeded", Test, NULL, setup, test_succeeded, teardown);
  g_test_add ("/cr/failed", Test, NULL, setup, test_failed, teardown);

  return g_test_run ();
}
