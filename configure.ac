dnl configure.ac for xfce4-mediaplayer-plugin

AC_PREREQ([2.69])
AC_INIT([xfce4-mediaplayer-plugin], [0.2.1], [https://github.com/], [xfce4-mediaplayer-plugin])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-bzip2 tar-ustar foreign])
AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE()

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O
LT_INIT

dnl ***********************************
dnl *** Check for required packages ***
dnl ***********************************
PKG_CHECK_MODULES([GTK], [gtk+-3.0 >= 3.22.0])
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.50.0 gio-2.0 >= 2.50.0 gio-unix-2.0 >= 2.50.0])
PKG_CHECK_MODULES([LIBXFCE4PANEL], [libxfce4panel-2.0 >= 4.16.0])
PKG_CHECK_MODULES([LIBXFCE4UI], [libxfce4ui-2 >= 4.16.0])
PKG_CHECK_MODULES([LIBXFCE4UTIL], [libxfce4util-1.0 >= 4.16.0])

AC_CONFIG_FILES([
Makefile
panel-plugin/Makefile
])
AC_OUTPUT

echo ""
echo "Configuration for $PACKAGE_NAME $PACKAGE_VERSION:"
echo ""
echo "  Prefix.................: $prefix"
echo "  C compiler..............: $CC"
echo ""
