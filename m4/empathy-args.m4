dnl configure-time options for Empathy

dnl EMPATHY_ARG_VALGRIND

AC_DEFUN([EMPATHY_ARG_VALGRIND],
[
  dnl valgrind inclusion
  AC_ARG_ENABLE(valgrind,
    AC_HELP_STRING([--enable-valgrind],[enable valgrind checking and run-time detection]),
    [
      case "${enableval}" in
        yes|no) enable="${enableval}" ;;
        *)   AC_MSG_ERROR(bad value ${enableval} for --enable-valgrind) ;;
      esac
    ],
    [enable=no])

  EMPATHY_VALGRIND($enable, [2.1])
])
