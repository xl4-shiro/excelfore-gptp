#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([xl4gptp], [1.2.3], [shiro@excelfore.com])
AC_SUBST(PACKAGE_DESC,"Excelfore gptp daemon and libary")

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects foreign])
AM_PROG_AR
LT_PREREQ([2.2])
LT_INIT([static])
AC_CONFIG_SRCDIR([gptpbasetypes.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O
AC_PROG_CC_C99

# pkg-config dependency
PKG_PROG_PKG_CONFIG
PKG_INSTALLDIR

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
AC_ARG_ENABLE([ekf_armada],
	AS_HELP_STRING([--enable-ekf-armada],[enable EKF ARMADA board variation]))
AC_ARG_ENABLE([understpl],
	AS_HELP_STRING([--enable-understpl],[enable understpl mode build]))
AC_ARG_ENABLE([ti-dp83867a],
	AS_HELP_STRING([--enable-ti-dp83867a],[enable TI DP83867A PHY PTP]))

# Checks for libraries.
PKG_CHECK_MODULES([x4unibase],[xl4unibase],,
	[AC_CHECK_HEADERS([xl4unibase/unibase.h])]
)
PKG_CHECK_MODULES([x4combase],[xl4combase],,
	[AC_CHECK_HEADERS([xl4combase/combase.h])]
)

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

# Armada platform build
# Supported platforms: EKF AC370/AL900
AM_CONDITIONAL(EKF_ARMADA, [test x"$enable_ekf_armada" = "xyes"])

AM_CONDITIONAL(TI_DP83867A, [test x"$enable_ti_dp83867a" = "xyes"])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNC([malloc], [], [AC_MSG_ERROR([malloc() function not found.])])
AC_CHECK_FUNC([realloc], [], [AC_MSG_ERROR([realloc() function not found.])])
AC_CHECK_FUNC([memmove], [], [AC_MSG_ERROR([memmove() function not found.])])
AC_CHECK_FUNC([memset], [], [AC_MSG_ERROR([memset() function not found.])])
AC_CHECK_FUNC([strtol], [], [AC_MSG_ERROR([strtol() function not found.])])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([
	Makefile
	xl4gptp.pc])

AC_OUTPUT
