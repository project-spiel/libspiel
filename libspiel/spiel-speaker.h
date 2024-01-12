/* spiel-speaker.h
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

GQuark spiel_error_quark (void);

/**
 * SPIEL_ERROR:
 *
 * Domain for `SpielSpeaker` errors.
 */
#define SPIEL_ERROR spiel_error_quark ()

/**
 * SpielError:
 * @SPIEL_ERROR_NO_PROVIDERS: No speech providers are available
 * @SPIEL_ERROR_PROVIDER_UNEXPECTEDLY_DIED: Speech provider unexpectedly
 * died
 * @SPIEL_ERROR_INTERNAL_PROVIDER_FAILURE: Internal error in speech
 * provider
 *
 * Error codes in the `SPIEL_ERROR` domain that can be emitted in the
 * `utterance-error` signal.
 */
typedef enum /*<underscore_name=spiel_error>*/
{
  SPIEL_ERROR_NO_PROVIDERS,
  SPIEL_ERROR_PROVIDER_UNEXPECTEDLY_DIED,
  SPIEL_ERROR_INTERNAL_PROVIDER_FAILURE,
} SpielError;

G_END_DECLS
