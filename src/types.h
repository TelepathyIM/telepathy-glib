
#ifndef __TYPES_H__
#define __TYPES_H__

#define TP_TYPE_TRANSPORT_STRUCT (dbus_g_type_get_struct ("GValueArray", \
      G_TYPE_UINT, \
      G_TYPE_STRING, \
      G_TYPE_UINT, \
      G_TYPE_UINT, \
      G_TYPE_STRING, \
      G_TYPE_STRING, \
      G_TYPE_DOUBLE, \
      G_TYPE_UINT, \
      G_TYPE_STRING, \
      G_TYPE_STRING, \
      G_TYPE_INVALID))

#define TP_TYPE_TRANSPORT_LIST (dbus_g_type_get_collection ("GPtrArray", \
      TP_TYPE_TRANSPORT_STRUCT))

#define TP_TYPE_CANDIDATE_STRUCT (dbus_g_type_get_struct ("GValueArray", \
      G_TYPE_STRING, \
      TP_TYPE_TRANSPORT_LIST, \
      G_TYPE_INVALID))

#define TP_TYPE_CANDIDATE_LIST (dbus_g_type_get_collection ("GPtrArray", \
      TP_TYPE_CANDIDATE_STRUCT))

#define TP_TYPE_CODEC_STRUCT (dbus_g_type_get_struct ("GValueArray", \
      G_TYPE_UINT, \
      G_TYPE_STRING, \
      G_TYPE_UINT, \
      G_TYPE_UINT, \
      G_TYPE_UINT, \
      DBUS_TYPE_G_STRING_STRING_HASHTABLE, \
      G_TYPE_INVALID))

#define TP_TYPE_CODEC_LIST (dbus_g_type_get_collection ("GPtrArray", \
      TP_TYPE_CODEC_STRUCT))

#define TP_TYPE_PROPERTY_DESCRIPTION (dbus_g_type_get_struct ("GValueArray", \
      G_TYPE_UINT, \
      G_TYPE_STRING, \
      G_TYPE_STRING, \
      G_TYPE_UINT))

#endif /* __TYPES_H__ */

