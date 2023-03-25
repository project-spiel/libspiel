
#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SPIEL_TYPE_UTTERANCE (spiel_utterance_get_type ())

G_DECLARE_FINAL_TYPE (
    SpielUtterance, spiel_utterance, SPIEL, UTTERANCE, GObject)

SpielUtterance *spiel_utterance_new (const char *text);

G_END_DECLS
