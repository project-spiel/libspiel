/* spiel-collect-providers.h
 *
 * Copyright (C) 2024 Eitan Isaacson <eitan@monotonous.org>
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

typedef struct _SpielProviderProxy SpielProviderProxy;
typedef struct _SpielProvider SpielProvider;

#define PROVIDER_SUFFIX ".Speech.Provider"

void spiel_collect_providers (GDBusConnection *connection,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data);

GHashTable *spiel_collect_providers_finish (GAsyncResult *res, GError **error);

GHashTable *spiel_collect_providers_sync (GDBusConnection *connection,
                                          GCancellable *cancellable,
                                          GError **error);

void spiel_collect_provider (GDBusConnection *connection,
                             GCancellable *cancellable,
                             const char *provider_name,
                             GAsyncReadyCallback callback,
                             gpointer user_data);

SpielProvider *spiel_collect_provider_finish (GAsyncResult *res,
                                              GError **error);
