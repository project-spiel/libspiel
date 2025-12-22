/* spiel-providers-list-model.c
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

#include "spiel-providers-list-model.h"

#include "spiel-provider-private.h"
#include "spiel-voice.h"

#define PROVIDER_SUFFIX ".Speech.Provider"

/**
 * SpielProvidersListModel:
 *
 * Represents an aggregate of all the providers available to Spiel.
 *
 *
 */

struct _SpielProvidersListModel
{
  GObject parent_instance;

  GDBusConnection *connection;
  guint subscription_ids[2];

  GListStore *providers_list;
  GHashTable *initializing_providers;
};

static void spiel_providers_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
    SpielProvidersListModel,
    spiel_providers_list_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                           spiel_providers_list_model_iface_init));

static void
_on_bus_get (GObject *source, GAsyncResult *result, gpointer user_data);

void
spiel_providers_list_model_new (GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
  SpielProvidersListModel *self =
      g_object_new (SPIEL_TYPE_PROVIDERS_LIST_MODEL, NULL);
  GTask *task = g_task_new (self, cancellable, callback, user_data);

  g_bus_get (G_BUS_TYPE_SESSION, cancellable, _on_bus_get, task);
}

SpielProvidersListModel *
spiel_providers_list_model_new_finish (GAsyncResult *result, GError **error)
{
  SpielProvidersListModel *self = g_task_get_source_object (G_TASK (result));
  return g_task_propagate_boolean (G_TASK (result), error) ? g_object_ref (self)
                                                           : NULL;
}

static void
_on_sync_init (GObject *source, GAsyncResult *result, gpointer user_data)
{
  g_autoptr (GError) error = NULL;
  if (!g_task_propagate_boolean (G_TASK (result), &error))
    {
      g_warning ("Failed to populate providers: %s", error->message);
    }
}

SpielProvidersListModel *
spiel_providers_list_model_new_sync (void)
{
  SpielProvidersListModel *self =
      g_object_new (SPIEL_TYPE_PROVIDERS_LIST_MODEL, NULL);

  GTask *task = g_task_new (self, NULL, _on_sync_init, NULL);

  g_bus_get (G_BUS_TYPE_SESSION, NULL, _on_bus_get, task);

  return self;
}

SpielProvider *
spiel_providers_list_model_get_by_name (SpielProvidersListModel *self,
                                        const char *provider_name,
                                        guint *position)
{
  guint providers_count =
      g_list_model_get_n_items (G_LIST_MODEL (self->providers_list));

  for (guint i = 0; i < providers_count; i++)
    {
      g_autoptr (SpielProvider) provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (self->providers_list), i));
      if (g_str_equal (provider_name, spiel_provider_get_identifier (provider)))
        {
          if (position)
            {
              *position = i;
            }
          return provider;
        }
    }

  return NULL;
}

static void
spiel_providers_list_model_finalize (GObject *object)
{
  SpielProvidersListModel *self = (SpielProvidersListModel *) object;

  g_clear_object (&self->providers_list);
  g_hash_table_unref (self->initializing_providers);

  G_OBJECT_CLASS (spiel_providers_list_model_parent_class)->finalize (object);
}

static void
spiel_providers_list_model_class_init (SpielProvidersListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_providers_list_model_finalize;
}

static void
_handle_providers_changed (GListModel *providers,
                           guint position,
                           guint removed,
                           guint added,
                           SpielProvidersListModel *self)
{
  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);
}

static void
spiel_providers_list_model_init (SpielProvidersListModel *self)
{
  self->providers_list = g_list_store_new (SPIEL_TYPE_PROVIDER);
  self->initializing_providers =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  g_signal_connect (self->providers_list, "items-changed",
                    G_CALLBACK (_handle_providers_changed), self);
}

static GType
spiel_providers_list_model_get_item_type (GListModel *list)
{
  return SPIEL_TYPE_PROVIDER;
}

static guint
spiel_providers_list_model_get_n_items (GListModel *list)
{
  SpielProvidersListModel *self = SPIEL_PROVIDERS_LIST_MODEL (list);
  return g_list_model_get_n_items (G_LIST_MODEL (self->providers_list));
}

static gpointer
spiel_providers_list_model_get_item (GListModel *list, guint position)
{
  SpielProvidersListModel *self = SPIEL_PROVIDERS_LIST_MODEL (list);
  return g_list_model_get_item (G_LIST_MODEL (self->providers_list), position);
}

static void
spiel_providers_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = spiel_providers_list_model_get_item_type;
  iface->get_n_items = spiel_providers_list_model_get_n_items;
  iface->get_item = spiel_providers_list_model_get_item;
}

static void
_get_provider_services_in_thread_func (GTask *task,
                                       gpointer source_object,
                                       gpointer task_data,
                                       GCancellable *cancellable)
{
  GDBusConnection *connection = g_task_get_source_object (task);
  g_autoptr (GHashTable) providers =
      g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  const char *list_name_methods[] = { "ListActivatableNames", "ListNames",
                                      NULL };
  for (const char **method = list_name_methods; *method; method++)
    {
      gboolean is_activatable = g_str_equal (*method, "ListActivatableNames");
      GError *error = NULL;
      g_autoptr (GVariant) real_ret = NULL;
      GVariantIter iter;
      const char *service_name = NULL;
      g_autoptr (GVariant) ret = g_dbus_connection_call_sync (
          connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
          "org.freedesktop.DBus", *method, NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
          -1, NULL, &error);
      if (error)
        {
          g_task_return_error (task, error);
          g_object_unref (task);
          return;
        }

      real_ret = g_variant_get_child_value (ret, 0);

      g_variant_iter_init (&iter, real_ret);
      while (g_variant_iter_loop (&iter, "s", &service_name) &&
             !g_cancellable_is_cancelled (cancellable))
        {
          g_autofree char *obj_path = NULL;
          g_autoptr (GError) err = NULL;

          if (!g_str_has_suffix (service_name, PROVIDER_SUFFIX) ||
              g_hash_table_contains (providers, service_name))
            {
              continue;
            }

          g_hash_table_insert (providers, g_strdup (service_name),
                               GINT_TO_POINTER (is_activatable));
        }
    }

  g_task_return_pointer (task, g_steal_pointer (&providers),
                         (GDestroyNotify) g_hash_table_unref);
  g_object_unref (task);
}

static void
_get_provider_services (GDBusConnection *connection,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  g_task_run_in_thread (task, _get_provider_services_in_thread_func);
}

static GHashTable *
_get_provider_services_finish (GAsyncResult *res, GError **error)
{
  return g_task_propagate_pointer (G_TASK (res), error);
}

static void
_insert_provider (const char *provider_name,
                  SpielProvider *new_provider,
                  SpielProvidersListModel *self)
{
  g_assert (g_hash_table_remove (self->initializing_providers, provider_name));
  g_assert (
      !spiel_providers_list_model_get_by_name (self, provider_name, NULL));
  g_list_store_insert_sorted (self->providers_list, new_provider,
                              (GCompareDataFunc) spiel_provider_compare, NULL);
}

static void
_on_provider_created (GObject *source, GAsyncResult *result, gpointer user_data)
{
  SpielProvidersListModel *self = SPIEL_PROVIDERS_LIST_MODEL (user_data);
  g_autoptr (GError) error = NULL;
  SpielProvider *provider = spiel_provider_new_direct_finish (result, &error);

  if (error != NULL)
    {
      g_warning ("Error creating provider: %s", error->message);
      return;
    }

  _insert_provider (spiel_provider_get_well_known_name (provider), provider,
                    self);
}

static void
_on_providers_updated (GObject *source, GAsyncResult *res, gpointer user_data)
{
  SpielProvidersListModel *self = SPIEL_PROVIDERS_LIST_MODEL (user_data);
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  g_autoptr (GError) error = NULL;
  g_autoptr (GHashTable) providers =
      _get_provider_services_finish (res, &error);
  GHashTableIter iter;
  char *well_known_name;
  gpointer is_activatable;

  if (error != NULL)
    {
      g_warning ("Error updating providers: %s\n", error->message);
      return;
    }

  g_hash_table_iter_init (&iter, providers);
  while (g_hash_table_iter_next (&iter, (gpointer *) &well_known_name,
                                 &is_activatable))
    {
      SpielProvider *provider =
          spiel_providers_list_model_get_by_name (self, well_known_name, NULL);

      if (provider)
        {
          spiel_provider_set_is_activatable (provider,
                                             GPOINTER_TO_INT (is_activatable));
          continue;
        }

      if (g_hash_table_insert (self->initializing_providers,
                               g_strdup (well_known_name), NULL))
        {
          spiel_provider_new_direct (connection, well_known_name,
                                     GPOINTER_TO_INT (is_activatable), NULL,
                                     _on_provider_created, self);
        }
    }

  for (gint i =
           g_list_model_get_n_items (G_LIST_MODEL (self->providers_list)) - 1;
       i >= 0; i--)
    {
      g_autoptr (SpielProvider) provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (self->providers_list), i));
      if (!g_hash_table_contains (providers,
                                  spiel_provider_get_identifier (provider)))
        {
          g_list_store_remove (self->providers_list, i);
        }
    }
}

static void
_maybe_activatable_providers_changed (GDBusConnection *connection,
                                      const gchar *sender_name,
                                      const gchar *object_path,
                                      const gchar *interface_name,
                                      const gchar *signal_name,
                                      GVariant *parameters,
                                      gpointer user_data)
{
  SpielProvidersListModel *self = user_data;

  // No arguments given, so update the whole providers cache.
  _get_provider_services (self->connection, NULL, _on_providers_updated, self);
}

static void
_maybe_running_providers_changed (GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data)
{
  SpielProvidersListModel *self = user_data;
  const char *service_name;
  const char *old_owner;
  const char *new_owner;
  g_variant_get (parameters, "(&s&s&s)", &service_name, &old_owner, &new_owner);
  if (g_str_has_suffix (service_name, PROVIDER_SUFFIX))
    {
      _get_provider_services (self->connection, NULL, _on_providers_updated,
                              self);
    }
}

static void
_on_initial_provider_created (GObject *source,
                              GAsyncResult *result,
                              gpointer user_data)
{
  GTask *task = user_data;
  SpielProvidersListModel *self = g_task_get_source_object (task);
  GError *error = NULL;
  SpielProvider *provider = spiel_provider_new_direct_finish (result, &error);

  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  _insert_provider (spiel_provider_get_well_known_name (provider), provider,
                    self);

  if (g_hash_table_size (self->initializing_providers) == 0)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
}

static void
_on_providers_collected (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GTask *task = G_TASK (user_data);
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  SpielProvidersListModel *self = g_task_get_source_object (task);
  GError *error = NULL;
  g_autoptr (GHashTable) providers =
      _get_provider_services_finish (res, &error);
  GHashTableIter iter;
  char *well_known_name;
  gpointer is_activatable;

  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_hash_table_iter_init (&iter, providers);
  while (g_hash_table_iter_next (&iter, (gpointer *) &well_known_name,
                                 &is_activatable))
    {
      if (g_hash_table_insert (self->initializing_providers,
                               g_strdup (well_known_name), NULL))
        {
          spiel_provider_new_direct (connection, well_known_name,
                                     GPOINTER_TO_INT (is_activatable),
                                     g_task_get_cancellable (task),
                                     _on_initial_provider_created, task);
        }
    }

  self->subscription_ids[0] = g_dbus_connection_signal_subscribe (
      self->connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "ActivatableServicesChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _maybe_activatable_providers_changed, self,
      NULL);

  self->subscription_ids[1] = g_dbus_connection_signal_subscribe (
      self->connection, "org.freedesktop.DBus", "org.freedesktop.DBus",
      "NameOwnerChanged", "/org/freedesktop/DBus", NULL,
      G_DBUS_SIGNAL_FLAGS_NONE, _maybe_running_providers_changed, self, NULL);
}

static void
_on_bus_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  GCancellable *cancellable = g_task_get_task_data (task);
  SpielProvidersListModel *self = g_task_get_source_object (task);
  GError *error = NULL;

  self->connection = g_bus_get_finish (result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  _get_provider_services (self->connection, cancellable,
                          _on_providers_collected, task);
}
