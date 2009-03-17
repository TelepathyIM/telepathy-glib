#ifndef TP_TESTS_MYASSERT_H
#define TP_TESTS_MYASSERT_H

#include <glib.h>
#include <telepathy-glib/util.h>

#define MYASSERT(assertion, extra_format, ...)\
  G_STMT_START {\
      if (!(assertion))\
        {\
          g_error ("\n%s:%d: Assertion failed: %s" extra_format,\
            __FILE__, __LINE__, #assertion, ##__VA_ARGS__);\
        }\
  } G_STMT_END

#define MYASSERT_SAME_ERROR(left, right) \
  G_STMT_START {\
    MYASSERT ((left)->domain == (right)->domain,\
        ": (%s #%d \"%s\") != (%s #%d \"%s\")",\
        g_quark_to_string ((left)->domain), (left)->code, (left)->message,\
        g_quark_to_string ((right)->domain), (right)->code, (right)->message);\
    MYASSERT ((left)->code == (right)->code,\
        ": (%s #%d \"%s\") != (%s #%d \"%s\")",\
        g_quark_to_string ((left)->domain), (left)->code, (left)->message,\
        g_quark_to_string ((right)->domain), (right)->code, (right)->message);\
    MYASSERT (!tp_strdiff ((left)->message, (right)->message),\
        ": (%s #%d \"%s\") != (%s #%d \"%s\")",\
        g_quark_to_string ((left)->domain), (left)->code, (left)->message,\
        g_quark_to_string ((right)->domain), (right)->code, (right)->message);\
  } G_STMT_END

#define MYASSERT_SAME_STRING(left, right) \
  g_assert_cmpstr ((left), ==, (right));

#define MYASSERT_SAME_UINT(left, right) \
  g_assert_cmpuint ((left), ==, (right))

#endif
