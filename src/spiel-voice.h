
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SPIEL_TYPE_VOICE (spiel_voice_get_type ())

G_DECLARE_FINAL_TYPE (SpielVoice, spiel_voice, SPIEL, VOICE, GObject)

SpielVoice *spiel_voice_new (const char *name,
                             const char *identifier,
                             const char **languages,
                             const char *provider_name);

const char *spiel_voice_get_identifier (SpielVoice *self);

const char *spiel_voice_get_provider_name (SpielVoice *self);

G_END_DECLS
