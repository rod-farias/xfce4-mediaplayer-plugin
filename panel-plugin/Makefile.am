plugindir = $(libdir)/xfce4/panel/plugins

plugin_LTLIBRARIES = libmediaplayer.la

libmediaplayer_la_SOURCES = \
	mediaplayer.c \
	mediaplayer.h \
	mediaplayer-mpris.c \
	mediaplayer-mpris.h \
	mediaplayer-dialogs.c \
	mediaplayer-dialogs.h

libmediaplayer_la_CFLAGS = \
	$(GTK_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4UTIL_CFLAGS) \
	-DG_LOG_DOMAIN=\"mediaplayer\"

libmediaplayer_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_construct$$'

libmediaplayer_la_LIBADD = \
	$(GTK_LIBS) \
	$(GLIB_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4UTIL_LIBS)
