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

  /* Setup */

  g_type_init ();

  hash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);

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
  g_value_set_int (value, 0x7FFFFFFF);
  g_hash_table_insert (hash, "i32:2**31-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0x7FFFFFFF);
  g_hash_table_insert (hash, "u32:2**31-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0x80000000U);
  g_hash_table_insert (hash, "u32:2**31", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT);
  g_value_set_uint (value, 0xFFFFFFFFU);
  g_hash_table_insert (hash, "u32:2**32-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, 0xFFFFFFFFU);
  g_hash_table_insert (hash, "u64:2**32-1", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, G_GUINT64_CONSTANT (0x100000000));
  g_hash_table_insert (hash, "u64:2**32", value);
  value = NULL;

  value = tp_g_value_slice_new (G_TYPE_UINT64);
  g_value_set_uint64 (value, G_GUINT64_CONSTANT (0xFFFFFFFFFFFFFFFF));
  g_hash_table_insert (hash, "u64:2**64-1", value);
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

  /* Teardown */

  g_hash_table_destroy (hash);

  return fail;
}
