/* Test enum #defines are up to date
 *
 * Copyright (C) 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <telepathy-glib/enums.h>

#include <telepathy-glib/contact.h>
#include <telepathy-glib/proxy.h>

#include "tests/lib/debug.h"
#include "tests/lib/util.h"

static void
test_tp_contact_feature (void)
{
  GEnumClass *klass;

  g_type_init ();

  klass = g_type_class_ref (TP_TYPE_CONTACT_FEATURE);

  g_assert (klass != NULL);
  g_assert (G_IS_ENUM_CLASS (klass));

  g_assert_cmpint (klass->n_values, ==, TP_NUM_CONTACT_FEATURES);

  g_type_class_unref (klass);
}


static void
test_tp_dbus_error (void)
{
  GEnumClass *klass;

  g_type_init ();

  klass = g_type_class_ref (TP_TYPE_DBUS_ERROR);

  g_assert (klass != NULL);
  g_assert (G_IS_ENUM_CLASS (klass));

  g_assert_cmpint (klass->n_values, ==, TP_NUM_DBUS_ERRORS);
}


int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/enums/tp-contact-feature", test_tp_contact_feature);
  g_test_add_func ("/enums/tp-dbus-error", test_tp_dbus_error);

  return g_test_run ();
}
