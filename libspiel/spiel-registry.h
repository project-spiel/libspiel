/* spiel-registry.h
 *
 * Copyright (C) 2023 Eitan Isaacson <eitan@monotonous.org>
 *
 * This file is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>

#include "spiel-voice.h"
#include "spieldbusgenerated.h"

G_BEGIN_DECLS

#define SPIEL_TYPE_REGISTRY (spiel_registry_get_type ())

G_DECLARE_FINAL_TYPE (SpielRegistry, spiel_registry, SPIEL, REGISTRY, GObject)

void spiel_registry_get (GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data);

SpielRegistry *spiel_registry_get_finish (GAsyncResult *result, GError **error);

SpielRegistry *spiel_registry_get_sync (GCancellable *cancellable,
                                        GError **error);

SpielProvider *spiel_registry_get_provider_for_voice (SpielRegistry *self,
                                                      SpielVoice *voice);

SpielVoice *spiel_registry_get_voice_for_utterance (SpielRegistry *self,
                                                    SpielUtterance *utterance);

GListStore *spiel_registry_get_voices (SpielRegistry *self);

G_END_DECLS
