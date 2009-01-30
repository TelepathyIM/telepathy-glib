/* Auto-generated, do not edit.
 *
 * This file may be distributed under the same terms
 * as the specification from which it was generated.
 */

GType
the_prefix_type_dbus_hash_ss (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_STRING);
  return t;
}

GType
the_prefix_type_dbus_hash_sa_7bsv_7d (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE)));
  return t;
}

GType
the_prefix_type_dbus_hash_sv (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE);
  return t;
}

GType
the_prefix_type_dbus_hash_saa_7bsv_7d (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, (dbus_g_type_get_collection ("GPtrArray", (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE)))));
  return t;
}

GType
the_prefix_type_dbus_struct_isu (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_struct ("GValueArray",
        G_TYPE_INT,
        G_TYPE_STRING,
        G_TYPE_UINT,
        G_TYPE_INVALID);
  return t;
}

GType
the_prefix_type_dbus_array_isu (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_collection ("GPtrArray", the_prefix_type_dbus_struct_isu ());
  return t;
}

GType
the_prefix_type_dbus_array_of_a_7bsa_7bsv_7d_7d (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_collection ("GPtrArray", the_prefix_type_dbus_hash_sa_7bsv_7d ());
  return t;
}

GType
the_prefix_type_dbus_array_of_a_7bsv_7d (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_collection ("GPtrArray", the_prefix_type_dbus_hash_sv ());
  return t;
}

GType
the_prefix_type_dbus_array_of_a_7bsaa_7bsv_7d_7d (void)
{
  static GType t = 0;

  if (G_UNLIKELY (t == 0))
    t = dbus_g_type_get_collection ("GPtrArray", the_prefix_type_dbus_hash_saa_7bsv_7d ());
  return t;
}

