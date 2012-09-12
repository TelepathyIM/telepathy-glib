#include "config.h"

#include <stdio.h>

#include <glib.h>

#include <telepathy-glib/telepathy-glib-dbus.h>

#include "tests/lib/myassert.h"
#include "telepathy-glib/variant-util-internal.h"

#define asv_assert(type, key, expected_value, expected_valid) \
    valid = (gboolean) 123; \
    g_assert (tp_asv_get_##type (hash, key, NULL) == expected_value); \
    g_assert (tp_asv_get_##type (hash, key, &valid) == expected_value); \
    g_assert (valid == expected_valid); \
\
    valid = (gboolean) 123; \
    g_assert (tp_vardict_get_##type (vardict, key, NULL) == expected_value); \
    g_assert (tp_vardict_get_##type (vardict, key, &valid) == expected_value); \
    g_assert (valid == expected_valid)

#define asv_assert_string(key, expected_value) \
    g_assert_cmpstr (tp_asv_get_string (hash, key), ==, expected_value); \
    g_assert_cmpstr (tp_vardict_get_string (vardict, key), ==, expected_value); \

#define asv_assert_object_path(key, expected_value) \
    g_assert_cmpstr (tp_asv_get_object_path (hash, key), ==, expected_value); \
    g_assert_cmpstr (tp_vardict_get_object_path (vardict, key), ==, expected_value); \

int main (int argc, char **argv)
{
  GHashTable *hash;
  GVariant *vardict;
  gboolean valid;
  static const char * const strv[] = { "Foo", "Bar", NULL };

  /* Setup */

  g_type_init ();

  hash = tp_asv_new (
      "d:123.2", G_TYPE_DOUBLE, 123.2,
      "s:test", G_TYPE_STRING, "test",
      NULL);

  MYASSERT (tp_asv_size (hash) == 2, "%u != 0", tp_asv_size (hash));

  g_hash_table_insert (hash, "d:0", tp_g_value_slice_new_double (0.0));

  MYASSERT (tp_asv_size (hash) == 3, "%u != 1", tp_asv_size (hash));

  g_hash_table_insert (hash, "d:-123", tp_g_value_slice_new_double (-123.0));

  MYASSERT (tp_asv_size (hash) == 4, "%u != 2", tp_asv_size (hash));

  g_hash_table_insert (hash, "b:TRUE", tp_g_value_slice_new_boolean (TRUE));
  g_hash_table_insert (hash, "b:FALSE", tp_g_value_slice_new_boolean (FALSE));

  g_hash_table_insert (hash, "s0", tp_g_value_slice_new_static_string (""));

  g_hash_table_insert (hash, "s",
      tp_g_value_slice_new_string ("hello, world!"));

  g_hash_table_insert (hash, "o",
      tp_g_value_slice_new_object_path ("/com/example/Object"));

  g_hash_table_insert (hash, "i32:-2**16",
      tp_g_value_slice_new_int (-0x10000));

  g_hash_table_insert (hash, "i32:0", tp_g_value_slice_new_int (0));
  g_hash_table_insert (hash, "u32:0", tp_g_value_slice_new_uint (0));
  g_hash_table_insert (hash, "i64:0", tp_g_value_slice_new_int64 (0));
  g_hash_table_insert (hash, "u64:0", tp_g_value_slice_new_uint64 (0));

  g_hash_table_insert (hash, "i32:2**16", tp_g_value_slice_new_int (0x10000));
  g_hash_table_insert (hash, "u32:2**16", tp_g_value_slice_new_uint (0x10000));

  g_hash_table_insert (hash, "i32:-2**31",
      tp_g_value_slice_new_int (0x10000 * -0x8000));

  g_hash_table_insert (hash, "i32:2**31-1",
      tp_g_value_slice_new_int (0x7FFFffff));
  g_hash_table_insert (hash, "u32:2**31-1",
      tp_g_value_slice_new_uint (0x7FFFffff));

  g_hash_table_insert (hash, "u32:2**31",
      tp_g_value_slice_new_uint (0x80000000U));
  g_hash_table_insert (hash, "u32:2**32-1",
      tp_g_value_slice_new_uint (0xFFFFffffU));
  g_hash_table_insert (hash, "u64:2**32-1",
      tp_g_value_slice_new_uint64 (0xFFFFffffU));

  g_hash_table_insert (hash, "u64:2**32",
      tp_g_value_slice_new_uint64 (G_GUINT64_CONSTANT (0x100000000)));

  g_hash_table_insert (hash, "i64:-2**63",
  tp_g_value_slice_new_int64 (G_GINT64_CONSTANT (-0x80000000) *
      G_GINT64_CONSTANT (0x100000000)));

  g_hash_table_insert (hash, "i64:2**63-1",
      tp_g_value_slice_new_int64 (G_GINT64_CONSTANT (0x7FFFffffFFFFffff)));

  g_hash_table_insert (hash, "u64:2**63-1",
      tp_g_value_slice_new_uint64 (G_GUINT64_CONSTANT (0x7FFFffffFFFFffff)));

  g_hash_table_insert (hash, "u64:2**64-1",
      tp_g_value_slice_new_uint64 (G_GUINT64_CONSTANT (0xFFFFffffFFFFffff)));

  g_hash_table_insert (hash, "as",
      tp_g_value_slice_new_boxed (G_TYPE_STRV, strv));

  g_hash_table_insert (hash, "as0",
      tp_g_value_slice_new_boxed (G_TYPE_STRV, strv + 2));

  tp_asv_dump (hash);

  vardict = _tp_asv_to_vardict (hash);

  /* Tests: tp_asv_get_boolean */

  asv_assert (boolean, "b:FALSE", FALSE, TRUE);
  asv_assert (boolean, "b:TRUE", TRUE, TRUE);
  asv_assert (boolean, "s", FALSE, FALSE);
  asv_assert (boolean, "not-there", FALSE, FALSE);
  asv_assert (boolean, "i32:2**16", FALSE, FALSE);
  asv_assert (boolean, "d:0", FALSE, FALSE);
  asv_assert (boolean, "d:-123", FALSE, FALSE);
  asv_assert (boolean, "d:123.2", FALSE, FALSE);

  /* Tests: tp_asv_get_double */

  asv_assert (double, "s", 0.0, FALSE);
  asv_assert (double, "b:TRUE", 0.0, FALSE);
  asv_assert (double, "not-there", 0.0, FALSE);
  asv_assert (double, "i32:0", 0.0, TRUE);
  asv_assert (double, "u32:0", 0.0, TRUE);
  asv_assert (double, "u32:2**16", (double) 0x10000, TRUE);
  asv_assert (double, "i32:-2**16", (double) -0x10000, TRUE);
  asv_assert (double, "u64:0", 0.0, TRUE);
  asv_assert (double, "d:0", 0.0, TRUE);
  asv_assert (double, "d:-123", -123.0, TRUE);
  asv_assert (double, "d:123.2", 123.2, TRUE);

  /* Tests: tp_asv_get_int32 */

  asv_assert (int32, "s", 0, FALSE);
  asv_assert (int32, "b:TRUE", 0, FALSE);
  asv_assert (int32, "d:0", 0, FALSE);
  asv_assert (int32, "not-there", 0, FALSE);
  asv_assert (int32, "i32:0", 0, TRUE);
  asv_assert (int32, "u32:0", 0, TRUE);
  asv_assert (int32, "i32:2**16", 0x10000, TRUE);
  asv_assert (int32, "u32:2**16", 0x10000, TRUE);
  asv_assert (int32, "i32:-2**16", -0x10000, TRUE);
  asv_assert (int32, "i32:-2**31", 0x10000 * -0x8000, TRUE);
  asv_assert (int32, "i32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (int32, "u32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (int32, "u32:2**31", 0, FALSE);
  asv_assert (int32, "u32:2**32-1", 0, FALSE);
  asv_assert (int32, "u64:2**32-1", 0, FALSE);
  asv_assert (int32, "u64:2**32", 0, FALSE);
  asv_assert (int32, "u64:2**64-1", 0, FALSE);
  asv_assert (int32, "i64:-2**63", 0, FALSE);
  asv_assert (int32, "i64:2**63-1", 0, FALSE);
  asv_assert (int32, "u64:2**63-1", 0, FALSE);

  /* Tests: tp_asv_get_uint32 */

  asv_assert (uint32, "s", 0, FALSE);
  asv_assert (uint32, "b:TRUE", 0, FALSE);
  asv_assert (uint32, "d:0", 0, FALSE);
  asv_assert (uint32, "not-there", 0, FALSE);
  asv_assert (uint32, "i32:0", 0, TRUE);
  asv_assert (uint32, "u32:0", 0, TRUE);
  asv_assert (uint32, "i32:2**16", 0x10000, TRUE);
  asv_assert (uint32, "u32:2**16", 0x10000, TRUE);
  asv_assert (uint32, "i32:-2**16", 0, FALSE);
  asv_assert (uint32, "i32:-2**31", 0, FALSE);
  asv_assert (uint32, "i32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (uint32, "u32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (uint32, "u32:2**31", 0x80000000U, TRUE);
  asv_assert (uint32, "u32:2**32-1", 0xFFFFFFFFU, TRUE);
  asv_assert (uint32, "u64:2**32-1", 0xFFFFFFFFU, TRUE);
  asv_assert (uint32, "u64:2**32", 0, FALSE);
  asv_assert (uint32, "u64:2**64-1", 0, FALSE);
  asv_assert (uint32, "i64:-2**63", 0, FALSE);
  asv_assert (uint32, "i64:2**63-1", 0, FALSE);
  asv_assert (uint32, "u64:2**63-1", 0, FALSE);

  /* Tests: tp_asv_get_int64 */

  asv_assert (int64, "s", 0, FALSE);
  asv_assert (int64, "b:TRUE", 0, FALSE);
  asv_assert (int64, "d:0", 0, FALSE);
  asv_assert (int64, "not-there", 0, FALSE);
  asv_assert (int64, "i32:0", 0, TRUE);
  asv_assert (int64, "u32:0", 0, TRUE);
  asv_assert (int64, "i32:2**16", 0x10000, TRUE);
  asv_assert (int64, "u32:2**16", 0x10000, TRUE);
  asv_assert (int64, "i32:-2**16", -0x10000, TRUE);
  asv_assert (int64, "i32:-2**31", 0x10000 * -0x8000, TRUE);
  asv_assert (int64, "i32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (int64, "u32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (int64, "u32:2**31", G_GINT64_CONSTANT (0x80000000), TRUE);
  asv_assert (int64, "u32:2**32-1", G_GINT64_CONSTANT (0xFFFFFFFF), TRUE);
  asv_assert (int64, "u64:2**32-1", G_GINT64_CONSTANT (0xFFFFFFFF), TRUE);
  asv_assert (int64, "u64:2**32", G_GINT64_CONSTANT (0x100000000), TRUE);
  asv_assert (int64, "u64:2**64-1", 0, FALSE);
  asv_assert (int64, "i64:-2**63", G_GINT64_CONSTANT (-0x80000000) * G_GINT64_CONSTANT (0x100000000), TRUE);
  asv_assert (int64, "i64:2**63-1", G_GINT64_CONSTANT (0x7FFFFFFFFFFFFFFF), TRUE);
  asv_assert (int64, "u64:2**63-1", G_GINT64_CONSTANT (0x7FFFFFFFFFFFFFFF), TRUE);

  /* Tests: tp_asv_get_uint64 */

  asv_assert (uint64, "s", 0, FALSE);
  asv_assert (uint64, "b:TRUE", 0, FALSE);
  asv_assert (uint64, "d:0", 0, FALSE);
  asv_assert (uint64, "not-there", 0, FALSE);
  asv_assert (uint64, "i32:0", 0, TRUE);
  asv_assert (uint64, "u32:0", 0, TRUE);
  asv_assert (uint64, "i32:2**16", 0x10000, TRUE);
  asv_assert (uint64, "u32:2**16", 0x10000, TRUE);
  asv_assert (uint64, "i32:-2**16", 0, FALSE);
  asv_assert (uint64, "i32:-2**31", 0, FALSE);
  asv_assert (uint64, "i32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (uint64, "u32:2**31-1", 0x7FFFFFFF, TRUE);
  asv_assert (uint64, "u32:2**31", 0x80000000U, TRUE);
  asv_assert (uint64, "u32:2**32-1", 0xFFFFFFFFU, TRUE);
  asv_assert (uint64, "u64:2**32-1", 0xFFFFFFFFU, TRUE);
  asv_assert (uint64, "u64:2**32", G_GUINT64_CONSTANT (0x100000000), TRUE);
  asv_assert (uint64, "u64:2**64-1", G_GUINT64_CONSTANT (0xFFFFffffFFFFffff), TRUE);
  asv_assert (uint64, "i64:-2**63", 0, FALSE);
  asv_assert (uint64, "i64:2**63-1", G_GUINT64_CONSTANT (0x7FFFffffFFFFffff), TRUE);
  asv_assert (uint64, "u64:2**63-1", G_GUINT64_CONSTANT (0x7FFFffffFFFFffff), TRUE);

  /* Tests: tp_asv_get_string */

  asv_assert_string ("s", "hello, world!");
  asv_assert_string ("s0", "");
  asv_assert_string ("b:TRUE", NULL);
  asv_assert_string ("b:FALSE", NULL);
  asv_assert_string ("not-there", NULL);
  asv_assert_string ("i32:0", NULL);
  asv_assert_string ("u32:0", NULL);
  asv_assert_string ("d:0", NULL);

  /* Tests: tp_asv_get_object_path */

  asv_assert_object_path ("o", "/com/example/Object");
  asv_assert_object_path ("s", NULL);
  asv_assert_object_path ("s0", NULL);
  asv_assert_object_path ("b:TRUE", NULL);
  asv_assert_object_path ("b:FALSE", NULL);
  asv_assert_object_path ("not-there", NULL);
  asv_assert_object_path ("i32:0", NULL);
  asv_assert_object_path ("u32:0", NULL);
  asv_assert_object_path ("d:0", NULL);

  /* Tests: tp_asv_get_strv */

  MYASSERT (tp_asv_get_strv (hash, "s") == NULL, "");
  MYASSERT (tp_asv_get_strv (hash, "u32:0") == NULL, "");
  MYASSERT (tp_asv_get_strv (hash, "as") != NULL, "");
  MYASSERT (!tp_strdiff (tp_asv_get_strv (hash, "as")[0], "Foo"), "");
  MYASSERT (!tp_strdiff (tp_asv_get_strv (hash, "as")[1], "Bar"), "");
  MYASSERT (tp_asv_get_strv (hash, "as")[2] == NULL, "");
  MYASSERT (tp_asv_get_strv (hash, "as0") != NULL, "");
  MYASSERT (tp_asv_get_strv (hash, "as0")[0] == NULL, "");

  /* Tests: tp_asv_lookup */

  MYASSERT (G_VALUE_HOLDS_STRING (tp_asv_lookup (hash, "s")), "");
  MYASSERT (G_VALUE_HOLDS_UINT (tp_asv_lookup (hash, "u32:0")), "");
  MYASSERT (G_VALUE_HOLDS_BOOLEAN (tp_asv_lookup (hash, "b:TRUE")), "");
  MYASSERT (G_VALUE_HOLDS_INT (tp_asv_lookup (hash, "i32:0")), "");
  MYASSERT (tp_asv_lookup (hash, "not-there") == NULL, "");

  /* Teardown */

  g_hash_table_unref (hash);
  g_variant_unref (vardict);

  return 0;
}
