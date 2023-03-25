
#pragma once

#include <gio/gio.h>
#include <glib-object.h>

#include "spiel-utterance.h"

G_BEGIN_DECLS

#define SPIEL_TYPE_SPEAKER (spiel_speaker_get_type ())

G_DECLARE_FINAL_TYPE (SpielSpeaker, spiel_speaker, SPIEL, SPEAKER, GObject)

void spiel_speaker_new (GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data);

SpielSpeaker *spiel_speaker_new_finish (GAsyncResult *result, GError **error);

SpielSpeaker *spiel_speaker_new_sync (GCancellable *cancellable,
                                      GError **error);

void spiel_speaker_speak (SpielSpeaker *self, SpielUtterance *utterance);

void spiel_speaker_pause (SpielSpeaker *self);

void spiel_speaker_resume (SpielSpeaker *self);

void spiel_speaker_cancel (SpielSpeaker *self);

G_END_DECLS
