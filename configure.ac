#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])
AC_INIT([xl4tsn], [1.0], [shiro@excelfore.com])
AC_PROG_CC
AM_INIT_AUTOMAKE([-Wall subdir-objects])
AM_PROG_AR
LT_PREREQ([2.2])
LT_INIT([static])

AC_CONFIG_SUBDIRS([xl4unibase xl4combase gptp2 ])
AC_SUBST([BSUBDIRS],["xl4unibase xl4combase gptp2 "])
AC_CONFIG_FILES([Makefile xl4unibase/Makefile xl4combase/Makefile gptp2/Makefile ])

AC_CONFIG_MACRO_DIR([m4])

AM_PATH_PYTHON([3])

# Check if posix platform
AC_CHECK_HEADERS([pthread.h],
	[AM_CONDITIONAL(POSIX_PLATFORM,[true])])

# Check if linux ethtool is available
AC_CHECK_HEADERS([linux/ptp_clock.h],
	[AM_CONDITIONAL(LINUX_PTPCLOCK,[true])])
AC_CHECK_HEADERS([linux/ethtool.h],
	[AM_CONDITIONAL(LINUX_ETHTOOL,[true])])

# Check if sys/io is available
AC_CHECK_HEADERS([sys/io.h],
        [AM_CONDITIONAL(HAVE_SYSIO,[true])],
        [AM_CONDITIONAL(HAVE_SYSIO,[false])])


AC_ARG_ENABLE([unittest],
	AS_HELP_STRING([--disable-unittest],[disable building unittest programs]))
AC_ARG_ENABLE([unittest_memtest],
	AS_HELP_STRING([--enable-unittest-memtest],[check memory leak in unittest]))
AC_ARG_ENABLE([ethernet],
	AS_HELP_STRING([--disable-ethernet],[disable building ethernet programs]))
AC_ARG_ENABLE([thread],
	AS_HELP_STRING([--disable-thread],[disable building threadding programs]))
AC_ARG_ENABLE([ipcshmem],
	AS_HELP_STRING([--disable-ipcshmem],[disable building shared memory ipc programs]))
AC_ARG_ENABLE([ipcsock],
	AS_HELP_STRING([--disable-ipcsock],[disable building socket ipc programs]))
AC_ARG_ENABLE([event],
	AS_HELP_STRING([--disable-event],[disable building eventing programs]))
AC_ARG_ENABLE([understpl],
	AS_HELP_STRING([--disable-understpl],[enable understpl mode build]))
AC_ARG_ENABLE([wrapper],
	AS_HELP_STRING([--enable-wrapper],[enable building avtpc wrapper when shared memory is not prefered]))

AM_CONDITIONAL(NO_ETHERNET, [test x"$enable_ethernet" = "xno"])
AM_CONDITIONAL(NO_INET, [test x"$enable_inet" = "xno"])
AM_CONDITIONAL(NO_THREAD, [test x"$enable_thread" = "xno"])
AM_CONDITIONAL(NO_IPCSHMEM, [test x"$enable_ipcshmem" = "xno"])
AM_CONDITIONAL(NO_IPCSOCK, [test x"$enable_ipcsock" = "xno"])
AM_CONDITIONAL(NO_EVENT, [test x"$enable_event" = "xno"])
AM_CONDITIONAL(UNITTEST_MEMTEST, [test x"$enable_unittest_memtest" = "xyes"])
AM_CONDITIONAL(BUILD_IN_STPL, [test x"$enable_understpl" != "xno"])
AM_CONDITIONAL(NO_H264, [test x"$enable_h264" = "xno"])
AM_CONDITIONAL(NO_MJPEG, [test x"$enable_mjpeg" = "xno"])
AM_CONDITIONAL(AVTPC_WRAPPER, [test x"$enable_wrapper" = "xyes"])


AC_OUTPUT
