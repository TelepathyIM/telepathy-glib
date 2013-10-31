dnl Detect Valgrind location and flags

AC_DEFUN([EMPATHY_VALGRIND],
[
  enable=$1
  if test -n "$2"; then
    valgrind_req=$2
  else
    valgrind_req="2.1"
  fi

  PKG_CHECK_MODULES(VALGRIND, valgrind > "$valgrind_req",
    have_valgrind_runtime="yes", have_valgrind_runtime="no")

  AC_PATH_PROG(VALGRIND_PATH, valgrind)

  # Compile the instrumentation for valgrind only if the valgrind
  # libraries are installed and the valgrind executable is found
  if test "x$enable" = xyes &&
     test "$have_valgrind_runtime" = yes &&
     test -n "$VALGRIND_PATH" ;
  then
    AC_DEFINE(HAVE_VALGRIND, 1, [Define if valgrind should be used])
    AC_MSG_NOTICE(using compile-time instrumentation for valgrind)
  fi

  AC_SUBST(VALGRIND_CFLAGS)
  AC_SUBST(VALGRIND_LIBS)

  AM_CONDITIONAL(HAVE_VALGRIND, test -n "$VALGRIND_PATH")
])
