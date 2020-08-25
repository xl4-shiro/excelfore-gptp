#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([xl4gptp], [1.0], [shiro@excelfore.com])
AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])
AM_PROG_AR
LT_PREREQ([2.2])
LT_INIT([static])
AC_CONFIG_SRCDIR([gptpbasetypes.h])

# Checks for programs.
PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O
AC_PROG_CC_C99

# Check if posix platform
AC_CHECK_HEADERS([pthread.h],
	[AM_CONDITIONAL(POSIX_PLATFORM,[true])])

# Check if linux ethtool is available
AC_CHECK_HEADERS([linux/ptp_clock.h],
	[AM_CONDITIONAL(LINUX_PTPCLOCK,[true])])

# Check if sys/io is available
AC_CHECK_HEADERS([sys/io.h],
        [AM_CONDITIONAL(HAVE_SYSIO,[true])],
        [AM_CONDITIONAL(HAVE_SYSIO,[false])])

# Check for command line options
AC_ARG_ENABLE([unittest],
	AS_HELP_STRING([--disable-unittest],[disable building unittest programs]))
AC_ARG_ENABLE([unittest_memtest],
	AS_HELP_STRING([--enable-unittest-memtest],[check memory leak in unittest]))
AC_ARG_ENABLE([sja1105_linux],
	AS_HELP_STRING([--enable-sja1105-linux],[enable sja1105 linux mode]))
AC_ARG_ENABLE([understpl],
	AS_HELP_STRING([--enable-understpl],[enable understpl mode build]))

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([stdio.h stdlib.h string.h xl4unibase/unibase.h xl4combase/combase.h])

if test x"$enable_unittest" != "xno"; then
AC_CHECK_HEADERS([setjmp.h cmocka.h],[],
	[AC_MSG_ERROR([you may need to install libcmocka-dev package])],
	[[#include <string.h>
	#include <stdio.h>
	#include <setjmp.h>
	]])
fi

# conditional build
AM_CONDITIONAL(UNITTEST_MEMTEST, [test x"$enable_unittest_memtest" = "xyes"])
AM_CONDITIONAL(BUILD_IN_STPL, [test x"$enable_understpl" = "xyes"])

# sja1105 molex switch
AM_CONDITIONAL(SJA1105_LINUX, [test "x$enable_sja1105_linux" = "xyes"])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove memset strtol])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
