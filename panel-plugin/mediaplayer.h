#ifndef __MEDIAPLAYER_H__
#define __MEDIAPLAYER_H__

#include <libxfce4panel/libxfce4panel.h>

#include "mediaplayer-mpris.h"

G_BEGIN_DECLS

typedef struct _MediaplayerPlugin MediaplayerPlugin;

struct _MediaplayerPlugin
{
  XfcePanelPlugin *plugin;

  MediaplayerMpris *mpris;

  GtkWidget *ebox;
  GtkWidget *content_box;
  GtkWidget *unsupported_icon;
  GtkWidget *box;
  GtkWidget *btn_previous;
  GtkWidget *btn_play_pause;
  GtkWidget *btn_next;
  GtkWidget *img_play_pause;
  GtkWidget *text_box;
  GtkWidget *info_row;
  GtkWidget *label;
  GtkWidget *progress_label;
  GtkWidget *progress_bar;
  GtkWidget *img_art;

  gboolean show_title;
  gboolean show_artist;
  gboolean show_album;
  gint     label_width_chars;
  gboolean controls_right;
  gboolean show_album_art;
  gboolean show_progress;

  gchar *art_cache_url;
  gint   art_cache_size;

  gchar *last_label_text;
  guint  no_player_timer_id;

  guint progress_timer_id;

  GtkCssProvider *bar_css_provider;
  GtkCssProvider *text_css_provider;
};

#define MEDIAPLAYER_LABEL_WIDTH_MIN     6
#define MEDIAPLAYER_LABEL_WIDTH_MAX     80
#define MEDIAPLAYER_LABEL_WIDTH_DEFAULT 50

/* minimum panel row size (px) before album art is worth showing */
#define MEDIAPLAYER_ART_MIN_PANEL_SIZE 28
#define MEDIAPLAYER_ART_MAX_SIZE       64
#define MEDIAPLAYER_ART_MARGIN         4

/* fixed width (in characters) of the elapsed/total time indicator,
 * so it doesn't shift the layout as the digits change */
#define MEDIAPLAYER_PROGRESS_TIME_WIDTH_CHARS 11

/* height (px) of the progress bar acting as a background behind the
 * overlaid text; tall enough to read the label against, but not so
 * tall it visually overwhelms a single text line */
#define MEDIAPLAYER_PROGRESS_BAR_HEIGHT 20

/* how long to keep showing the previous track's text across a gap
 * with no metadata (e.g. the brief moment between tracks) before
 * falling back to "No player" */
#define MEDIAPLAYER_NO_PLAYER_GRACE_SECONDS 3

void mediaplayer_save (XfcePanelPlugin *plugin, MediaplayerPlugin *mp);
void mediaplayer_update_ui (MediaplayerPlugin *mp);
void mediaplayer_apply_controls_position (MediaplayerPlugin *mp);
void mediaplayer_apply_label_width (MediaplayerPlugin *mp);

G_END_DECLS

#endif /* !__MEDIAPLAYER_H__ */
