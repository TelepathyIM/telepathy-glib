#ifndef TP_TESTS_MYASSERT_H
#define TP_TESTS_MYASSERT_H

#include <glib.h>
#include <telepathy-glib/util.h>

/* code using this header must define */
static void myassert_failed (void);

#define MYASSERT(assertion, extra_format, ...)\
  G_STMT_START {\
      if (!(assertion))\
        {\
          g_critical ("\n%s:%d: Assertion failed: %s" extra_format,\
            __FILE__, __LINE__, #assertion, ##__VA_ARGS__);\
          myassert_failed ();\
        }\
  } G_STMT_END

#define MYASSERT_NO_ERROR(e) \
  MYASSERT (e == NULL, ": %s #%d: %s", g_quark_to_string (e->domain), e->code,\
      e->message)

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

#endif
