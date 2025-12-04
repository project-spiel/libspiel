/* spiel-provider.h
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

G_BEGIN_DECLS

#define SPIEL_TYPE_PROVIDER (spiel_provider_get_type ())

G_DECLARE_FINAL_TYPE (SpielProvider, spiel_provider, SPIEL, PROVIDER, GObject)

const char *spiel_provider_get_name (SpielProvider *self);

const char *spiel_provider_get_well_known_name (SpielProvider *self);

const char *spiel_provider_get_identifier (SpielProvider *self);

GListModel *spiel_provider_get_voices (SpielProvider *self);

G_END_DECLS
