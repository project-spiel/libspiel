/* spiel-collect-providers.c
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

#include "spiel-collect-providers.h"

#include "spiel-provider-private.h"
#include "spiel-provider-proxy.h"

typedef struct
{
  GCancellable *cancellable;
  GHashTable *providers;
  GList *providers_to_process;
  char *provider_name;
} _CollectProvidersClosure;

static void
_collect_providers_closure_destroy (gpointer data)
{
  _CollectProvidersClosure *closure = data;
  g_clear_object (&closure->cancellable);
  g_hash_table_unref (closure->providers);
  if (closure->providers_to_process)
    {
      g_list_free (closure->providers_to_process);
    }

  if (closure->provider_name)
    {
      g_free (closure->provider_name);
    }

  g_slice_free (_CollectProvidersClosure, closure);
}

static void _on_list_activatable_names (GObject *source,
                                        GAsyncResult *result,
                                        gpointer user_data);

static gboolean _collect_provider_names (GObject *source,
                                         GAsyncResult *result,
                                         GTask *task,
                                         gboolean activatable);

static void _on_provider_created (GObject *source,
                                  GAsyncResult *result,
                                  gpointer user_data);

static void _create_next_provider (_CollectProvidersClosure *closure,
                                   GTask *task);

static void _create_provider (_CollectProvidersClosure *closure, GTask *task);

static void
_on_list_names (GObject *source, GAsyncResult *result, gpointer user_data);

static void _spiel_collect_providers (GDBusConnection *connection,
                                      GCancellable *cancellable,
                                      const char *provider_name,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data);

void
spiel_collect_providers (GDBusConnection *connection,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
  _spiel_collect_providers (connection, cancellable, NULL, callback, user_data);
}

void
spiel_collect_provider (GDBusConnection *connection,
                        GCancellable *cancellable,
                        const char *provider_name,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
  _spiel_collect_providers (connection, cancellable, provider_name, callback,
                            user_data);
}

static void
_spiel_collect_providers (GDBusConnection *connection,
                          GCancellable *cancellable,
                          const char *provider_name,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
  GTask *task = g_task_new (connection, cancellable, callback, user_data);
  _CollectProvidersClosure *closure = g_slice_new0 (_CollectProvidersClosure);

  closure->providers = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                              (GDestroyNotify) g_object_unref);
  closure->cancellable = cancellable ? g_object_ref (cancellable) : NULL;

  if (provider_name)
    {
      closure->provider_name = g_strdup (provider_name);
    }

  g_task_set_task_data (task, closure,
                        (GDestroyNotify) _collect_providers_closure_destroy);
  g_dbus_connection_call (connection, "org.freedesktop.DBus",
                          "/org/freedesktop/DBus", "org.freedesktop.DBus",
                          "ListActivatableNames", NULL, NULL,
                          G_DBUS_CALL_FLAGS_NONE, -1, closure->cancellable,
                          _on_list_activatable_names, task);
}

static void
_on_list_activatable_names (GObject *source,
                            GAsyncResult *result,
                            gpointer user_data)
{
  GTask *task = user_data;
  _CollectProvidersClosure *closure = g_task_get_task_data (task);
  GDBusConnection *connection = g_task_get_source_object (task);

  if (!_collect_provider_names (source, result, user_data, TRUE))
    {
      return;
    }

  g_dbus_connection_call (connection, "org.freedesktop.DBus",
                          "/org/freedesktop/DBus", "org.freedesktop.DBus",
                          "ListNames", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                          closure->cancellable, _on_list_names, task);
}

static void
_on_list_names (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  _CollectProvidersClosure *closure = g_task_get_task_data (task);

  if (!_collect_provider_names (source, result, user_data, FALSE))
    {
      return;
    }

  closure->providers_to_process = g_hash_table_get_keys (closure->providers);

  if (!closure->providers_to_process)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "No voice provider found");
      g_object_unref (task);
    }
  else
    {
      _create_provider (closure, task);
    }
}

static void
_create_next_provider (_CollectProvidersClosure *closure, GTask *task)
{
  GList *next_provider = closure->providers_to_process;
  closure->providers_to_process =
      g_list_remove_link (closure->providers_to_process, next_provider);
  g_list_free (next_provider);

  if (closure->providers_to_process)
    {
      _create_provider (closure, task);
      return;
    }

  // All done, lets return from the task
  if (closure->provider_name)
    {
      // If a provider name is specified we return a single result.
      SpielProvider *provider =
          g_hash_table_lookup (closure->providers, closure->provider_name);
      g_hash_table_steal (closure->providers, closure->provider_name);
      g_assert_cmpint (g_hash_table_size (closure->providers), ==, 0);
      g_task_return_pointer (task, provider, (GDestroyNotify) g_object_unref);
      g_object_unref (task);
    }
  else
    {
      // If no provider name was specified we return a hash table of
      // results.
      g_task_return_pointer (task, g_hash_table_ref (closure->providers),
                             (GDestroyNotify) g_hash_table_unref);
      g_object_unref (task);
    }
}

static char *
_object_path_from_service_name (const char *service_name)
{
  char **split_name = g_strsplit (service_name, ".", 0);
  g_autofree char *partial_path = g_strjoinv ("/", split_name);
  char *obj_path = g_strdup_printf ("/%s", partial_path);
  g_strfreev (split_name);
  return obj_path;
}

static void
_create_provider (_CollectProvidersClosure *closure, GTask *task)
{
  GList *next_provider = closure->providers_to_process;
  const char *service_name = next_provider->data;
  g_autofree char *obj_path = _object_path_from_service_name (service_name);

  spiel_provider_proxy_proxy_new_for_bus (G_BUS_TYPE_SESSION, 0, service_name,
                                          obj_path, closure->cancellable,
                                          _on_provider_created, task);
}

static void
_on_provider_created (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  _CollectProvidersClosure *closure = g_task_get_task_data (task);
  const char *service_name = closure->providers_to_process->data;
  g_autoptr (GError) error = NULL;
  g_autoptr (SpielProviderProxy) provider_proxy =
      spiel_provider_proxy_proxy_new_for_bus_finish (result, &error);

  if (error != NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          // If we are cancelled, return from this task.
          g_task_return_error (task, error);
          return;
        }
      else
        {
          // Otherwise, just warn and move on to the next provider.
          g_warning ("Error creating proxy for '%s': %s\n", service_name,
                     error->message);
        }
    }
  else
    {
      SpielProvider *provider =
          g_hash_table_lookup (closure->providers, service_name);

      g_assert (provider);
      g_assert (g_str_equal (
          g_dbus_proxy_get_name (G_DBUS_PROXY (provider_proxy)), service_name));

      if (provider)
        {
          spiel_provider_set_proxy (provider, provider_proxy);
        }
    }

  _create_next_provider (closure, task);
}

static gboolean
_collect_provider_names (GObject *source,
                         GAsyncResult *result,
                         GTask *task,
                         gboolean activatable)
{
  _CollectProvidersClosure *closure = g_task_get_task_data (task);
  GError *error = NULL;
  GDBusConnection *bus = G_DBUS_CONNECTION (source);
  g_autoptr (GVariant) real_ret = NULL;
  GVariantIter iter;
  GVariant *service = NULL;
  g_autoptr (GVariant) ret =
      g_dbus_connection_call_finish (bus, result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return FALSE;
    }

  real_ret = g_variant_get_child_value (ret, 0);

  g_variant_iter_init (&iter, real_ret);
  while ((service = g_variant_iter_next_value (&iter)))
    {
      const char *service_name = g_variant_get_string (service, NULL);
      if (g_str_has_suffix (service_name, PROVIDER_SUFFIX) &&
          (!closure->provider_name ||
           g_str_equal (closure->provider_name, service_name)))
        {
          SpielProvider *provider =
              g_hash_table_lookup (closure->providers, service_name);
          if (!provider)
            {
              provider = spiel_provider_new ();
              g_hash_table_insert (closure->providers, g_strdup (service_name),
                                   provider);
            }
          if (activatable)
            {
              spiel_provider_set_is_activatable (provider, TRUE);
            }
        }
      g_variant_unref (service);
    }

  return TRUE;
}

GHashTable *
spiel_collect_providers_finish (GAsyncResult *res, GError **error)
{
  return g_task_propagate_pointer (G_TASK (res), error);
}

SpielProvider *
spiel_collect_provider_finish (GAsyncResult *res, GError **error)
{
  return g_task_propagate_pointer (G_TASK (res), error);
}

GHashTable *
spiel_collect_providers_sync (GDBusConnection *connection,
                              GCancellable *cancellable,
                              GError **error)
{
  g_autoptr (GHashTable) providers = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);
  const char *list_name_methods[] = { "ListActivatableNames", "ListNames",
                                      NULL };
  for (const char **method = list_name_methods; *method; method++)
    {
      g_autoptr (GVariant) real_ret = NULL;
      GVariantIter iter;
      const char *service_name = NULL;
      g_autoptr (GVariant) ret = g_dbus_connection_call_sync (
          connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
          "org.freedesktop.DBus", *method, NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
          -1, NULL, error);
      if (error && *error)
        {
          g_warning ("Error calling list (%s): %s\n", *method,
                     (*error)->message);
          return NULL;
        }

      real_ret = g_variant_get_child_value (ret, 0);

      g_variant_iter_init (&iter, real_ret);
      while (g_variant_iter_loop (&iter, "s", &service_name) &&
             !g_cancellable_is_cancelled (cancellable))
        {
          g_autofree char *obj_path = NULL;
          SpielProvider *provider = NULL;
          g_autoptr (SpielProviderProxy) provider_proxy = NULL;
          g_autoptr (GError) err = NULL;

          if (!g_str_has_suffix (service_name, PROVIDER_SUFFIX) ||
              g_hash_table_contains (providers, service_name))
            {
              continue;
            }
          obj_path = _object_path_from_service_name (service_name);
          provider_proxy = spiel_provider_proxy_proxy_new_sync (
              connection, 0, service_name, obj_path, cancellable, &err);

          if (err)
            {
              g_warning ("Error creating proxy for '%s': %s\n", service_name,
                         err->message);
              continue;
            }

          provider = spiel_provider_new ();
          spiel_provider_set_proxy (provider, provider_proxy);
          spiel_provider_set_is_activatable (
              provider, g_str_equal (*method, "ListActivatableNames"));
          g_hash_table_insert (providers, g_strdup (service_name), provider);
        }
    }

  return g_steal_pointer (&providers);
}