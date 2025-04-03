/* spiel-provider-private.h
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

#include "spiel-provider.h"

typedef struct _SpielProviderProxy SpielProviderProxy;
typedef struct _SpielVoice SpielVoice;

SpielProvider *spiel_provider_new (void);

void spiel_provider_set_proxy (SpielProvider *self,
                               SpielProviderProxy *provider_proxy);

SpielVoice *spiel_provider_get_voice_by_id (SpielProvider *self,
                                            const char *voice_id);

void spiel_provider_set_is_activatable (SpielProvider *self,
                                        gboolean is_activatable);

gboolean spiel_provider_get_is_activatable (SpielProvider *self);

gint spiel_provider_compare (SpielProvider *self,
                             SpielProvider *other,
                             gpointer user_data);