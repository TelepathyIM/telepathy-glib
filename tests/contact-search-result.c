/* Tests of TpContactSearchResult
 *
 * Copyright © 2010-2011 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include "config.h"

#include <glib.h>

#include "telepathy-glib/contact-search-internal.h"

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/gtypes.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/util.h>

#include "tests/lib/util.h"

static void
test_contact_search_result (void)
{
  TpContactSearchResult *result;
  TpContactInfoField *field;
  const gchar *identifier;
  GList *fields;
  gchar *field_value[] = { "Joe", NULL };

  result = _tp_contact_search_result_new ("id");
  g_assert (TP_IS_CONTACT_SEARCH_RESULT (result));

  identifier = tp_contact_search_result_get_identifier (result);
  g_assert_cmpstr (identifier, ==, "id");

  fields = tp_contact_search_result_get_fields (result);
  g_assert (fields == NULL);

  field = tp_contact_search_result_get_field (result, "fn");
  g_assert (field == NULL);

  field = tp_contact_info_field_new ("fn", NULL, field_value);
  g_assert (field != NULL);

  _tp_contact_search_result_insert_field (result, field);
  fields = tp_contact_search_result_get_fields (result);
  g_assert (fields != NULL);
  g_list_free (fields);

  field = tp_contact_search_result_get_field (result, "fn");
  g_assert (field != NULL);
  g_assert_cmpstr (field->field_value[0], ==, field_value[0]);
  g_assert_cmpstr (field->field_value[1], ==, field_value[1]);

  g_object_unref (result);
}

int
main (int argc,
    char **argv)
{
  g_type_init ();
  tp_debug_set_flags ("all");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  g_test_add_func ("/contact-search/contact-search-result",
      test_contact_search_result);

  return g_test_run ();
}
