/* spiel-providers-list-model.h
 *
 * Copyright (C) 2026 Eitan Isaacson <eitan@monotonous.org>
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

typedef struct _SpielProvider SpielProvider;

G_BEGIN_DECLS

#define SPIEL_TYPE_PROVIDERS_LIST_MODEL (spiel_providers_list_model_get_type ())

G_DECLARE_FINAL_TYPE (SpielProvidersListModel,
                      spiel_providers_list_model,
                      SPIEL,
                      PROVIDERS_LIST_MODEL,
                      GObject)

void spiel_providers_list_model_new (gboolean use_portal,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data);

SpielProvidersListModel *
spiel_providers_list_model_new_finish (GAsyncResult *result, GError **error);

SpielProvidersListModel *
spiel_providers_list_model_new_sync (gboolean use_portal);

SpielProvider *spiel_providers_list_model_get_by_name (
    SpielProvidersListModel *self, const char *provider_name, guint *position);

G_END_DECLS
