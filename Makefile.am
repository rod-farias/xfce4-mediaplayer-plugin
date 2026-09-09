SUBDIRS = panel-plugin

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_DATA = mediaplayer.desktop

EXTRA_DIST = \
	$(desktop_DATA)

.PHONY: ChangeLog

ChangeLog: Makefile
	(GIT_DIR=$(top_srcdir)/.git git log > .changelog.tmp && mv .changelog.tmp ChangeLog; rm -f .changelog.tmp) || (touch ChangeLog; echo 'Git directory not found: installing possibly empty changelog.' >&2)

dist-hook: ChangeLog
