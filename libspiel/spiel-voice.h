/* spiel-voice.h
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

G_BEGIN_DECLS

#define SPIEL_TYPE_VOICE (spiel_voice_get_type ())

G_DECLARE_FINAL_TYPE (SpielVoice, spiel_voice, SPIEL, VOICE, GObject)

const char *spiel_voice_get_name (SpielVoice *self);

const char *spiel_voice_get_identifier (SpielVoice *self);

const char *spiel_voice_get_provider_name (SpielVoice *self);

const char *spiel_voice_get_output_format (SpielVoice *self);

const char *const *spiel_voice_get_languages (SpielVoice *self);

guint spiel_voice_hash (SpielVoice *self);

gboolean spiel_voice_equal (SpielVoice *self, SpielVoice *other);

gint
spiel_voice_compare (SpielVoice *self, SpielVoice *other, gpointer user_data);

G_END_DECLS
