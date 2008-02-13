#ifndef TP_TESTS_MYASSERT_H
#define TP_TESTS_MYASSERT_H

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

#endif
