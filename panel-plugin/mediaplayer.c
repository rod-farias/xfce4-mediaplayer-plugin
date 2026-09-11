#include <string.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "mediaplayer.h"
#include "mediaplayer-dialogs.h"

static void
mediaplayer_load_settings (MediaplayerPlugin *mp)
{
  gchar *file;
  XfceRc *rc;

  mp->show_title = TRUE;
  mp->show_artist = TRUE;
  mp->show_album = FALSE;
  mp->label_width_chars = MEDIAPLAYER_LABEL_WIDTH_DEFAULT;
  mp->controls_right = FALSE;
  mp->show_album_art = TRUE;
  mp->show_progress = TRUE;

  file = xfce_panel_plugin_save_location (mp->plugin, TRUE);
  if (file == NULL)
    return;

  rc = xfce_rc_simple_open (file, TRUE);
  g_free (file);

  if (rc == NULL)
    return;

  mp->show_title = xfce_rc_read_bool_entry (rc, "ShowTitle", TRUE);
  mp->show_artist = xfce_rc_read_bool_entry (rc, "ShowArtist", TRUE);
  mp->show_album = xfce_rc_read_bool_entry (rc, "ShowAlbum", FALSE);
  mp->label_width_chars = CLAMP (xfce_rc_read_int_entry (rc, "LabelWidthChars", MEDIAPLAYER_LABEL_WIDTH_DEFAULT),
                                  MEDIAPLAYER_LABEL_WIDTH_MIN, MEDIAPLAYER_LABEL_WIDTH_MAX);
  mp->controls_right = xfce_rc_read_bool_entry (rc, "ControlsRight", FALSE);
  mp->show_album_art = xfce_rc_read_bool_entry (rc, "ShowAlbumArt", TRUE);
  mp->show_progress = xfce_rc_read_bool_entry (rc, "ShowProgress", TRUE);
  mediaplayer_mpris_set_preferred_player (mp->mpris, xfce_rc_read_entry (rc, "PreferredPlayer", NULL));

  xfce_rc_close (rc);

  mediaplayer_apply_label_width (mp);
  mediaplayer_apply_controls_position (mp);
}

void
mediaplayer_save (XfcePanelPlugin *plugin, MediaplayerPlugin *mp)
{
  gchar *file;
  XfceRc *rc;
  const gchar *preferred;

  file = xfce_panel_plugin_save_location (plugin, TRUE);
  if (file == NULL)
    return;

  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  if (rc == NULL)
    return;

  xfce_rc_write_bool_entry (rc, "ShowTitle", mp->show_title);
  xfce_rc_write_bool_entry (rc, "ShowArtist", mp->show_artist);
  xfce_rc_write_bool_entry (rc, "ShowAlbum", mp->show_album);
  xfce_rc_write_int_entry (rc, "LabelWidthChars", mp->label_width_chars);
  xfce_rc_write_bool_entry (rc, "ControlsRight", mp->controls_right);
  xfce_rc_write_bool_entry (rc, "ShowAlbumArt", mp->show_album_art);
  xfce_rc_write_bool_entry (rc, "ShowProgress", mp->show_progress);

  preferred = mediaplayer_mpris_get_preferred_player (mp->mpris);
  xfce_rc_write_entry (rc, "PreferredPlayer", preferred != NULL ? preferred : "");

  xfce_rc_close (rc);
}

void
mediaplayer_apply_controls_position (MediaplayerPlugin *mp)
{
  /* the art always stays leftmost; only the text (label + progress
   * bar) moves to the other side of the (untouched) button order. */
  gtk_box_reorder_child (GTK_BOX (mp->box), mp->img_art, 0);
  gtk_box_reorder_child (GTK_BOX (mp->box), mp->text_box, mp->controls_right ? 1 : -1);
}

void
mediaplayer_apply_label_width (MediaplayerPlugin *mp)
{
  PangoContext *context;
  PangoFontMetrics *metrics;
  gint char_width_px;

  gtk_label_set_max_width_chars (GTK_LABEL (mp->label), mp->label_width_chars);

  /* the progress bar is the overlay's sizing (base) child, so its
   * width has to be driven explicitly to match how wide the label is
   * allowed to grow -- an overlay child doesn't contribute its own
   * size to the overlay's request. */
  context = gtk_widget_get_pango_context (mp->label);
  metrics = pango_context_get_metrics (context, NULL, pango_context_get_language (context));
  char_width_px = pango_font_metrics_get_approximate_char_width (metrics) / PANGO_SCALE;
  pango_font_metrics_unref (metrics);

  gtk_widget_set_size_request (mp->progress_bar, char_width_px * mp->label_width_chars,
                                MEDIAPLAYER_PROGRESS_BAR_HEIGHT);
}

static gchar *
build_tooltip (MediaplayerPlugin *mp)
{
  const gchar *title = mediaplayer_mpris_get_title (mp->mpris);
  const gchar *artist = mediaplayer_mpris_get_artist (mp->mpris);
  const gchar *album = mediaplayer_mpris_get_album (mp->mpris);
  GPtrArray *lines;
  gchar *result;

  if (!mediaplayer_mpris_has_active_player (mp->mpris))
    return g_strdup ("No media player detected");

  if (title == NULL && artist == NULL && album == NULL)
    return g_strdup ("Media Player");

  lines = g_ptr_array_new ();
  if (title != NULL)
    g_ptr_array_add (lines, (gpointer) title);
  if (artist != NULL)
    g_ptr_array_add (lines, (gpointer) artist);
  if (album != NULL)
    g_ptr_array_add (lines, (gpointer) album);
  g_ptr_array_add (lines, NULL);

  result = g_strjoinv ("\n", (gchar **) lines->pdata);
  g_ptr_array_free (lines, TRUE);

  return result;
}

static void mediaplayer_update_art (MediaplayerPlugin *mp, gboolean active, const gchar *art_url);

static gboolean
mediaplayer_art_grace_timeout (MediaplayerPlugin *mp)
{
  mp->art_grace_timer_id = 0;
  mp->has_last_art = FALSE;
  mediaplayer_update_art (mp, mediaplayer_mpris_has_active_player (mp->mpris),
                           mediaplayer_mpris_get_art_url (mp->mpris));
  return G_SOURCE_REMOVE;
}

static void
mediaplayer_update_art (MediaplayerPlugin *mp, gboolean active, const gchar *art_url)
{
  gint panel_size = xfce_panel_plugin_get_size (mp->plugin);
  gboolean art_allowed = mp->show_album_art && panel_size >= MEDIAPLAYER_ART_MIN_PANEL_SIZE;
  gboolean has_art = art_allowed && active && art_url != NULL;

  if (!art_allowed)
    {
      gtk_widget_hide (mp->img_art);
      return;
    }

  if (has_art)
    {
      mp->has_last_art = TRUE;

      if (mp->art_grace_timer_id != 0)
        {
          g_source_remove (mp->art_grace_timer_id);
          mp->art_grace_timer_id = 0;
        }
    }
  else if (mp->has_last_art && mp->art_grace_timer_id == 0)
    {
      mp->art_grace_timer_id = g_timeout_add_seconds (MEDIAPLAYER_NO_PLAYER_GRACE_SECONDS,
                                                        (GSourceFunc) mediaplayer_art_grace_timeout, mp);
    }

  if (!has_art)
    {
      if (mp->has_last_art)
        return; /* momentary gap: keep showing the last art */

      gtk_widget_hide (mp->img_art);
      return;
    }

  {
    gint art_size = CLAMP (panel_size - MEDIAPLAYER_ART_MARGIN, 16, MEDIAPLAYER_ART_MAX_SIZE);

    if (g_strcmp0 (mp->art_cache_url, art_url) != 0 || mp->art_cache_size != art_size)
      {
        gchar *path;
        GdkPixbuf *pixbuf = NULL;
        GError *error = NULL;

        path = g_filename_from_uri (art_url, NULL, &error);
        if (path != NULL)
          {
            pixbuf = gdk_pixbuf_new_from_file_at_scale (path, art_size, art_size, TRUE, &error);
            g_free (path);
          }

        if (pixbuf != NULL)
          {
            gtk_image_set_from_pixbuf (GTK_IMAGE (mp->img_art), pixbuf);
            g_object_unref (pixbuf);
          }
        else
          {
            g_debug ("mediaplayer: could not load album art from %s: %s", art_url, error->message);
            gtk_image_clear (GTK_IMAGE (mp->img_art));
          }

        g_clear_error (&error);

        g_free (mp->art_cache_url);
        mp->art_cache_url = g_strdup (art_url);
        mp->art_cache_size = art_size;
      }
  }

  gtk_widget_show (mp->img_art);
}

static gchar *
format_time (gint64 microseconds)
{
  gint total_seconds = (gint) (microseconds / G_USEC_PER_SEC);
  gint hours = total_seconds / 3600;
  gint minutes = (total_seconds % 3600) / 60;
  gint seconds = total_seconds % 60;

  if (hours > 0)
    return g_strdup_printf ("%d:%02d:%02d", hours, minutes, seconds);

  return g_strdup_printf ("%d:%02d", minutes, seconds);
}

static void mediaplayer_update_progress (MediaplayerPlugin *mp);

static gboolean
mediaplayer_progress_tick (MediaplayerPlugin *mp)
{
  mediaplayer_update_progress (mp);
  return G_SOURCE_CONTINUE;
}

static gboolean
mediaplayer_progress_grace_timeout (MediaplayerPlugin *mp)
{
  mp->progress_grace_timer_id = 0;
  mp->has_last_progress = FALSE;
  mediaplayer_update_progress (mp);
  return G_SOURCE_REMOVE;
}

static void
mediaplayer_update_progress (MediaplayerPlugin *mp)
{
  gboolean active;
  MediaplayerStatus status;
  gint64 length;
  gboolean has_length;

  if (!mp->show_progress)
    {
      /* disabled by the user: hide immediately and drop any pending
       * grace state. Grace is only meant to smooth over a real,
       * momentary gap in the player's own data (folded into
       * has_length below) -- it must never kick in for a deliberate
       * setting change, or toggling this off would appear to do
       * nothing for the next few seconds. */
      mp->has_last_progress = FALSE;

      if (mp->progress_grace_timer_id != 0)
        {
          g_source_remove (mp->progress_grace_timer_id);
          mp->progress_grace_timer_id = 0;
        }

      if (mp->progress_timer_id != 0)
        {
          g_source_remove (mp->progress_timer_id);
          mp->progress_timer_id = 0;
        }

      gtk_widget_hide (mp->progress_label);
      gtk_widget_set_opacity (mp->progress_bar, 0.0);
      return;
    }

  active = mediaplayer_mpris_has_active_player (mp->mpris);
  status = mediaplayer_mpris_get_status (mp->mpris);
  length = mediaplayer_mpris_get_length (mp->mpris);
  has_length = active && length > 0;

  /* this runs both from mediaplayer_update_ui() and from the
   * once-a-second tick below while playing, so the grace state has to
   * live (and be checked) right here rather than at the call sites --
   * a poll landing mid-gap must see the same frozen state a change
   * notification would. */
  if (has_length)
    {
      mp->has_last_progress = TRUE;

      if (mp->progress_grace_timer_id != 0)
        {
          g_source_remove (mp->progress_grace_timer_id);
          mp->progress_grace_timer_id = 0;
        }
    }
  else if (mp->has_last_progress && mp->progress_grace_timer_id == 0)
    {
      mp->progress_grace_timer_id = g_timeout_add_seconds (MEDIAPLAYER_NO_PLAYER_GRACE_SECONDS,
                                                             (GSourceFunc) mediaplayer_progress_grace_timeout, mp);
    }

  if (!has_length && mp->has_last_progress)
    return; /* momentary gap: keep showing the last position/duration */

  if (has_length)
    {
      gint64 position = mediaplayer_mpris_get_position (mp->mpris);
      gdouble fraction;
      gchar *pos_str, *len_str, *text;

      position = CLAMP (position, 0, length);
      fraction = (gdouble) position / (gdouble) length;

      gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (mp->progress_bar), fraction);

      pos_str = format_time (position);
      len_str = format_time (length);
      text = g_strdup_printf ("%s / %s", pos_str, len_str);
      gtk_label_set_text (GTK_LABEL (mp->progress_label), text);
      g_free (pos_str);
      g_free (len_str);
      g_free (text);

      gtk_widget_show (mp->progress_label);
      gtk_widget_set_opacity (mp->progress_bar, 1.0);
    }
  else
    {
      /* the bar is the overlay's sizing (main) child: hiding it
       * outright would collapse the whole overlay -- and the text
       * floating on top of it -- to zero size, so fade it out
       * instead of hiding it. */
      gtk_widget_hide (mp->progress_label);
      gtk_widget_set_opacity (mp->progress_bar, 0.0);
    }

  /* only poll while actually playing and visible: a paused/stopped
   * position doesn't move, and there is no point querying D-Bus once
   * a second for indicators nobody can see. */
  if (has_length && status == MEDIAPLAYER_STATUS_PLAYING)
    {
      if (mp->progress_timer_id == 0)
        mp->progress_timer_id = g_timeout_add_seconds (1, (GSourceFunc) mediaplayer_progress_tick, mp);
    }
  else
    {
      if (mp->progress_timer_id != 0)
        {
          g_source_remove (mp->progress_timer_id);
          mp->progress_timer_id = 0;
        }
    }
}

static gboolean
mediaplayer_no_player_timeout (MediaplayerPlugin *mp)
{
  mp->no_player_timer_id = 0;
  mp->has_last_text = FALSE;
  g_clear_pointer (&mp->last_label_text, g_free);
  mediaplayer_update_ui (mp);
  return G_SOURCE_REMOVE;
}

void
mediaplayer_update_ui (MediaplayerPlugin *mp)
{
  gboolean active = mediaplayer_mpris_has_active_player (mp->mpris);
  MediaplayerStatus status = mediaplayer_mpris_get_status (mp->mpris);
  const gchar *title = mediaplayer_mpris_get_title (mp->mpris);
  const gchar *artist = mediaplayer_mpris_get_artist (mp->mpris);
  const gchar *album = mediaplayer_mpris_get_album (mp->mpris);
  const gchar *art_url = mediaplayer_mpris_get_art_url (mp->mpris);
  gboolean show_any = mp->show_title || mp->show_artist || mp->show_album;
  gboolean has_text_data = active && (title != NULL || artist != NULL || album != NULL);
  gchar *tooltip;
  gchar *label_text;

  /* text gets its own grace period before going blank on a momentary
   * gap (e.g. the brief moment between tracks while Previous/Next is
   * still being processed); art and progress track the same kind of
   * gap independently, inside their own update functions below. */

  if (has_text_data)
    {
      mp->has_last_text = TRUE;

      if (mp->no_player_timer_id != 0)
        {
          g_source_remove (mp->no_player_timer_id);
          mp->no_player_timer_id = 0;
        }
    }
  else if (mp->has_last_text && mp->no_player_timer_id == 0)
    {
      mp->no_player_timer_id = g_timeout_add_seconds (MEDIAPLAYER_NO_PLAYER_GRACE_SECONDS,
                                                        (GSourceFunc) mediaplayer_no_player_timeout, mp);
    }

  mediaplayer_update_art (mp, active, art_url);
  mediaplayer_update_progress (mp);

  gtk_widget_set_sensitive (mp->btn_play_pause, active);
  gtk_widget_set_sensitive (mp->btn_previous, active && mediaplayer_mpris_can_go_previous (mp->mpris));
  gtk_widget_set_sensitive (mp->btn_next, active && mediaplayer_mpris_can_go_next (mp->mpris));

  gtk_image_set_from_icon_name (GTK_IMAGE (mp->img_play_pause),
                                 status == MEDIAPLAYER_STATUS_PLAYING ? "media-playback-pause-symbolic"
                                                                       : "media-playback-start-symbolic",
                                 GTK_ICON_SIZE_SMALL_TOOLBAR);

  if (show_any)
    {
      if (has_text_data)
        {
          GPtrArray *parts = g_ptr_array_new ();

          if (mp->show_artist && artist != NULL)
            g_ptr_array_add (parts, (gpointer) artist);
          if (mp->show_title && title != NULL)
            g_ptr_array_add (parts, (gpointer) title);
          if (mp->show_album && album != NULL)
            g_ptr_array_add (parts, (gpointer) album);

          if (parts->len > 0)
            {
              g_ptr_array_add (parts, NULL);
              label_text = g_strjoinv (" \xe2\x80\x93 ", (gchar **) parts->pdata);

              g_free (mp->last_label_text);
              mp->last_label_text = g_strdup (label_text);
            }
          else
            {
              label_text = g_strdup ("No player");
            }

          g_ptr_array_free (parts, TRUE);
        }
      else if (mp->last_label_text != NULL)
        {
          /* momentary gap with no metadata (e.g. the brief moment
           * between tracks): keep the previous text on screen for a
           * short grace period instead of immediately flashing
           * "No player". */
          label_text = g_strdup (mp->last_label_text);
        }
      else
        {
          label_text = g_strdup ("No player");
        }

      gtk_label_set_text (GTK_LABEL (mp->label), label_text);
      gtk_widget_set_visible (mp->label, TRUE);
      g_free (label_text);
    }
  else
    {
      gtk_widget_set_visible (mp->label, FALSE);
    }

  tooltip = build_tooltip (mp);
  gtk_widget_set_tooltip_text (mp->ebox, tooltip);
  g_free (tooltip);

}

static void
mpris_changed_cb (MediaplayerMpris *mpris, MediaplayerPlugin *mp)
{
  mediaplayer_update_ui (mp);
}

static void
previous_clicked_cb (GtkButton *button, MediaplayerPlugin *mp)
{
  mediaplayer_mpris_previous (mp->mpris);
}

static void
play_pause_clicked_cb (GtkButton *button, MediaplayerPlugin *mp)
{
  mediaplayer_mpris_play_pause (mp->mpris);
}

static void
next_clicked_cb (GtkButton *button, MediaplayerPlugin *mp)
{
  mediaplayer_mpris_next (mp->mpris);
}

static gboolean
info_button_press_cb (GtkWidget *widget, GdkEventButton *event, MediaplayerPlugin *mp)
{
  if (event->button != GDK_BUTTON_PRIMARY)
    return GDK_EVENT_PROPAGATE;

  mediaplayer_mpris_play_pause (mp->mpris);

  return GDK_EVENT_STOP;
}

static void
info_ebox_realize_cb (GtkWidget *widget, gpointer user_data)
{
  GdkWindow *window = gtk_widget_get_window (widget);
  GdkCursor *cursor;

  if (window == NULL)
    return;

  cursor = gdk_cursor_new_from_name (gtk_widget_get_display (widget), "pointer");
  if (cursor != NULL)
    {
      gdk_window_set_cursor (window, cursor);
      g_object_unref (cursor);
    }
}

static gboolean
art_query_tooltip_cb (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode,
                       GtkTooltip *tooltip, MediaplayerPlugin *mp)
{
  const gchar *art_url = mediaplayer_mpris_get_art_url (mp->mpris);
  gchar *path;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (art_url == NULL)
    return FALSE;

  path = g_filename_from_uri (art_url, NULL, &error);
  if (path == NULL)
    {
      g_clear_error (&error);
      return FALSE;
    }

  /* re-decode from the source rather than reusing the small panel
   * pixbuf, so the preview is as sharp as the source art allows. */
  pixbuf = gdk_pixbuf_new_from_file_at_scale (path, MEDIAPLAYER_ART_PREVIEW_SIZE,
                                               MEDIAPLAYER_ART_PREVIEW_SIZE, TRUE, &error);
  g_free (path);
  g_clear_error (&error);

  if (pixbuf == NULL)
    return FALSE;

  gtk_tooltip_set_icon (tooltip, pixbuf);
  g_object_unref (pixbuf);

  return TRUE;
}

static GtkWidget *
create_button (const gchar *icon_name, GCallback callback, MediaplayerPlugin *mp, GtkWidget **image_out)
{
  GtkWidget *button;
  GtkWidget *image;

  button = gtk_button_new ();
  gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
  gtk_widget_set_focus_on_click (button, FALSE);

  image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_container_add (GTK_CONTAINER (button), image);

  g_signal_connect (button, "clicked", callback, mp);

  if (image_out != NULL)
    *image_out = image;

  return button;
}

static void
mediaplayer_update_bar_contrast (MediaplayerPlugin *mp)
{
  /* using the theme's own accent color for the fill as-is turned out
   * to be too low-contrast in both directions: too dark against a
   * dark panel, and (per the theme actually in use here) still not
   * enough against the label's own near-black text despite the
   * text's halo. Rather than falling back to a fixed color (losing
   * the theme's own hue entirely), look up its accent color and
   * lighten it -- keeping it recognizably the theme's own blue/green/
   * whatever, just brighter -- on a light panel; a dark panel instead
   * gets a plain, guaranteed-visible white, since lightening further
   * wouldn't help there and white is already about as bright as it
   * gets. The trough itself (the progress row's own background,
   * spanning its full width regardless of playback position) stays
   * fully transparent either way, so the row reads as plain panel
   * background everywhere the fill hasn't reached yet -- only the
   * fill is "the bar". The text still needs a shadow/halo to stay
   * legible wherever it crosses that fill, in the *opposite* shade of
   * its own color. */
  GtkStyleContext *label_context;
  GdkRGBA text_color;
  GdkRGBA accent_color = { 0.20, 0.51, 0.85, 1.0 }; /* fallback if the theme defines no accent color */
  gdouble luminance;
  gint text_shadow_shade;
  gboolean dark_panel;
  gchar *fill_override_css;
  gchar *bar_css;
  gchar *text_css;

  label_context = gtk_widget_get_style_context (mp->label);
  gtk_style_context_get_color (label_context, gtk_style_context_get_state (label_context), &text_color);
  gtk_style_context_lookup_color (label_context, "theme_selected_bg_color", &accent_color);

  luminance = 0.2126 * text_color.red + 0.7152 * text_color.green + 0.0722 * text_color.blue;
  text_shadow_shade = luminance > 0.5 ? 0 : 255;
  dark_panel = luminance > 0.5;

  if (dark_panel)
    {
      fill_override_css = g_strdup ("progressbar > trough > progress { background-color: rgba(255,255,255,0.75); }");
    }
  else
    {
      /* blend the accent color a third of the way toward white, to
       * lift it clear of dark label text without washing out its
       * hue. */
      const gdouble lighten = 0.35;
      gint r = (gint) CLAMP ((accent_color.red   * (1.0 - lighten) + lighten) * 255.0, 0, 255);
      gint g = (gint) CLAMP ((accent_color.green * (1.0 - lighten) + lighten) * 255.0, 0, 255);
      gint b = (gint) CLAMP ((accent_color.blue  * (1.0 - lighten) + lighten) * 255.0, 0, 255);

      fill_override_css = g_strdup_printf (
        "progressbar > trough > progress { background-color: rgb(%d,%d,%d); }", r, g, b);
    }

  bar_css = g_strdup_printf (
    "progressbar, progressbar > trough, progressbar > trough > progress {"
    "  min-height: %dpx;"
    "  padding: 0;"
    "  margin: 0;"
    "  border-radius: 4px;"
    "  background-image: none;"
    "}"
    "progressbar, progressbar > trough {"
    "  background-color: transparent;"
    "}"
    "%s",
    MEDIAPLAYER_PROGRESS_BAR_HEIGHT,
    fill_override_css);

  g_free (fill_override_css);

  text_css = g_strdup_printf (
    "label {"
    "  text-shadow: 0 0 3px rgba(%d,%d,%d,0.9), 0 0 3px rgba(%d,%d,%d,0.9);"
    "}",
    text_shadow_shade, text_shadow_shade, text_shadow_shade,
    text_shadow_shade, text_shadow_shade, text_shadow_shade);

  if (mp->bar_css_provider != NULL)
    {
      gtk_style_context_remove_provider (gtk_widget_get_style_context (mp->progress_bar),
                                          GTK_STYLE_PROVIDER (mp->bar_css_provider));
      g_object_unref (mp->bar_css_provider);
    }

  mp->bar_css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (mp->bar_css_provider, bar_css, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (mp->progress_bar),
                                   GTK_STYLE_PROVIDER (mp->bar_css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  if (mp->text_css_provider != NULL)
    {
      gtk_style_context_remove_provider (gtk_widget_get_style_context (mp->label),
                                          GTK_STYLE_PROVIDER (mp->text_css_provider));
      gtk_style_context_remove_provider (gtk_widget_get_style_context (mp->progress_label),
                                          GTK_STYLE_PROVIDER (mp->text_css_provider));
      g_object_unref (mp->text_css_provider);
    }

  mp->text_css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (mp->text_css_provider, text_css, -1, NULL);
  gtk_style_context_add_provider (gtk_widget_get_style_context (mp->label),
                                   GTK_STYLE_PROVIDER (mp->text_css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_style_context_add_provider (gtk_widget_get_style_context (mp->progress_label),
                                   GTK_STYLE_PROVIDER (mp->text_css_provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  g_free (bar_css);
  g_free (text_css);
}

static void
mediaplayer_label_style_updated_cb (GtkWidget *widget, MediaplayerPlugin *mp)
{
  mediaplayer_update_bar_contrast (mp);
}

static void
mediaplayer_orientation_changed (XfcePanelPlugin *plugin, GtkOrientation orientation, MediaplayerPlugin *mp)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (mp->box), orientation);
}

static void
mediaplayer_update_orientation_support (MediaplayerPlugin *mp)
{
  /* XFCE_PANEL_PLUGIN_MODE_VERTICAL is a rotated vertical panel where
   * plugins are expected to lay themselves out top-to-bottom in a
   * narrow column; this plugin's row of icons, art and text has no
   * vertical layout, so rather than render squashed/clipped, swap it
   * out for a small warning icon explaining why. Deskbar mode (also a
   * vertical panel, but with horizontally-laid-out plugins) is fine
   * and left alone. */
  gboolean supported = xfce_panel_plugin_get_mode (mp->plugin) != XFCE_PANEL_PLUGIN_MODE_VERTICAL;

  gtk_widget_set_visible (mp->box, supported);
  gtk_widget_set_visible (mp->unsupported_icon, !supported);
}

static void
mediaplayer_mode_changed (XfcePanelPlugin *plugin, XfcePanelPluginMode mode, MediaplayerPlugin *mp)
{
  mediaplayer_update_orientation_support (mp);
}

static gboolean
mediaplayer_size_changed (XfcePanelPlugin *plugin, gint size, MediaplayerPlugin *mp)
{
  gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, -1);
  mediaplayer_update_ui (mp);
  return TRUE;
}

static void
mediaplayer_configure_plugin (XfcePanelPlugin *plugin, MediaplayerPlugin *mp)
{
  mediaplayer_dialogs_show (plugin, mp);
}

static void
mediaplayer_free_data (XfcePanelPlugin *plugin, MediaplayerPlugin *mp)
{
  if (mp->progress_timer_id != 0)
    g_source_remove (mp->progress_timer_id);

  if (mp->no_player_timer_id != 0)
    g_source_remove (mp->no_player_timer_id);

  if (mp->art_grace_timer_id != 0)
    g_source_remove (mp->art_grace_timer_id);

  if (mp->progress_grace_timer_id != 0)
    g_source_remove (mp->progress_grace_timer_id);

  if (mp->mpris != NULL)
    g_object_unref (mp->mpris);

  g_clear_object (&mp->bar_css_provider);
  g_clear_object (&mp->text_css_provider);

  g_free (mp->art_cache_url);
  g_free (mp->last_label_text);
  g_free (mp);
}

static void
mediaplayer_construct (XfcePanelPlugin *plugin)
{
  MediaplayerPlugin *mp;

  mp = g_new0 (MediaplayerPlugin, 1);
  mp->plugin = plugin;
  mp->mpris = mediaplayer_mpris_new ();

  mp->ebox = gtk_event_box_new ();
  gtk_widget_show (mp->ebox);

  /* holds either the normal UI (mp->box) or, in an unsupported
   * vertical panel, a small warning icon in its place -- see
   * mediaplayer_update_orientation_support(). */
  mp->content_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (mp->content_box);
  gtk_container_add (GTK_CONTAINER (mp->ebox), mp->content_box);

  mp->unsupported_icon = gtk_image_new_from_icon_name ("dialog-warning-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_widget_set_tooltip_text (mp->unsupported_icon,
                                "Media Player doesn't support vertical panels.\n"
                                "Switch the panel to Deskbar mode, or use a horizontal panel.");
  gtk_widget_set_no_show_all (mp->unsupported_icon, TRUE);
  gtk_box_pack_start (GTK_BOX (mp->content_box), mp->unsupported_icon, FALSE, FALSE, 0);

  mp->box = gtk_box_new (xfce_panel_plugin_get_orientation (plugin) == GTK_ORIENTATION_HORIZONTAL
                            ? GTK_ORIENTATION_HORIZONTAL
                            : GTK_ORIENTATION_VERTICAL,
                          2);
  gtk_widget_set_halign (mp->box, GTK_ALIGN_CENTER);
  gtk_widget_show (mp->box);
  gtk_box_pack_start (GTK_BOX (mp->content_box), mp->box, TRUE, TRUE, 0);

  mp->btn_previous = create_button ("media-skip-backward-symbolic", G_CALLBACK (previous_clicked_cb), mp, NULL);
  mp->btn_play_pause = create_button ("media-playback-start-symbolic", G_CALLBACK (play_pause_clicked_cb), mp, &mp->img_play_pause);
  mp->btn_next = create_button ("media-skip-forward-symbolic", G_CALLBACK (next_clicked_cb), mp, NULL);

  gtk_widget_show_all (mp->btn_previous);
  gtk_widget_show_all (mp->btn_play_pause);
  gtk_widget_show_all (mp->btn_next);

  gtk_box_pack_start (GTK_BOX (mp->box), mp->btn_previous, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (mp->box), mp->btn_play_pause, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (mp->box), mp->btn_next, FALSE, FALSE, 0);

  mp->img_art = gtk_image_new ();
  gtk_widget_set_no_show_all (mp->img_art, TRUE);
  gtk_box_pack_start (GTK_BOX (mp->box), mp->img_art, FALSE, FALSE, 0);

  /* an enlarged preview of the current art, shown as a hover tooltip
   * on the small panel-sized image. */
  gtk_widget_set_has_tooltip (mp->img_art, TRUE);
  g_signal_connect (mp->img_art, "query-tooltip", G_CALLBACK (art_query_tooltip_cb), mp);

  /* the progress bar sits as the overlay's base child (a background
   * wash for the row), with the label/time row floating on top of it
   * -- GtkLabel has no background of its own, so the text reads
   * directly against the bar underneath. */
  mp->text_box = gtk_overlay_new ();
  gtk_widget_set_valign (mp->text_box, GTK_ALIGN_CENTER);
  gtk_widget_show (mp->text_box);
  gtk_box_pack_start (GTK_BOX (mp->box), mp->text_box, FALSE, FALSE, 4);

  /* always shown (as the overlay's sizing child) -- update_progress()
   * toggles its opacity, not its visibility, to turn it "off". */
  mp->progress_bar = gtk_progress_bar_new ();
  gtk_widget_show (mp->progress_bar);
  gtk_container_add (GTK_CONTAINER (mp->text_box), mp->progress_bar);

  mp->info_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_widget_set_halign (mp->info_row, GTK_ALIGN_FILL);
  gtk_widget_set_valign (mp->info_row, GTK_ALIGN_FILL);
  gtk_widget_show (mp->info_row);

  /* wrapped in its own event box (rather than reusing mp->ebox) so
   * clicking the text/time toggles playback without the whole
   * plugin -- buttons and art included -- reacting to a left click. */
  mp->info_ebox = gtk_event_box_new ();
  gtk_widget_set_halign (mp->info_ebox, GTK_ALIGN_FILL);
  gtk_widget_set_valign (mp->info_ebox, GTK_ALIGN_FILL);
  gtk_widget_show (mp->info_ebox);
  gtk_container_add (GTK_CONTAINER (mp->info_ebox), mp->info_row);
  g_signal_connect (mp->info_ebox, "button-press-event", G_CALLBACK (info_button_press_cb), mp);
  g_signal_connect (mp->info_ebox, "realize", G_CALLBACK (info_ebox_realize_cb), NULL);
  gtk_overlay_add_overlay (GTK_OVERLAY (mp->text_box), mp->info_ebox);

  mp->label = gtk_label_new (NULL);
  gtk_label_set_ellipsize (GTK_LABEL (mp->label), PANGO_ELLIPSIZE_END);
  gtk_widget_show (mp->label);
  gtk_box_pack_start (GTK_BOX (mp->info_row), mp->label, TRUE, TRUE, 4);

  mp->progress_label = gtk_label_new (NULL);
  gtk_label_set_width_chars (GTK_LABEL (mp->progress_label), MEDIAPLAYER_PROGRESS_TIME_WIDTH_CHARS);
  gtk_label_set_xalign (GTK_LABEL (mp->progress_label), 1.0);
  gtk_widget_set_no_show_all (mp->progress_label, TRUE);
  gtk_box_pack_end (GTK_BOX (mp->info_row), mp->progress_label, FALSE, FALSE, 4);

  mediaplayer_update_bar_contrast (mp);
  g_signal_connect (mp->label, "style-updated", G_CALLBACK (mediaplayer_label_style_updated_cb), mp);

  gtk_container_add (GTK_CONTAINER (plugin), mp->ebox);

  xfce_panel_plugin_add_action_widget (plugin, mp->btn_previous);
  xfce_panel_plugin_add_action_widget (plugin, mp->btn_play_pause);
  xfce_panel_plugin_add_action_widget (plugin, mp->btn_next);

  mediaplayer_load_settings (mp);

  g_signal_connect (mp->mpris, "changed", G_CALLBACK (mpris_changed_cb), mp);

  g_signal_connect (plugin, "free-data", G_CALLBACK (mediaplayer_free_data), mp);
  g_signal_connect (plugin, "save", G_CALLBACK (mediaplayer_save), mp);
  g_signal_connect (plugin, "size-changed", G_CALLBACK (mediaplayer_size_changed), mp);
  g_signal_connect (plugin, "orientation-changed", G_CALLBACK (mediaplayer_orientation_changed), mp);
  g_signal_connect (plugin, "mode-changed", G_CALLBACK (mediaplayer_mode_changed), mp);
  g_signal_connect (plugin, "configure-plugin", G_CALLBACK (mediaplayer_configure_plugin), mp);

  /* expand steals a share of the flanking separators' leftover
   * space, which visually eats into their "push everything to
   * center" job; it was needed while the plugin's width tracked the
   * current track's text length, but the bar/text column now has a
   * fixed width driven by the label-width preference (set explicitly
   * on progress_bar, the overlay's sizing child), so the panel's
   * normal one-time layout pass is enough without it. */

  xfce_panel_plugin_menu_show_configure (plugin);

  mediaplayer_update_orientation_support (mp);
  mediaplayer_update_ui (mp);
}

XFCE_PANEL_PLUGIN_REGISTER (mediaplayer_construct);
