/* spiel-utterance.h
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

#include "spiel-voice.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define SPIEL_TYPE_UTTERANCE (spiel_utterance_get_type ())

G_DECLARE_FINAL_TYPE (
    SpielUtterance, spiel_utterance, SPIEL, UTTERANCE, GObject)

SpielUtterance *spiel_utterance_new (const char *text);

const char *spiel_utterance_get_text (SpielUtterance *self);

void spiel_utterance_set_text (SpielUtterance *self, const char *text);

double spiel_utterance_get_pitch (SpielUtterance *self);

void spiel_utterance_set_pitch (SpielUtterance *self, double pitch);

double spiel_utterance_get_rate (SpielUtterance *self);

void spiel_utterance_set_rate (SpielUtterance *self, double rate);

double spiel_utterance_get_volume (SpielUtterance *self);

void spiel_utterance_set_volume (SpielUtterance *self, double volume);

SpielVoice *spiel_utterance_get_voice (SpielUtterance *self);

void spiel_utterance_set_voice (SpielUtterance *self, SpielVoice *voice);

const char *spiel_utterance_get_language (SpielUtterance *self);

void spiel_utterance_set_language (SpielUtterance *self, const char *language);

void spiel_utterance_set_is_ssml (SpielUtterance *self, gboolean is_ssml);

gboolean spiel_utterance_get_is_ssml (SpielUtterance *self);

G_END_DECLS
