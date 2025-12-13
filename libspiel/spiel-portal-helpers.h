/* spiel-portal-helpers.h
 *
 * Copyright (C) 2025 Eitan Isaacson <eitan@monotonous.org>
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

void _portal_create_session (GDBusConnection *connection,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data);

char *_portal_create_session_finish (GAsyncResult *result, GError **error);

void _portal_get_providers (GDBusConnection *connection,
                            const char *session_handle,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data);

GHashTable *_portal_get_providers_finish (GAsyncResult *result, GError **error);

typedef void (*PortalProvidersChangedCallback) (GHashTable *providers,
                                                gpointer data);

guint
_portal_subscribe_to_providers_changed (GDBusConnection *connection,
                                        const char *session_handle,
                                        PortalProvidersChangedCallback callback,
                                        gpointer user_data);

void _portal_unsubscribe_from_providers_changed (GDBusConnection *connection,
                                                 guint subscription_id);

void _portal_get_voices (GDBusConnection *connection,
                         const char *session_handle,
                         const char *well_known_name,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data);

GVariant *_portal_get_voices_finish (GAsyncResult *result, GError **error);

typedef void (*PortalVoicesChangedCallback) (GVariant *voices_variant,
                                             gpointer data);

guint _portal_subscribe_to_voices_changed (GDBusConnection *connection,
                                           const char *session_handle,
                                           const char *well_known_name,
                                           PortalVoicesChangedCallback callback,
                                           gpointer user_data);

void _portal_unsubscribe_from_voices_changed (GDBusConnection *connection,
                                              guint subscription_id);

void _portal_synthesize (GDBusConnection *connection,
                         const char *session_handle,
                         const char *well_known_name,
                         int fd,
                         const char *text,
                         const char *voice_id,
                         gdouble pitch,
                         gdouble rate,
                         gboolean is_ssml,
                         const char *language,
                         GUnixFDList *fd_list,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data);

gboolean _portal_synthesize_finish (GAsyncResult *result, GError **error);

G_END_DECLS
