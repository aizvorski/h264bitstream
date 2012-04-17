AC_INIT(h264bitstream, 0.2.0, aizvorski@gmail.com)

AC_CONFIG_SRCDIR(./h264_stream.c)

AC_CONFIG_AUX_DIR(build-aux)

# Make sure to compute the target so we know if we are on OS X
AC_CANONICAL_TARGET

# Build fat binaries on OS X
EXTRA_CFLAGS='-std=c99 -Wno-error'

if test ${target_os%%.*} = darwin9; then
   EXTRA_CFLAGS="-force_cpusubtype_ALL -mmacosx-version-min=10.4 -arch i386 -arch ppc -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.4u.sdk"
   CFLAGS+=${EXTRA_CFLAGS}
   am_cv_CC_dependencies_compiler_type=gcc
fi


AC_SUBST(EXTRA_CFLAGS)

AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_LD
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_CHECK_FUNCS(getopt_long, , AC_MSG_WARN(getopt_long not found. Long options will not work.) )

AC_CONFIG_FILES([Makefile])
AC_CONFIG_MACRO_DIR([m4])

AX_CHECK_DEBUG

ax_create_pkgconfig_src_libdir=`pwd`
AX_CREATE_PKGCONFIG_INFO(libh264bitstream.pc, , '\\\${libdir}/libh264bitstream.la')

AC_OUTPUT
