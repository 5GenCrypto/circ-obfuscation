m4_define([version_major], [0])
m4_define([version_minor], [0])
m4_define([version_micro], [0])

AC_INIT([ab-obfuscator], [version_major.version_minor.version_micro])

AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_SRCDIR([./])

AC_CONFIG_AUX_DIR([build/autoconf])
AC_CONFIG_MACRO_DIR([build/autoconf])
AM_INIT_AUTOMAKE([subdir-objects])

AM_PROG_AR
AC_PROG_RANLIB

AC_ARG_ENABLE(debug,
[AS_HELP_STRING([--enable-debug], [enable debugging])],
[], [enable_debug=no])

CFLAGS=                         dnl get rid of default -g -O2
CFLAGS="-Wall -Wformat -Wformat-security -Wextra -Wunused \
-Wshadow -Wmissing-prototypes -Wfloat-equal -Wpointer-arith -Wcast-align \
-Wstrict-prototypes -Wredundant-decls -Wendif-labels -Wcast-qual \
-std=gnu11 -Wpedantic"

if test "x$enable_debug" = x"yes"; then
  EXTRA_CFLAGS="-O0 -g"
else
  EXTRA_CFLAGS="-O3"
  AC_DEFINE(NDEBUG,1,[Define whether debugging is enabled])
fi
AC_SUBST(COMMON_CFLAGS)
AC_SUBST(EXTRA_CFLAGS)


AC_SEARCH_LIBS(acirc_init, acirc, [], AC_MSG_ERROR([libacirc not found]))
AC_SEARCH_LIBS(aes_randinit, aesrand, [], AC_MSG_ERROR([libaesrand not found]))
AC_SEARCH_LIBS(mmap_enc_mat_init, mmap, [], AC_MSG_ERROR([libmmap not found]))
AC_SEARCH_LIBS(threadpool_create, threadpool, [], AC_MSG_ERROR([libthreadpool not found]))

AC_SEARCH_LIBS(floor, m, [], AC_MSG_ERROR([libm not found]))
AC_SEARCH_LIBS(_fmpz_clear_mpz, flint, [], AC_MSG_ERROR([libflint not found]))
AC_SEARCH_LIBS(__gmpz_init, gmp, [], AC_MSG_ERROR([libgmp not found]))

AC_CONFIG_FILES([Makefile src/ab/Makefile src/zim/Makefile src/Makefile])

AC_OUTPUT
