AC_DEFUN([SEQ_DIAS],
[
dnl check for tools for drawing sequence diagrams
AC_ARG_ENABLE(seq-dias,
      AC_HELP_STRING([--enable-seq-dias],
                         [use plotutils to draw sequence diagrams [default=yes]]),,
          enable_seq_dias=yes)

MAKE_SEQ_DIAS=no
if test x$enable_seq_dias = xyes; then
  MAKE_SEQ_DIAS=yes
  AC_PATH_PROG(PIC2PLOT, pic2plot, no)
  AC_PATH_PROG(GS, gs, no)
  AC_PATH_PROG(CONVERT, convert, no)
  if test "$CONVERT" == "no"; then
      AC_MSG_WARN([Imagemagick not found, drawing sequence diagrams will be disabled.])
    MAKE_SEQ_DIAS=no
      fi
        if test "$GS" == "no"; then
            AC_MSG_WARN([Ghostscript not found, drawing sequence diagrams will be disabled.])
    MAKE_SEQ_DIAS=no
      fi
        if test "$PIC2PLOT" == "no"; then
            AC_MSG_WARN([GNU plotutils not found, drawing sequence diagrams will be disabled.])
    MAKE_SEQ_DIAS=no
      fi
        AC_SUBST(PIC2PLOT)
  AC_SUBST(CONVERT)

fi
AC_SUBST(MAKE_SEQ_DIAS)
AM_CONDITIONAL(MAKE_SEQ_DIAS, test x$MAKE_SEQ_DIAS == xyes)
])
