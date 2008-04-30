#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/handle-repo-dynamic.h>
#include <telepathy-glib/enums.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/errors.h>

#include "tests/lib/myassert.h"

static int fail = 0;

static void
myassert_failed (void)
{
  fail = 1;
}

int
main (int argc,
      char **argv)
{
  TpHandleRepoIface *repo = NULL;
  TpHandleSet *set = NULL;
  TpIntSet *iset = NULL, *result = NULL;
  GError *error = NULL;

  TpHandle h1, h2, h3, h4;

  g_type_init ();

  repo = (TpHandleRepoIface *) g_object_new (TP_TYPE_DYNAMIC_HANDLE_REPO,
      "handle-type", TP_HANDLE_TYPE_CONTACT,
      NULL);
  MYASSERT (repo != NULL, "");

  set = tp_handle_set_new (repo);
  MYASSERT (set != NULL, "");

  h1 = tp_handle_ensure (repo, "h1@foo", NULL, NULL);
  h2 = tp_handle_ensure (repo, "h2@foo", NULL, NULL);
  h3 = tp_handle_ensure (repo, "h3@foo", NULL, NULL);
  h4 = tp_handle_ensure (repo, "h4@foo", NULL, NULL);
  MYASSERT (h1 != 0, "");
  MYASSERT (h2 != 0, "");
  MYASSERT (h3 != 0, "");
  MYASSERT (h4 != 0, "");

  MYASSERT (tp_handle_lookup (repo, "not-there", NULL, &error) == 0, "");
  /* Regression test for https://bugs.freedesktop.org/show_bug.cgi?id=15387 */
  MYASSERT (error != NULL, "");
  g_error_free (error);
  error = NULL;

  /* Add one handle, check that it's in, check the size */
  tp_handle_set_add (set, h1);
  MYASSERT (tp_handle_set_is_member (set, h1), "");
  MYASSERT (tp_handle_set_size (set) == 1,
      ": size really %i", tp_handle_set_size (set));

  /* Adding it again should be no-op */
  tp_handle_set_add (set, h1);
  MYASSERT (tp_handle_set_size (set) == 1,
      ": size really %i", tp_handle_set_size (set));

  /* Removing a non-member should fail */
  MYASSERT (tp_handle_set_remove (set, h2) == FALSE, "");

  /* Add some members via _update() */
  iset = tp_intset_new ();
  tp_intset_add (iset, h1);
  tp_intset_add (iset, h2);
  tp_intset_add (iset, h3);
  result = tp_handle_set_update (set, iset);
  tp_intset_destroy (iset);

  /* h2 and h3 should be added, and h1 not */
  MYASSERT (!tp_intset_is_member (result, h1), "");
  MYASSERT (tp_intset_is_member (result, h2), "");
  MYASSERT (tp_intset_is_member (result, h3), "");
  tp_intset_destroy (result);

  MYASSERT (tp_handle_set_is_member (set, h2), "");
  MYASSERT (tp_handle_set_is_member (set, h3), "");

  /* Remove some members via _update_difference() */
  iset = tp_intset_new ();
  tp_intset_add (iset, h1);
  tp_intset_add (iset, h4);
  result = tp_handle_set_difference_update (set, iset);
  tp_intset_destroy (iset);

  /* h1 should be removed, h4 not */
  MYASSERT (tp_intset_is_member (result, h1), "");
  MYASSERT (!tp_intset_is_member (result, h4), "");
  tp_intset_destroy (result);

  /* Removing a member should succeed */
  MYASSERT (tp_handle_set_remove (set, h2) == TRUE, "");

  /* Finally, only h3 should be in the set */
  MYASSERT (tp_handle_set_is_member (set, h3), "");
  MYASSERT (tp_handle_set_size (set) == 1,
      ": size really %i", tp_handle_set_size (set));

  MYASSERT (tp_handle_set_remove (set, h3) == TRUE, "");

  tp_handle_set_destroy (set);

  tp_handle_unref (repo, h1);
  tp_handle_unref (repo, h2);
  tp_handle_unref (repo, h3);
  tp_handle_unref (repo, h4);

  g_object_unref (G_OBJECT (repo));

  return fail;
}
