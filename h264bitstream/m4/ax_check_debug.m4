#############################################################
# AX_CHECK_DEBUG
#
# Debug and default optimization compile flags (DEBUG / NDEBUG, optimization level, -g)
# 
# LICENSE
# Copyright (C) 2012 BitGravity LLC, a Tata company
# All Rights Reserved
#
#############################################################


AC_DEFUN([AX_CHECK_DEBUG],
[
    AC_MSG_CHECKING([Checking for --enable-debug flag])
    AC_ARG_ENABLE([debug],
        [AS_HELP_STRING([--enable-debug],
                    [Set DEBUG mode, no compiler optimization, default is no. Symbols (-g) is included in both debug and release mode])],
        [usedebug="$enableval"],
        [usedebug="no"])
    
    AC_MSG_RESULT([$usedebug])
    if test "x$usedebug" = "xyes" ; then
        AC_DEFINE([DEBUG],[],[Debug Mode])
        AM_CFLAGS="$AM_CFLAGS -O0 -g -Wall -Werror"
    else
        AC_DEFINE([NDEBUG],[],[Release Mode])
        #-O3 anyone? Kills using -g in release mode so we'll stick with O2 for now.
        AM_CFLAGS="$AM_CFLAGS -O2 -g -Wall"
    fi
    AC_SUBST([AM_CFLAGS])
])

