#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.61])
AC_INIT([librudp], [0.3], [sdk@freebox.fr])
AC_CONFIG_MACRO_DIR([autotools])
AC_CONFIG_AUX_DIR([autotools])

AC_CONFIG_SRCDIR([src/rudp.c])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([1.10 foreign dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

# Initialize libtool
LT_PREREQ([2.2])
LT_INIT

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h])

AC_PATH_PROG([mkdoc], [mkdoc])
AM_CONDITIONAL(HAVE_MKDOC, test x$mkdoc != x)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_UINT32_T

AM_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES(ELA, ela)

# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES([
    rudp.pc
    Makefile
    doc/Makefile
    include/Makefile
    include/rudp/Makefile
    src/Makefile
    test/Makefile
    ])
AC_OUTPUT
