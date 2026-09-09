#include <string.h>
#include <gio/gio.h>

#include "mediaplayer-mpris.h"

#define MPRIS_PREFIX          "org.mpris.MediaPlayer2."
#define MPRIS_OBJECT_PATH     "/org/mpris/MediaPlayer2"
#define MPRIS_PLAYER_IFACE    "org.mpris.MediaPlayer2.Player"
#define DBUS_PROPERTIES_IFACE "org.freedesktop.DBus.Properties"

enum
{
  SIGNAL_CHANGED,
  N_SIGNALS,
};

static guint mpris_signals[N_SIGNALS] = { 0, };

struct _MediaplayerMpris
{
  GObject parent_instance;

  GDBusConnection *connection;
  guint            name_owner_subscription;
  guint            properties_subscription;

  GPtrArray       *known_players; /* array of owned gchar* bus names */

  gchar            *preferred_player;
  gchar            *active_player;

  gchar             *title;
  gchar             *artist;
  gchar             *album;
  gchar             *art_url;
  gint64             length;
  MediaplayerStatus  status;
  gboolean           can_go_next;
  gboolean           can_go_previous;
};

G_DEFINE_TYPE (MediaplayerMpris, mediaplayer_mpris, G_TYPE_OBJECT)

static void mediaplayer_mpris_select_active_player (MediaplayerMpris *mpris);
static void mediaplayer_mpris_refresh_properties (MediaplayerMpris *mpris);
static void mediaplayer_mpris_clear_track_info (MediaplayerMpris *mpris);
static void mediaplayer_mpris_subscribe_active_player (MediaplayerMpris *mpris);

static gboolean
bus_name_is_mpris_player (const gchar *name)
{
  return name != NULL && g_str_has_prefix (name, MPRIS_PREFIX);
}

static gint
find_player (GPtrArray *players, const gchar *name)
{
  guint i;

  for (i = 0; i < players->len; i++)
    {
      if (g_strcmp0 (g_ptr_array_index (players, i), name) == 0)
        return (gint) i;
    }

  return -1;
}

static void
name_owner_changed_cb (GDBusConnection *connection,
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
  MediaplayerMpris *mpris = MEDIAPLAYER_MPRIS (user_data);
  const gchar *name = NULL;
  const gchar *old_owner = NULL;
  const gchar *new_owner = NULL;

  g_variant_get (parameters, "(&s&s&s)", &name, &old_owner, &new_owner);

  if (!bus_name_is_mpris_player (name))
    return;

  if (new_owner != NULL && *new_owner != '\0')
    {
      /* player appeared */
      if (find_player (mpris->known_players, name) < 0)
        g_ptr_array_add (mpris->known_players, g_strdup (name));
    }
  else
    {
      /* player disappeared */
      gint idx = find_player (mpris->known_players, name);

      if (idx >= 0)
        g_ptr_array_remove_index (mpris->known_players, (guint) idx);
    }

  mediaplayer_mpris_select_active_player (mpris);
}

static void
properties_changed_cb (GDBusConnection *connection,
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
  MediaplayerMpris *mpris = MEDIAPLAYER_MPRIS (user_data);

  mediaplayer_mpris_refresh_properties (mpris);
  g_signal_emit (mpris, mpris_signals[SIGNAL_CHANGED], 0);
}

static void
mediaplayer_mpris_subscribe_active_player (MediaplayerMpris *mpris)
{
  if (mpris->properties_subscription != 0)
    {
      g_dbus_connection_signal_unsubscribe (mpris->connection, mpris->properties_subscription);
      mpris->properties_subscription = 0;
    }

  if (mpris->active_player == NULL)
    return;

  mpris->properties_subscription =
    g_dbus_connection_signal_subscribe (mpris->connection,
                                         mpris->active_player,
                                         DBUS_PROPERTIES_IFACE,
                                         "PropertiesChanged",
                                         MPRIS_OBJECT_PATH,
                                         NULL,
                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                         properties_changed_cb,
                                         mpris,
                                         NULL);
}

static MediaplayerStatus
parse_playback_status (const gchar *status_str)
{
  if (g_strcmp0 (status_str, "Playing") == 0)
    return MEDIAPLAYER_STATUS_PLAYING;
  if (g_strcmp0 (status_str, "Paused") == 0)
    return MEDIAPLAYER_STATUS_PAUSED;

  return MEDIAPLAYER_STATUS_STOPPED;
}

static void
apply_properties_variant (MediaplayerMpris *mpris, GVariant *props)
{
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  g_clear_pointer (&mpris->title, g_free);
  g_clear_pointer (&mpris->artist, g_free);
  g_clear_pointer (&mpris->album, g_free);
  g_clear_pointer (&mpris->art_url, g_free);
  mpris->length = 0;

  g_variant_iter_init (&iter, props);
  while (g_variant_iter_next (&iter, "{&sv}", &key, &value))
    {
      if (g_strcmp0 (key, "PlaybackStatus") == 0 && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
        {
          mpris->status = parse_playback_status (g_variant_get_string (value, NULL));
        }
      else if (g_strcmp0 (key, "CanGoNext") == 0 && g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        {
          mpris->can_go_next = g_variant_get_boolean (value);
        }
      else if (g_strcmp0 (key, "CanGoPrevious") == 0 && g_variant_is_of_type (value, G_VARIANT_TYPE_BOOLEAN))
        {
          mpris->can_go_previous = g_variant_get_boolean (value);
        }
      else if (g_strcmp0 (key, "Metadata") == 0 && g_variant_is_of_type (value, G_VARIANT_TYPE_VARDICT))
        {
          GVariant *v;

          v = g_variant_lookup_value (value, "xesam:title", G_VARIANT_TYPE_STRING);
          if (v != NULL)
            {
              mpris->title = g_variant_dup_string (v, NULL);
              g_variant_unref (v);
            }

          v = g_variant_lookup_value (value, "xesam:artist", NULL);
          if (v != NULL)
            {
              if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING_ARRAY) &&
                  g_variant_n_children (v) > 0)
                {
                  GVariant *first = g_variant_get_child_value (v, 0);
                  mpris->artist = g_variant_dup_string (first, NULL);
                  g_variant_unref (first);
                }
              g_variant_unref (v);
            }

          v = g_variant_lookup_value (value, "xesam:album", G_VARIANT_TYPE_STRING);
          if (v != NULL)
            {
              mpris->album = g_variant_dup_string (v, NULL);
              g_variant_unref (v);
            }

          v = g_variant_lookup_value (value, "mpris:artUrl", G_VARIANT_TYPE_STRING);
          if (v != NULL)
            {
              mpris->art_url = g_variant_dup_string (v, NULL);
              g_variant_unref (v);
            }

          v = g_variant_lookup_value (value, "mpris:length", G_VARIANT_TYPE_INT64);
          if (v != NULL)
            {
              mpris->length = g_variant_get_int64 (v);
              g_variant_unref (v);
            }
        }

      g_variant_unref (value);
    }
}

static void
mediaplayer_mpris_clear_track_info (MediaplayerMpris *mpris)
{
  g_clear_pointer (&mpris->title, g_free);
  g_clear_pointer (&mpris->artist, g_free);
  g_clear_pointer (&mpris->album, g_free);
  g_clear_pointer (&mpris->art_url, g_free);
  mpris->length = 0;
  mpris->status = MEDIAPLAYER_STATUS_STOPPED;
  mpris->can_go_next = FALSE;
  mpris->can_go_previous = FALSE;
}

static void
mediaplayer_mpris_refresh_properties (MediaplayerMpris *mpris)
{
  GVariant *reply;
  GError *error = NULL;

  if (mpris->active_player == NULL)
    {
      mediaplayer_mpris_clear_track_info (mpris);
      return;
    }

  reply = g_dbus_connection_call_sync (mpris->connection,
                                        mpris->active_player,
                                        MPRIS_OBJECT_PATH,
                                        DBUS_PROPERTIES_IFACE,
                                        "GetAll",
                                        g_variant_new ("(s)", MPRIS_PLAYER_IFACE),
                                        G_VARIANT_TYPE ("(a{sv})"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        1000,
                                        NULL,
                                        &error);

  if (reply == NULL)
    {
      g_debug ("mediaplayer: GetAll failed for %s: %s", mpris->active_player, error->message);
      g_clear_error (&error);
      mediaplayer_mpris_clear_track_info (mpris);
      return;
    }

  {
    GVariant *props = g_variant_get_child_value (reply, 0);
    apply_properties_variant (mpris, props);
    g_variant_unref (props);
  }

  g_variant_unref (reply);
}

static MediaplayerStatus
query_playback_status (MediaplayerMpris *mpris, const gchar *bus_name)
{
  GVariant *reply;
  MediaplayerStatus result = MEDIAPLAYER_STATUS_STOPPED;
  GError *error = NULL;

  reply = g_dbus_connection_call_sync (mpris->connection,
                                        bus_name,
                                        MPRIS_OBJECT_PATH,
                                        DBUS_PROPERTIES_IFACE,
                                        "Get",
                                        g_variant_new ("(ss)", MPRIS_PLAYER_IFACE, "PlaybackStatus"),
                                        G_VARIANT_TYPE ("(v)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        1000,
                                        NULL,
                                        &error);

  if (reply == NULL)
    {
      g_debug ("mediaplayer: could not query PlaybackStatus for %s: %s", bus_name, error->message);
      g_clear_error (&error);
      return result;
    }

  {
    GVariant *v, *inner;

    g_variant_get (reply, "(v)", &v);
    inner = v;
    if (g_variant_is_of_type (inner, G_VARIANT_TYPE_STRING))
      result = parse_playback_status (g_variant_get_string (inner, NULL));
    g_variant_unref (v);
  }

  g_variant_unref (reply);

  return result;
}

static void
mediaplayer_mpris_select_active_player (MediaplayerMpris *mpris)
{
  gchar *new_active = NULL;
  guint i;

  if (mpris->preferred_player != NULL &&
      find_player (mpris->known_players, mpris->preferred_player) >= 0)
    {
      new_active = g_strdup (mpris->preferred_player);
    }
  else
    {
      /* prefer a player that is currently playing */
      for (i = 0; i < mpris->known_players->len; i++)
        {
          const gchar *candidate = g_ptr_array_index (mpris->known_players, i);

          if (query_playback_status (mpris, candidate) == MEDIAPLAYER_STATUS_PLAYING)
            {
              new_active = g_strdup (candidate);
              break;
            }
        }

      if (new_active == NULL && mpris->known_players->len > 0)
        new_active = g_strdup (g_ptr_array_index (mpris->known_players, 0));
    }

  if (g_strcmp0 (new_active, mpris->active_player) != 0)
    {
      g_free (mpris->active_player);
      mpris->active_player = new_active;
      mediaplayer_mpris_subscribe_active_player (mpris);
    }
  else
    {
      g_free (new_active);
    }

  mediaplayer_mpris_refresh_properties (mpris);
  g_signal_emit (mpris, mpris_signals[SIGNAL_CHANGED], 0);
}

static void
mediaplayer_mpris_discover_players (MediaplayerMpris *mpris)
{
  GVariant *reply;
  GError *error = NULL;
  GVariantIter *iter;
  const gchar *name;

  g_ptr_array_set_size (mpris->known_players, 0);

  reply = g_dbus_connection_call_sync (mpris->connection,
                                        "org.freedesktop.DBus",
                                        "/org/freedesktop/DBus",
                                        "org.freedesktop.DBus",
                                        "ListNames",
                                        NULL,
                                        G_VARIANT_TYPE ("(as)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        1000,
                                        NULL,
                                        &error);

  if (reply == NULL)
    {
      g_warning ("mediaplayer: ListNames failed: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_variant_get (reply, "(as)", &iter);
  while (g_variant_iter_next (iter, "&s", &name))
    {
      if (bus_name_is_mpris_player (name))
        g_ptr_array_add (mpris->known_players, g_strdup (name));
    }
  g_variant_iter_free (iter);
  g_variant_unref (reply);
}

static void
mediaplayer_mpris_connect (MediaplayerMpris *mpris)
{
  GError *error = NULL;

  mpris->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  if (mpris->connection == NULL)
    {
      g_warning ("mediaplayer: unable to connect to session bus: %s", error->message);
      g_clear_error (&error);
      return;
    }

  mpris->name_owner_subscription =
    g_dbus_connection_signal_subscribe (mpris->connection,
                                         "org.freedesktop.DBus",
                                         "org.freedesktop.DBus",
                                         "NameOwnerChanged",
                                         "/org/freedesktop/DBus",
                                         NULL,
                                         G_DBUS_SIGNAL_FLAGS_NONE,
                                         name_owner_changed_cb,
                                         mpris,
                                         NULL);

  mediaplayer_mpris_discover_players (mpris);
  mediaplayer_mpris_select_active_player (mpris);
}

static void
mediaplayer_mpris_call_player_method (MediaplayerMpris *mpris, const gchar *method)
{
  if (mpris->connection == NULL || mpris->active_player == NULL)
    return;

  g_dbus_connection_call (mpris->connection,
                           mpris->active_player,
                           MPRIS_OBJECT_PATH,
                           MPRIS_PLAYER_IFACE,
                           method,
                           NULL,
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           NULL,
                           NULL);
}

void
mediaplayer_mpris_play_pause (MediaplayerMpris *mpris)
{
  g_return_if_fail (MEDIAPLAYER_IS_MPRIS (mpris));
  mediaplayer_mpris_call_player_method (mpris, "PlayPause");
}

void
mediaplayer_mpris_next (MediaplayerMpris *mpris)
{
  g_return_if_fail (MEDIAPLAYER_IS_MPRIS (mpris));
  mediaplayer_mpris_call_player_method (mpris, "Next");
}

void
mediaplayer_mpris_previous (MediaplayerMpris *mpris)
{
  g_return_if_fail (MEDIAPLAYER_IS_MPRIS (mpris));
  mediaplayer_mpris_call_player_method (mpris, "Previous");
}

void
mediaplayer_mpris_set_preferred_player (MediaplayerMpris *mpris, const gchar *bus_name)
{
  g_return_if_fail (MEDIAPLAYER_IS_MPRIS (mpris));

  g_free (mpris->preferred_player);
  mpris->preferred_player = (bus_name != NULL && *bus_name != '\0') ? g_strdup (bus_name) : NULL;

  mediaplayer_mpris_select_active_player (mpris);
}

const gchar *
mediaplayer_mpris_get_preferred_player (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), NULL);
  return mpris->preferred_player;
}

GList *
mediaplayer_mpris_list_players (MediaplayerMpris *mpris)
{
  GList *result = NULL;
  guint i;

  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), NULL);

  mediaplayer_mpris_discover_players (mpris);

  for (i = 0; i < mpris->known_players->len; i++)
    result = g_list_append (result, g_strdup (g_ptr_array_index (mpris->known_players, i)));

  return result;
}

gboolean
mediaplayer_mpris_has_active_player (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), FALSE);
  return mpris->active_player != NULL;
}

const gchar *
mediaplayer_mpris_get_title (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), NULL);
  return mpris->title;
}

const gchar *
mediaplayer_mpris_get_artist (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), NULL);
  return mpris->artist;
}

const gchar *
mediaplayer_mpris_get_album (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), NULL);
  return mpris->album;
}

const gchar *
mediaplayer_mpris_get_art_url (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), NULL);
  return mpris->art_url;
}

gint64
mediaplayer_mpris_get_length (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), 0);
  return mpris->length;
}

gint64
mediaplayer_mpris_get_position (MediaplayerMpris *mpris)
{
  GVariant *reply;
  gint64 result = -1;
  GError *error = NULL;

  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), -1);

  if (mpris->connection == NULL || mpris->active_player == NULL)
    return -1;

  reply = g_dbus_connection_call_sync (mpris->connection,
                                        mpris->active_player,
                                        MPRIS_OBJECT_PATH,
                                        DBUS_PROPERTIES_IFACE,
                                        "Get",
                                        g_variant_new ("(ss)", MPRIS_PLAYER_IFACE, "Position"),
                                        G_VARIANT_TYPE ("(v)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        1000,
                                        NULL,
                                        &error);

  if (reply == NULL)
    {
      g_debug ("mediaplayer: could not query Position for %s: %s", mpris->active_player, error->message);
      g_clear_error (&error);
      return -1;
    }

  {
    GVariant *v;

    g_variant_get (reply, "(v)", &v);
    if (g_variant_is_of_type (v, G_VARIANT_TYPE_INT64))
      result = g_variant_get_int64 (v);
    g_variant_unref (v);
  }

  g_variant_unref (reply);

  return result;
}

MediaplayerStatus
mediaplayer_mpris_get_status (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), MEDIAPLAYER_STATUS_STOPPED);
  return mpris->status;
}

gboolean
mediaplayer_mpris_can_go_next (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), FALSE);
  return mpris->can_go_next;
}

gboolean
mediaplayer_mpris_can_go_previous (MediaplayerMpris *mpris)
{
  g_return_val_if_fail (MEDIAPLAYER_IS_MPRIS (mpris), FALSE);
  return mpris->can_go_previous;
}

static void
mediaplayer_mpris_init (MediaplayerMpris *mpris)
{
  mpris->known_players = g_ptr_array_new_with_free_func (g_free);
  mpris->status = MEDIAPLAYER_STATUS_STOPPED;

  mediaplayer_mpris_connect (mpris);
}

static void
mediaplayer_mpris_finalize (GObject *object)
{
  MediaplayerMpris *mpris = MEDIAPLAYER_MPRIS (object);

  if (mpris->connection != NULL)
    {
      if (mpris->name_owner_subscription != 0)
        g_dbus_connection_signal_unsubscribe (mpris->connection, mpris->name_owner_subscription);
      if (mpris->properties_subscription != 0)
        g_dbus_connection_signal_unsubscribe (mpris->connection, mpris->properties_subscription);

      g_object_unref (mpris->connection);
    }

  g_ptr_array_free (mpris->known_players, TRUE);

  g_free (mpris->preferred_player);
  g_free (mpris->active_player);
  g_free (mpris->title);
  g_free (mpris->artist);
  g_free (mpris->album);
  g_free (mpris->art_url);

  G_OBJECT_CLASS (mediaplayer_mpris_parent_class)->finalize (object);
}

static void
mediaplayer_mpris_class_init (MediaplayerMprisClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = mediaplayer_mpris_finalize;

  mpris_signals[SIGNAL_CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
}

MediaplayerMpris *
mediaplayer_mpris_new (void)
{
  return g_object_new (MEDIAPLAYER_TYPE_MPRIS, NULL);
}
