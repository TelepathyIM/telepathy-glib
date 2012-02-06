#include "config.h"

#include <glib.h>
#include <telepathy-glib/intset.h>
#include <telepathy-glib/util.h>

static void
iterate_in_order (TpIntset *set)
{
  TpIntsetIter iter;
  guint n = 0;
  gint64 prev = (guint) -1;

  tp_intset_iter_init (&iter, set);

  while (tp_intset_iter_next (&iter))
    {
      g_assert (tp_intset_is_member (set, iter.element));

      if (prev != (guint) -1)
        g_assert_cmpuint (iter.element, >, prev);

      prev = iter.element;
      n++;
    }

  g_assert_cmpuint (n, ==, tp_intset_size (set));
}

static void
iterate_fast (TpIntset *set)
{
  TpIntsetFastIter iter;
  guint n = 0;
  guint i;

  tp_intset_fast_iter_init (&iter, set);

  while (tp_intset_fast_iter_next (&iter, &i))
    {
      g_assert (tp_intset_is_member (set, i));
      n++;
    }

  g_assert_cmpuint (n, ==, tp_intset_size (set));
}

static void
test_iteration (TpIntset *set)
{
  iterate_fast (set);
  iterate_in_order (set);
}

int main (int argc, char **argv)
{
  TpIntset *set1 = tp_intset_new ();
  TpIntset *a, *b, *copy;
  TpIntset *ab_union, *ab_expected_union;
  TpIntset *ab_inter, *ab_expected_inter;
  TpIntset *a_diff_b, *a_expected_diff_b;
  TpIntset *b_diff_a, *b_expected_diff_a;
  TpIntset *ab_symmdiff, *ab_expected_symmdiff;
  GValue *value;

  g_type_init ();

  g_assert (tp_intset_is_empty (set1));
  g_assert_cmpuint (tp_intset_size (set1), ==, 0);

  tp_intset_add (set1, 0);

  tp_intset_add (set1, 2);
  tp_intset_add (set1, 3);
  tp_intset_add (set1, 5);
  tp_intset_add (set1, 8);

  tp_intset_add (set1, 1024);
  tp_intset_add (set1, 32);

  g_assert (!tp_intset_is_empty (set1));
  g_assert_cmpuint (tp_intset_size (set1), ==, 7);

  g_assert (tp_intset_is_member (set1, 2));
  g_assert (tp_intset_is_member (set1, 5));
  g_assert (tp_intset_is_member (set1, 1024));
  g_assert (!tp_intset_is_member (set1, 1023));
  g_assert (!tp_intset_is_member (set1, 1025));
  g_assert (tp_intset_is_member (set1, 0));
  g_assert (tp_intset_is_member (set1, 32));
  g_assert (!tp_intset_is_member (set1, 31));
  g_assert (!tp_intset_is_member (set1, 33));

  tp_intset_remove (set1, 8);
  tp_intset_remove (set1, 1024);
  g_assert_cmpuint (tp_intset_size (set1), ==, 5);

  test_iteration (set1);

  tp_intset_destroy (set1);

#define NUM_A 11
#define NUM_B 823
#define NUM_C 367
#define NUM_D 4177
#define NUM_E 109
#define NUM_F 1861

  a = tp_intset_new ();
  tp_intset_add (a, NUM_A);
  tp_intset_add (a, NUM_B);
  tp_intset_add (a, NUM_C);
  tp_intset_add (a, NUM_D);
  test_iteration (a);

  g_assert (tp_intset_is_equal (a, a));

  b = tp_intset_new ();
  tp_intset_add (b, NUM_C);
  tp_intset_add (b, NUM_D);
  tp_intset_add (b, NUM_E);
  tp_intset_add (b, NUM_F);

  test_iteration (b);
  g_assert (tp_intset_is_equal (b, b));
  g_assert (!tp_intset_is_equal (a, b));

  ab_expected_union = tp_intset_new ();
  tp_intset_add (ab_expected_union, NUM_A);
  tp_intset_add (ab_expected_union, NUM_B);
  tp_intset_add (ab_expected_union, NUM_C);
  tp_intset_add (ab_expected_union, NUM_D);
  tp_intset_add (ab_expected_union, NUM_E);
  tp_intset_add (ab_expected_union, NUM_F);

  ab_union = tp_intset_union (a, b);
  g_assert (tp_intset_is_equal (ab_union, ab_expected_union));
  test_iteration (ab_union);
  tp_intset_destroy (ab_union);
  tp_intset_destroy (ab_expected_union);
  ab_union = NULL;
  ab_expected_union = NULL;

  ab_expected_inter = tp_intset_new ();
  tp_intset_add (ab_expected_inter, NUM_C);
  tp_intset_add (ab_expected_inter, NUM_D);

  ab_inter = tp_intset_intersection (a, b);
  test_iteration (ab_inter);
  g_assert (tp_intset_is_equal (ab_inter, ab_expected_inter));
  tp_intset_destroy (ab_inter);
  tp_intset_destroy (ab_expected_inter);
  ab_inter = NULL;
  ab_expected_inter = NULL;

  a_expected_diff_b = tp_intset_new ();
  tp_intset_add (a_expected_diff_b, NUM_A);
  tp_intset_add (a_expected_diff_b, NUM_B);

  a_diff_b = tp_intset_difference (a, b);
  test_iteration (a_diff_b);
  g_assert (tp_intset_is_equal (a_diff_b, a_expected_diff_b));
  tp_intset_destroy (a_diff_b);
  tp_intset_destroy (a_expected_diff_b);
  a_diff_b = NULL;
  a_expected_diff_b = NULL;

  b_expected_diff_a = tp_intset_new ();
  tp_intset_add (b_expected_diff_a, NUM_E);
  tp_intset_add (b_expected_diff_a, NUM_F);

  b_diff_a = tp_intset_difference (b, a);
  test_iteration (b_diff_a);
  g_assert (tp_intset_is_equal (b_diff_a, b_expected_diff_a));
  tp_intset_destroy (b_diff_a);
  tp_intset_destroy (b_expected_diff_a);
  b_diff_a = NULL;
  b_expected_diff_a = NULL;

  ab_expected_symmdiff = tp_intset_new ();
  tp_intset_add (ab_expected_symmdiff, NUM_A);
  tp_intset_add (ab_expected_symmdiff, NUM_B);
  tp_intset_add (ab_expected_symmdiff, NUM_E);
  tp_intset_add (ab_expected_symmdiff, NUM_F);

  ab_symmdiff = tp_intset_symmetric_difference (a, b);
  test_iteration (ab_symmdiff);
  g_assert (tp_intset_is_equal (ab_symmdiff, ab_expected_symmdiff));
  tp_intset_destroy (ab_symmdiff);
  tp_intset_destroy (ab_expected_symmdiff);
  ab_symmdiff = NULL;
  ab_expected_symmdiff = NULL;

  {
    GArray *arr;
    TpIntset *tmp;

    arr = tp_intset_to_array (a);
    tmp = tp_intset_from_array (arr);
    g_assert (tp_intset_is_equal (a, tmp));
    g_array_unref (arr);
    tp_intset_destroy (tmp);
    arr = NULL;
    tmp = NULL;

    arr = tp_intset_to_array (b);
    tmp = tp_intset_from_array (arr);
    g_assert (tp_intset_is_equal (b, tmp));
    g_array_unref (arr);
    tp_intset_destroy (tmp);
    arr = NULL;
    tmp = NULL;
  }

  value = tp_g_value_slice_new_take_boxed (TP_TYPE_INTSET, a);
  copy = g_value_dup_boxed (value);

  g_assert (copy != a);
  g_assert (tp_intset_is_equal (copy, a));
  test_iteration (copy);
  g_boxed_free (TP_TYPE_INTSET, copy);

  /* a is owned by value now, so don't free it explicitly */
  tp_g_value_slice_free (value);
  tp_intset_destroy (b);
  a = NULL;
  value = NULL;
  b = NULL;

  return 0;
}
