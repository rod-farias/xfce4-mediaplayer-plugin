#ifndef __MEDIAPLAYER_MPRIS_H__
#define __MEDIAPLAYER_MPRIS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MEDIAPLAYER_TYPE_MPRIS (mediaplayer_mpris_get_type ())
G_DECLARE_FINAL_TYPE (MediaplayerMpris, mediaplayer_mpris, MEDIAPLAYER, MPRIS, GObject)

typedef enum
{
  MEDIAPLAYER_STATUS_STOPPED,
  MEDIAPLAYER_STATUS_PAUSED,
  MEDIAPLAYER_STATUS_PLAYING,
} MediaplayerStatus;

MediaplayerMpris *mediaplayer_mpris_new (void);

void mediaplayer_mpris_set_preferred_player (MediaplayerMpris *mpris,
                                              const gchar      *bus_name);

/* preferred player, as a bus name without any ".instance<pid>" suffix */
const gchar *mediaplayer_mpris_get_preferred_player (MediaplayerMpris *mpris);

/* @bus_name with any MPRIS ".instance<pid>" suffix removed, so it stays
 * the same across player restarts. Free with g_free(). */
gchar *mediaplayer_mpris_player_base_name (const gchar *bus_name);

/* the player's human-readable Identity (org.mpris.MediaPlayer2), or
 * NULL if it does not provide one. Free with g_free(). */
gchar *mediaplayer_mpris_get_player_identity (MediaplayerMpris *mpris,
                                              const gchar      *bus_name);

/* Names (well-known org.mpris.MediaPlayer2.* bus names) of players
 * currently available on the session bus. Free the returned list
 * (and its contents) with g_list_free_full (list, g_free). */
GList *mediaplayer_mpris_list_players (MediaplayerMpris *mpris);

gboolean mediaplayer_mpris_has_active_player (MediaplayerMpris *mpris);

const gchar      *mediaplayer_mpris_get_title (MediaplayerMpris *mpris);
const gchar      *mediaplayer_mpris_get_artist (MediaplayerMpris *mpris);
const gchar      *mediaplayer_mpris_get_album (MediaplayerMpris *mpris);
const gchar      *mediaplayer_mpris_get_art_url (MediaplayerMpris *mpris);
MediaplayerStatus  mediaplayer_mpris_get_status (MediaplayerMpris *mpris);
gboolean           mediaplayer_mpris_can_go_next (MediaplayerMpris *mpris);
gboolean           mediaplayer_mpris_can_go_previous (MediaplayerMpris *mpris);

/* track length, in microseconds, from the current metadata (0 if unknown) */
gint64 mediaplayer_mpris_get_length (MediaplayerMpris *mpris);

/* current playback position, in microseconds, queried live from the
 * active player over D-Bus (-1 if unavailable). Unlike the other
 * getters this is not cached: MPRIS players do not reliably notify
 * position changes, so callers are expected to poll this while
 * playing (e.g. once a second). */
gint64 mediaplayer_mpris_get_position (MediaplayerMpris *mpris);

void mediaplayer_mpris_play_pause (MediaplayerMpris *mpris);
void mediaplayer_mpris_next (MediaplayerMpris *mpris);
void mediaplayer_mpris_previous (MediaplayerMpris *mpris);

G_END_DECLS

#endif /* !__MEDIAPLAYER_MPRIS_H__ */
