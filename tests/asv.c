#include <stdio.h>

#include <glib.h>

#include <telepathy-glib/dbus.h>
#include <telepathy-glib/util.h>

#include "tests/lib/myassert.h"

static int fail = 0;

static void
myassert_failed (void)
{
  fail = 1;
}

int main (int argc, char **argv)
{
  GHashTable *hash;
  GValue *value;
  gboolean valid;
  static const char * const strv[] = { "Foo", "Bar", NULL };

  /* Setup */

  g_type_init ();

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

  value = tp_g_value_slice_new (G_TYPE_DOUBLE);
  g_value_set_double (value, 0.0);
  g_hash_table_insert (hash, "d:0", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_DOUBLE);
  g_value_set_double (value, -123.0);
  g_hash_table_insert (hash, "d:-123", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_BOOLEAN);
  g_value_set_boolean (value, TRUE);
  g_hash_table_insert (hash, "b:TRUE", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_BOOLEAN);
  g_value_set_boolean (value, FALSE);
  g_hash_table_insert (hash, "b:FALSE", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, "");
  g_hash_table_insert (hash, "s0", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_STRING);
  g_value_set_string (value, "hello, world!");
  g_hash_table_insert (hash, "s", value);
  value = NULL;

  value = tp_g_value_slice_new (DBUS_TYPE_G_OBJECT_PATH);
  g_value_set_boxed (value, "/com/example/Object");
  g_hash_table_insert (hash, "o", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT);
  g_value_set_int (value, -0x10000);
  g_hash_table_insert (hash, "i32:-2**16", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT);
  g_value_set_int (value, 0);
  g_hash_table_insert (hash, "i32:0", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0);
  g_hash_table_insert (hash, "u32:0", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT64);
  g_value_set_int64 (value, 0);
  g_hash_table_insert (hash, "i64:0", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, 0);
  g_hash_table_insert (hash, "u64:0", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT);
  g_value_set_int (value, 0x10000);
  g_hash_table_insert (hash, "i32:2**16", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0x10000);
  g_hash_table_insert (hash, "u32:2**16", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT);
  g_value_set_int (value, 0x10000 * -0x8000);
  g_hash_table_insert (hash, "i32:-2**31", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT);
  g_value_set_int (value, 0x7FFFffff);
  g_hash_table_insert (hash, "i32:2**31-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0x7FFFffff);
  g_hash_table_insert (hash, "u32:2**31-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0x80000000U);
  g_hash_table_insert (hash, "u32:2**31", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0xFFFFffffU);
  g_hash_table_insert (hash, "u32:2**32-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, 0xFFFFffffU);
  g_hash_table_insert (hash, "u64:2**32-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, G_GUINT64_CONSTANT (0x100000000));
  g_hash_table_insert (hash, "u64:2**32", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT64);
  g_value_set_int64 (value, G_GINT64_CONSTANT (-0x80000000) *
      G_GINT64_CONSTANT (0x100000000));
  g_hash_table_insert (hash, "i64:-2**63", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_INT64);
  g_value_set_int64 (value, G_GINT64_CONSTANT (0x7FFFffffFFFFffff));
  g_hash_table_insert (hash, "i64:2**63-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, G_GUINT64_CONSTANT (0x7FFFffffFFFFffff));
  g_hash_table_insert (hash, "u64:2**63-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, G_GUINT64_CONSTANT (0xFFFFffffFFFFffff));
  g_hash_table_insert (hash, "u64:2**64-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_STRV);
  g_value_set_boxed (value, strv);
  g_hash_table_insert (hash, "as", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_STRV);
  g_value_set_boxed (value, strv + 2);
  g_hash_table_insert (hash, "as0", value);
  value = NULL;

  /* Tests: tp_asv_get_boolean */

  valid = (gboolean) 123;
  MYASSERT (!tp_asv_get_boolean (hash, "b:FALSE", NULL), "");
  MYASSERT (!tp_asv_get_boolean (hash, "b:FALSE", &valid), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_boolean (hash, "b:TRUE", NULL), "");
  MYASSERT (tp_asv_get_boolean (hash, "b:TRUE", &valid), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (!tp_asv_get_boolean (hash, "s", NULL), "");
  MYASSERT (!tp_asv_get_boolean (hash, "s", &valid), "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (!tp_asv_get_boolean (hash, "not-there", NULL), "");
  MYASSERT (!tp_asv_get_boolean (hash, "not-there", &valid), "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (!tp_asv_get_boolean (hash, "i32:2**16", NULL), "");
  MYASSERT (!tp_asv_get_boolean (hash, "i32:2**16", &valid), "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (!tp_asv_get_boolean (hash, "d:0", NULL), "");
  MYASSERT (!tp_asv_get_boolean (hash, "d:0", &valid), "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (!tp_asv_get_boolean (hash, "d:-123", NULL), "");
  MYASSERT (!tp_asv_get_boolean (hash, "d:-123", &valid), "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  /* Tests: tp_asv_get_double */

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "s", NULL) == 0, "");
  MYASSERT (tp_asv_get_double (hash, "s", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "b:TRUE", NULL) == 0, "");
  MYASSERT (tp_asv_get_double (hash, "b:TRUE", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "not-there", NULL) == 0.0, "");
  MYASSERT (tp_asv_get_double (hash, "not-there", &valid) == 0.0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "i32:0", NULL) == 0.0, "");
  MYASSERT (tp_asv_get_double (hash, "i32:0", &valid) == 0.0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "u32:0", NULL) == 0.0, "");
  MYASSERT (tp_asv_get_double (hash, "u32:0", &valid) == 0.0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "u32:2**16", NULL)
      == (double) 0x10000, "");
  MYASSERT (tp_asv_get_double (hash, "u32:2**16", &valid)
      == (double) 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "i32:-2**16", NULL) ==
      (double) -0x10000, "");
  MYASSERT (tp_asv_get_double (hash, "i32:-2**16", &valid) ==
      (double) -0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "u64:0", NULL) == 0.0, "");
  MYASSERT (tp_asv_get_double (hash, "u64:0", &valid) == 0.0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "d:0", NULL) == 0.0, "");
  MYASSERT (tp_asv_get_double (hash, "d:0", &valid) == 0.0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_double (hash, "d:-123", NULL) == -123.0, "");
  MYASSERT (tp_asv_get_double (hash, "d:-123", &valid) == -123.0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  /* Tests: tp_asv_get_int32 */

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "s", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "s", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "b:TRUE", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "b:TRUE", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "d:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "d:0", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "not-there", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "not-there", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "i32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_int32 (hash, "i32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i32:-2**16", NULL) == -0x10000, "");
  MYASSERT (tp_asv_get_int32 (hash, "i32:-2**16", &valid) == -0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i32:-2**31", NULL) == 0x10000 * -0x8000,
      "");
  MYASSERT (tp_asv_get_int32 (hash, "i32:-2**31", &valid) == 0x10000 * -0x8000,
      "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_int32 (hash, "i32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**31", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**31", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**32-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u32:2**32-1", &valid) == 0,
      "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**32-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**32-1", &valid) == 0,
      "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**32", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**32", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**64-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**64-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i64:-2**63", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "i64:-2**63", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "i64:2**63-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "i64:2**63-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**63-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_int32 (hash, "u64:2**63-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  /* Tests: tp_asv_get_uint32 */

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "s", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "s", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "b:TRUE", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "b:TRUE", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "d:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "d:0", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "not-there", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "not-there", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i32:-2**16", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i32:-2**16", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i32:-2**31", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i32:-2**31", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**31", NULL) == 0x80000000U, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**31", &valid) == 0x80000000U, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**32-1", NULL) == 0xFFFFFFFFU, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u32:2**32-1", &valid) == 0xFFFFFFFFU,
      "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**32-1", NULL) == 0xFFFFFFFFU, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**32-1", &valid) == 0xFFFFFFFFU,
      "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**32", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**32", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**64-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**64-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i64:-2**63", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i64:-2**63", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "i64:2**63-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "i64:2**63-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**63-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint32 (hash, "u64:2**63-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  /* Tests: tp_asv_get_int64 */

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "s", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "s", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "b:TRUE", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "b:TRUE", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "d:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "d:0", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "not-there", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "not-there", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "i32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "u32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_int64 (hash, "i32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i32:-2**16", NULL) == -0x10000, "");
  MYASSERT (tp_asv_get_int64 (hash, "i32:-2**16", &valid) == -0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i32:-2**31", NULL) == 0x10000 * -0x8000,
      "");
  MYASSERT (tp_asv_get_int64 (hash, "i32:-2**31", &valid) == 0x10000 * -0x8000,
      "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_int64 (hash, "i32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**31", NULL) ==
      G_GINT64_CONSTANT (0x80000000), "");
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**31", &valid) ==
      G_GINT64_CONSTANT (0x80000000), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**32-1", NULL) ==
      G_GINT64_CONSTANT (0xFFFFFFFF), "");
  MYASSERT (tp_asv_get_int64 (hash, "u32:2**32-1", &valid) ==
      G_GINT64_CONSTANT (0xFFFFFFFF), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**32-1", NULL) ==
      G_GINT64_CONSTANT (0xFFFFFFFF), "");
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**32-1", &valid) ==
      G_GINT64_CONSTANT (0xFFFFFFFF), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**32", NULL) ==
      G_GINT64_CONSTANT (0x100000000), "");
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**32", &valid) ==
      G_GINT64_CONSTANT (0x100000000), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**64-1", NULL) == 0, "");
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**64-1", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i64:-2**63", NULL) ==
      G_GINT64_CONSTANT (-0x80000000) * G_GINT64_CONSTANT (0x100000000), "");
  MYASSERT (tp_asv_get_int64 (hash, "i64:-2**63", &valid) ==
      G_GINT64_CONSTANT (-0x80000000) * G_GINT64_CONSTANT (0x100000000), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "i64:2**63-1", NULL) ==
      G_GINT64_CONSTANT (0x7FFFFFFFFFFFFFFF), "");
  MYASSERT (tp_asv_get_int64 (hash, "i64:2**63-1", &valid) ==
      G_GINT64_CONSTANT (0x7FFFFFFFFFFFFFFF), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**63-1", NULL) ==
      G_GINT64_CONSTANT (0x7FFFFFFFFFFFFFFF), "");
  MYASSERT (tp_asv_get_int64 (hash, "u64:2**63-1", &valid) ==
      G_GINT64_CONSTANT (0x7FFFFFFFFFFFFFFF), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  /* Tests: tp_asv_get_uint64 */

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "s", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "s", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "b:TRUE", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "b:TRUE", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "d:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "d:0", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "not-there", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "not-there", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "i32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u32:0", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "u32:0", &valid) == 0, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_uint64 (hash, "i32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**16", NULL) == 0x10000, "");
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**16", &valid) == 0x10000, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i32:-2**16", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "i32:-2**16", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i32:-2**31", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "i32:-2**31", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_uint64 (hash, "i32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**31-1", NULL) == 0x7FFFFFFF, "");
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**31-1", &valid) == 0x7FFFFFFF, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**31", NULL) == 0x80000000U, "");
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**31", &valid) == 0x80000000U, "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**32-1", NULL) == 0xFFFFFFFFU, "");
  MYASSERT (tp_asv_get_uint64 (hash, "u32:2**32-1", &valid) == 0xFFFFFFFFU,
      "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**32-1", NULL) == 0xFFFFFFFFU, "");
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**32-1", &valid) == 0xFFFFFFFFU,
      "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**32", NULL) ==
      G_GUINT64_CONSTANT (0x100000000), "");
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**32", &valid) ==
      G_GUINT64_CONSTANT (0x100000000), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**64-1", NULL) ==
      G_GUINT64_CONSTANT (0xFFFFffffFFFFffff), "");
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**64-1", &valid) ==
      G_GUINT64_CONSTANT (0xFFFFffffFFFFffff), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i64:-2**63", NULL) == 0, "");
  MYASSERT (tp_asv_get_uint64 (hash, "i64:-2**63", &valid) == 0, "");
  MYASSERT (valid == FALSE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "i64:2**63-1", NULL) ==
      G_GUINT64_CONSTANT (0x7FFFffffFFFFffff), "");
  MYASSERT (tp_asv_get_uint64 (hash, "i64:2**63-1", &valid) ==
      G_GUINT64_CONSTANT (0x7FFFffffFFFFffff), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  valid = (gboolean) 123;
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**63-1", NULL) ==
      G_GUINT64_CONSTANT (0x7FFFffffFFFFffff), "");
  MYASSERT (tp_asv_get_uint64 (hash, "u64:2**63-1", &valid) ==
      G_GUINT64_CONSTANT (0x7FFFffffFFFFffff), "");
  MYASSERT (valid == TRUE, ": %u", (guint) valid);

  /* Tests: tp_asv_get_string */

  MYASSERT (tp_asv_get_string (hash, "s") != NULL, "");
  MYASSERT (g_str_equal (tp_asv_get_string (hash, "s"), "hello, world!"), "");

  MYASSERT (tp_asv_get_string (hash, "s0") != NULL, "");
  MYASSERT (g_str_equal (tp_asv_get_string (hash, "s0"), ""), "");

  MYASSERT (tp_asv_get_string (hash, "b:TRUE") == NULL, "");
  MYASSERT (tp_asv_get_string (hash, "b:FALSE") == NULL, "");
  MYASSERT (tp_asv_get_string (hash, "not-there") == NULL, "");
  MYASSERT (tp_asv_get_string (hash, "i32:0") == NULL, "");
  MYASSERT (tp_asv_get_string (hash, "u32:0") == NULL, "");
  MYASSERT (tp_asv_get_string (hash, "d:0") == NULL, "");

  /* Tests: tp_asv_get_object_path */

  MYASSERT (tp_asv_get_object_path (hash, "o") != NULL, "");
  MYASSERT (g_str_equal (tp_asv_get_object_path (hash, "o"),
        "/com/example/Object"), "");

  MYASSERT (tp_asv_get_object_path (hash, "s") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "s0") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "b:TRUE") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "b:FALSE") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "not-there") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "i32:0") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "u32:0") == NULL, "");
  MYASSERT (tp_asv_get_object_path (hash, "d:0") == NULL, "");

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

  g_hash_table_destroy (hash);

  return fail;
}
