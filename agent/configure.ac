#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([openolt], [1.4.0], [voltha-discuss@opencord.org])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

AC_ARG_WITH(sysroot,
  [AS_HELP_STRING([--with-sysroot[=DIR]], [search for usr/include, usr/lib, usr/lib64 etal within DIR])],
  [
  sysroot=$withval

  if test "$sysroot" = "yes"; then
    sysroot='${exec_prefix}/${target_alias}/sys-root'
  elif test "$sysroot" = "no"; then
    sysroot=
  fi

  if test "x$sysroot" != x; then
    CXXFLAGS="$CXXFLAGS --sysroot=$sysroot"
    LDFLAGS="$LDFLAGS --sysroot=$sysroot"
  fi
  ], [])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_AWK
AC_PROG_SED
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([dl], [main])
AC_CHECK_LIB([pthread], [main])

# Checks for header files.
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
