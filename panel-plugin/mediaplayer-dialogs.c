#include <string.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "mediaplayer-dialogs.h"

typedef struct
{
  MediaplayerPlugin *mp;
  GtkWidget          *check_title;
  GtkWidget          *check_artist;
  GtkWidget          *check_album;
  GtkWidget          *position_label;
  GtkWidget          *position_combo;
} DialogWidgets;

static gchar *
friendly_player_name (const gchar *bus_name)
{
  const gchar *short_name = bus_name + strlen ("org.mpris.MediaPlayer2.");

  if (short_name[0] == '\0')
    return g_strdup (bus_name);

  return g_strdup_printf ("%c%s", g_ascii_toupper (short_name[0]), short_name + 1);
}

static void
preferred_player_changed_cb (GtkComboBox *combo, DialogWidgets *dw)
{
  const gchar *id = gtk_combo_box_get_active_id (combo);

  mediaplayer_mpris_set_preferred_player (dw->mp->mpris, (id != NULL && *id != '\0') ? id : NULL);
}

static void
update_position_sensitivity (DialogWidgets *dw)
{
  gboolean any_text = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dw->check_title)) ||
                       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dw->check_artist)) ||
                       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dw->check_album));

  gtk_widget_set_sensitive (dw->position_label, any_text);
  gtk_widget_set_sensitive (dw->position_combo, any_text);
}

static void
show_title_toggled_cb (GtkToggleButton *toggle, DialogWidgets *dw)
{
  dw->mp->show_title = gtk_toggle_button_get_active (toggle);
  mediaplayer_update_ui (dw->mp);
  update_position_sensitivity (dw);
}

static void
show_artist_toggled_cb (GtkToggleButton *toggle, DialogWidgets *dw)
{
  dw->mp->show_artist = gtk_toggle_button_get_active (toggle);
  mediaplayer_update_ui (dw->mp);
  update_position_sensitivity (dw);
}

static void
show_album_toggled_cb (GtkToggleButton *toggle, DialogWidgets *dw)
{
  dw->mp->show_album = gtk_toggle_button_get_active (toggle);
  mediaplayer_update_ui (dw->mp);
  update_position_sensitivity (dw);
}

static void
show_album_art_toggled_cb (GtkToggleButton *toggle, DialogWidgets *dw)
{
  dw->mp->show_album_art = gtk_toggle_button_get_active (toggle);
  mediaplayer_update_ui (dw->mp);
}

static void
show_progress_toggled_cb (GtkToggleButton *toggle, DialogWidgets *dw)
{
  dw->mp->show_progress = gtk_toggle_button_get_active (toggle);
  mediaplayer_update_ui (dw->mp);
}

static void
label_width_changed_cb (GtkSpinButton *spin, DialogWidgets *dw)
{
  dw->mp->label_width_chars = gtk_spin_button_get_value_as_int (spin);
  mediaplayer_apply_label_width (dw->mp);
  mediaplayer_update_ui (dw->mp);
}

static void
controls_position_changed_cb (GtkComboBox *combo, DialogWidgets *dw)
{
  dw->mp->controls_right = (g_strcmp0 (gtk_combo_box_get_active_id (combo), "right") == 0);
  mediaplayer_apply_controls_position (dw->mp);
}

static void
dialog_response_cb (GtkWidget *dialog, gint response, DialogWidgets *dw)
{
  gtk_widget_destroy (dialog);
  xfce_panel_plugin_unblock_menu (dw->mp->plugin);
  mediaplayer_save (dw->mp->plugin, dw->mp);
}

void
mediaplayer_dialogs_show (XfcePanelPlugin *plugin, MediaplayerPlugin *mp)
{
  GtkWidget *dialog;
  GtkWidget *content;
  GtkWidget *grid;
  GtkWidget *label;
  GtkWidget *combo;
  GtkWidget *width_label;
  GtkWidget *width_spin;
  GtkWidget *check_art;
  GtkWidget *check_progress;
  GList *players, *iter;
  const gchar *preferred;
  DialogWidgets *dw;

  xfce_panel_plugin_block_menu (plugin);

  dw = g_new0 (DialogWidgets, 1);
  dw->mp = mp;

  dialog = xfce_titled_dialog_new_with_mixed_buttons ("Media Player",
                                                        GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        "window-close", "Close", GTK_RESPONSE_CLOSE,
                                                        NULL);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "multimedia-player");
  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog), "Music playback preferences");

  g_object_set_data_full (G_OBJECT (dialog), "dialog-widgets", dw, g_free);

  content = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  grid = gtk_grid_new ();
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_container_set_border_width (GTK_CONTAINER (grid), 12);
  gtk_box_pack_start (GTK_BOX (content), grid, TRUE, TRUE, 0);

  label = gtk_label_new ("Preferred player:");
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), "", "Auto (currently playing)");

  players = mediaplayer_mpris_list_players (mp->mpris);
  for (iter = players; iter != NULL; iter = iter->next)
    {
      const gchar *bus_name = iter->data;
      gchar *display = friendly_player_name (bus_name);

      gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (combo), bus_name, display);
      g_free (display);
    }
  g_list_free_full (players, g_free);

  preferred = mediaplayer_mpris_get_preferred_player (mp->mpris);
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), preferred != NULL ? preferred : "");

  g_signal_connect (combo, "changed", G_CALLBACK (preferred_player_changed_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), combo, 1, 0, 1, 1);

  dw->check_title = gtk_check_button_new_with_label ("Show title");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dw->check_title), mp->show_title);
  g_signal_connect (dw->check_title, "toggled", G_CALLBACK (show_title_toggled_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), dw->check_title, 0, 1, 2, 1);

  dw->check_artist = gtk_check_button_new_with_label ("Show artist");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dw->check_artist), mp->show_artist);
  g_signal_connect (dw->check_artist, "toggled", G_CALLBACK (show_artist_toggled_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), dw->check_artist, 0, 2, 2, 1);

  dw->check_album = gtk_check_button_new_with_label ("Show album");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dw->check_album), mp->show_album);
  g_signal_connect (dw->check_album, "toggled", G_CALLBACK (show_album_toggled_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), dw->check_album, 0, 3, 2, 1);

  check_art = gtk_check_button_new_with_label ("Show album art (if the panel is tall enough)");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_art), mp->show_album_art);
  g_signal_connect (check_art, "toggled", G_CALLBACK (show_album_art_toggled_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), check_art, 0, 4, 2, 1);

  check_progress = gtk_check_button_new_with_label ("Show progress bar");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_progress), mp->show_progress);
  g_signal_connect (check_progress, "toggled", G_CALLBACK (show_progress_toggled_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), check_progress, 0, 5, 2, 1);

  width_label = gtk_label_new ("Label width (characters):");
  gtk_widget_set_halign (width_label, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid), width_label, 0, 6, 1, 1);

  width_spin = gtk_spin_button_new_with_range (MEDIAPLAYER_LABEL_WIDTH_MIN, MEDIAPLAYER_LABEL_WIDTH_MAX, 1);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (width_spin), mp->label_width_chars);
  g_signal_connect (width_spin, "value-changed", G_CALLBACK (label_width_changed_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), width_spin, 1, 6, 1, 1);

  dw->position_label = gtk_label_new ("Controls position:");
  gtk_widget_set_halign (dw->position_label, GTK_ALIGN_START);
  gtk_grid_attach (GTK_GRID (grid), dw->position_label, 0, 7, 1, 1);

  dw->position_combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (dw->position_combo), "left", "Left of text");
  gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (dw->position_combo), "right", "Right of text");
  gtk_combo_box_set_active_id (GTK_COMBO_BOX (dw->position_combo), mp->controls_right ? "right" : "left");
  g_signal_connect (dw->position_combo, "changed", G_CALLBACK (controls_position_changed_cb), dw);
  gtk_grid_attach (GTK_GRID (grid), dw->position_combo, 1, 7, 1, 1);

  update_position_sensitivity (dw);

  g_signal_connect (dialog, "response", G_CALLBACK (dialog_response_cb), dw);

  gtk_widget_show_all (dialog);
}
