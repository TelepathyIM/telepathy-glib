/* Auto-generated, do not edit.
 *
 * This file may be distributed under the same terms
 * as the specification from which it was generated.
 */

/**
 * THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP:
 *
 * (Undocumented)
 *
 * This macro expands to a call to a function
 * that returns the #GType of a #GHashTable
 * appropriate for representing a D-Bus
 * dictionary of signature
 * <literal>a{sv}</literal>.
 *
 * Keys (D-Bus type <literal>s</literal>,
 * named <literal>Key</literal>):
 * (Undocumented)
 *
 * Values (D-Bus type <literal>v</literal>,
 * named <literal>Value</literal>):
 * (Undocumented)
 *
 */
#define THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP (the_prefix_type_dbus_hash_sv ())

/**
 * THE_PREFIX_ARRAY_TYPE_STRING_VARIANT_MAP_LIST:

 * Expands to a call to a function
 * that returns the #GType of a #GPtrArray
 * of #THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP.
 */
#define THE_PREFIX_ARRAY_TYPE_STRING_VARIANT_MAP_LIST (the_prefix_type_dbus_array_of_a_7bsv_7d ())

/**
 * THE_PREFIX_HASH_TYPE_STRING_STRING_MAP:
 *
 * (Undocumented)
 *
 * This macro expands to a call to a function
 * that returns the #GType of a #GHashTable
 * appropriate for representing a D-Bus
 * dictionary of signature
 * <literal>a{ss}</literal>.
 *
 * Keys (D-Bus type <literal>s</literal>,
 * named <literal>Key</literal>):
 * (Undocumented)
 *
 * Values (D-Bus type <literal>s</literal>,
 * named <literal>Value</literal>):
 * (Undocumented)
 *
 */
#define THE_PREFIX_HASH_TYPE_STRING_STRING_MAP (the_prefix_type_dbus_hash_ss ())

/**
 * THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP_MAP:
 *
 * (Undocumented)
 *
 * This macro expands to a call to a function
 * that returns the #GType of a #GHashTable
 * appropriate for representing a D-Bus
 * dictionary of signature
 * <literal>a{sa{sv}}</literal>.
 *
 * Keys (D-Bus type <literal>s</literal>,
 * named <literal>Key</literal>):
 * (Undocumented)
 *
 * Values (D-Bus type <literal>a{sv}</literal>,
 * type <literal>String_Variant_Map</literal>,
 * named <literal>Value</literal>):
 * (Undocumented)
 *
 */
#define THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP_MAP (the_prefix_type_dbus_hash_sa_7bsv_7d ())

/**
 * THE_PREFIX_ARRAY_TYPE_STRING_VARIANT_MAP_MAP_LIST:

 * Expands to a call to a function
 * that returns the #GType of a #GPtrArray
 * of #THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP_MAP.
 */
#define THE_PREFIX_ARRAY_TYPE_STRING_VARIANT_MAP_MAP_LIST (the_prefix_type_dbus_array_of_a_7bsa_7bsv_7d_7d ())

/**
 * THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP_LIST_MAP:
 *
 * (Undocumented)
 *
 * This macro expands to a call to a function
 * that returns the #GType of a #GHashTable
 * appropriate for representing a D-Bus
 * dictionary of signature
 * <literal>a{saa{sv}}</literal>.
 *
 * Keys (D-Bus type <literal>s</literal>,
 * named <literal>Key</literal>):
 * (Undocumented)
 *
 * Values (D-Bus type <literal>aa{sv}</literal>,
 * type <literal>String_Variant_Map[]</literal>,
 * named <literal>Value</literal>):
 * (Undocumented)
 *
 */
#define THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP_LIST_MAP (the_prefix_type_dbus_hash_saa_7bsv_7d ())

/**
 * THE_PREFIX_ARRAY_TYPE_STRING_VARIANT_MAP_LIST_MAP_LIST:

 * Expands to a call to a function
 * that returns the #GType of a #GPtrArray
 * of #THE_PREFIX_HASH_TYPE_STRING_VARIANT_MAP_LIST_MAP.
 */
#define THE_PREFIX_ARRAY_TYPE_STRING_VARIANT_MAP_LIST_MAP_LIST (the_prefix_type_dbus_array_of_a_7bsaa_7bsv_7d_7d ())

GType the_prefix_type_dbus_hash_ss (void);

GType the_prefix_type_dbus_hash_sa_7bsv_7d (void);

GType the_prefix_type_dbus_hash_sv (void);

GType the_prefix_type_dbus_hash_saa_7bsv_7d (void);

/**
 * THE_PREFIX_STRUCT_TYPE_STRUCT:

 * (Undocumented)
 *
 * This macro expands to a call to a function
 * that returns the #GType of a #GValueArray
 * appropriate for representing a D-Bus struct
 * with signature <literal>(isu)</literal>.
 *
 * Member 0 (D-Bus type <literal>i</literal>,
 * named <literal>Int</literal>):
 * (Undocumented)
 *
 * Member 1 (D-Bus type <literal>s</literal>,
 * named <literal>String</literal>):
 * (Undocumented)
 *
 * Member 2 (D-Bus type <literal>u</literal>,
 * named <literal>UInt</literal>):
 * (Undocumented)
 *
 */
#define THE_PREFIX_STRUCT_TYPE_STRUCT (the_prefix_type_dbus_struct_isu ())

/**
 * THE_PREFIX_ARRAY_TYPE_STRUCT_LIST:

 * Expands to a call to a function
 * that returns the #GType of a #GPtrArray
 * of #THE_PREFIX_STRUCT_TYPE_STRUCT.
 */
#define THE_PREFIX_ARRAY_TYPE_STRUCT_LIST (the_prefix_type_dbus_array_isu ())

GType the_prefix_type_dbus_struct_isu (void);

GType the_prefix_type_dbus_array_isu (void);

GType the_prefix_type_dbus_array_of_a_7bsa_7bsv_7d_7d (void);

GType the_prefix_type_dbus_array_of_a_7bsv_7d (void);

GType the_prefix_type_dbus_array_of_a_7bsaa_7bsv_7d_7d (void);

