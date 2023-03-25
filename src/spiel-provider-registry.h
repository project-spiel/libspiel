
#pragma once

#include <glib-object.h>

#include "spiel-voice.h"
#include "spieldbusgenerated.h"

G_BEGIN_DECLS

#define SPIEL_TYPE_PROVIDER_REGISTRY (spiel_provider_registry_get_type ())

G_DECLARE_FINAL_TYPE (SpielProviderRegistry,
                      spiel_provider_registry,
                      SPIEL,
                      PROVIDER_REGISTRY,
                      GObject)

void spiel_provider_registry_get (GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data);

SpielProviderRegistry *spiel_provider_registry_get_finish (GAsyncResult *result,
                                                           GError **error);

SpielProviderRegistry *
spiel_provider_registry_get_sync (GCancellable *cancellable, GError **error);

SpielProvider *
spiel_provider_registry_get_provider_for_voice (SpielProviderRegistry *self,
                                                SpielVoice *voice);

SpielVoice *
spiel_provider_registry_get_voice_for_utterance (SpielProviderRegistry *self,
                                                 SpielUtterance *utterance);

GListStore *spiel_provider_registry_get_voices (SpielProviderRegistry *self);

G_END_DECLS
