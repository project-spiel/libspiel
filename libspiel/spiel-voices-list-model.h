/* spiel-voices-list-model.h
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

#define SPIEL_TYPE_VOICES_LIST_MODEL (spiel_voices_list_model_get_type ())

G_DECLARE_FINAL_TYPE (SpielVoicesListModel,
                      spiel_voices_list_model,
                      SPIEL,
                      VOICES_LIST_MODEL,
                      GObject)

SpielVoicesListModel *spiel_voices_list_model_new (GListModel *providers);

G_END_DECLS
