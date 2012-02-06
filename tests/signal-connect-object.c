/* Regression test for fd.o#27242
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/debug.h>
#include <telepathy-glib/util.h>

#include "tests/lib/stub-object.h"
#include "tests/lib/util.h"

typedef struct
{
  guint caught;
  GObject *emitter;
  GObject *observer;
} Test;

#define DATA_KEY "signal-connect-object Test struct"

static void
increment_caught (GObject *emitter,
    GParamSpec *param_spec,
    gpointer user_data)
{
  GObject *observer = user_data;
  Test *test = g_object_get_data (observer, DATA_KEY);

  g_assert (test->emitter != NULL);
  g_assert (G_IS_OBJECT (test->emitter));
  g_assert (emitter != NULL);
  g_assert (emitter == test->emitter);

  g_assert (test->observer != NULL);
  g_assert (G_IS_OBJECT (test->observer));
  g_assert (observer != NULL);
  g_assert (observer == test->observer);

  g_assert (param_spec != NULL);
  g_assert_cmpstr (g_param_spec_get_name (param_spec), ==, "name");

  test->caught++;
}

static void
increment_caught_swapped (gpointer user_data,
    GParamSpec *param_spec,
    GObject *emitter)
{
  increment_caught (emitter, param_spec, user_data);
}

static void
setup (Test *test,
    gconstpointer data)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  test->caught = 0;
  test->observer = tp_tests_object_new_static_class (
      tp_tests_stub_object_get_type (), NULL);
  g_object_set_data (test->observer, DATA_KEY, test);
  test->emitter = tp_tests_object_new_static_class (
      tp_tests_stub_object_get_type (), NULL);
}

static void
teardown (Test *test,
    gconstpointer data)
{
  tp_clear_object (&test->emitter);
  tp_clear_object (&test->observer);
}

static void
test_no_unref (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  tp_g_signal_connect_object (test->emitter, "notify::name",
      G_CALLBACK (increment_caught), test->observer, 0);
  g_object_notify (test->emitter, "name");
  g_assert_cmpuint (test->caught, ==, 1);
}

static void
test_swapped (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  tp_g_signal_connect_object (test->emitter, "notify::name",
      G_CALLBACK (increment_caught_swapped), test->observer,
      G_CONNECT_SWAPPED);
  g_object_notify (test->emitter, "name");
  g_assert_cmpuint (test->caught, ==, 1);
}

static void
test_dead_observer (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  tp_g_signal_connect_object (test->emitter, "notify::name",
      G_CALLBACK (increment_caught), test->observer, 0);
  g_object_notify (test->emitter, "name");
  g_object_notify (test->emitter, "name");
  tp_clear_object (&test->observer);
  g_object_notify (test->emitter, "name");
  g_assert_cmpuint (test->caught, ==, 2);
}

static void
test_dead_emitter (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  tp_g_signal_connect_object (test->emitter, "notify::name",
      G_CALLBACK (increment_caught), test->observer, 0);
  g_object_notify (test->emitter, "name");
  g_object_notify (test->emitter, "name");
  tp_clear_object (&test->emitter);
  g_assert_cmpuint (test->caught, ==, 2);
}

static void
test_disconnected (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gulong id;

  id = tp_g_signal_connect_object (test->emitter, "notify::name",
      G_CALLBACK (increment_caught), test->observer, 0);
  g_object_notify (test->emitter, "name");
  g_object_notify (test->emitter, "name");
  g_signal_handler_disconnect (test->emitter, id);
  g_object_notify (test->emitter, "name");
  g_assert_cmpuint (test->caught, ==, 2);
}

static void
test_dead_observer_and_disconnected (Test *test,
    gconstpointer data G_GNUC_UNUSED)
{
  gulong id;

  id = tp_g_signal_connect_object (test->emitter, "notify::name",
      G_CALLBACK (increment_caught), test->observer, 0);
  g_object_notify (test->emitter, "name");
  g_object_notify (test->emitter, "name");
  g_signal_handler_disconnect (test->emitter, id);
  tp_clear_object (&test->observer);
  g_object_notify (test->emitter, "name");
  g_assert_cmpuint (test->caught, ==, 2);
}

int
main (int argc,
    char **argv)
{
#define TEST_PREFIX "/signal-connect-object/"

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add (TEST_PREFIX "no_unref", Test, NULL, setup, test_no_unref,
      teardown);
  g_test_add (TEST_PREFIX "swapped", Test, NULL, setup, test_swapped,
      teardown);
  g_test_add (TEST_PREFIX "dead_observer", Test, NULL, setup,
      test_dead_observer, teardown);
  g_test_add (TEST_PREFIX "dead_emitter", Test, NULL, setup,
      test_dead_emitter, teardown);
  g_test_add (TEST_PREFIX "disconnected", Test, NULL, setup,
      test_disconnected, teardown);
  g_test_add (TEST_PREFIX "dead_observer_and_disconnected", Test, NULL, setup,
      test_dead_observer_and_disconnected, teardown);

  return g_test_run ();
}
