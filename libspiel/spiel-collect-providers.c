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

#include "spiel-voice.h"
#include "spieldbusgenerated.h"

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

static void
_free_voices_slist (GSList *voices)
{
  g_slist_free_full (voices, (GDestroyNotify) g_object_unref);
}

void
spiel_collect_free_provider_and_voices (ProviderAndVoices *provider_and_voices)
{
  g_clear_object (&provider_and_voices->provider);
  _free_voices_slist (provider_and_voices->voices);
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

  closure->providers = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) spiel_collect_free_provider_and_voices);
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
      g_task_return_pointer (task, NULL, NULL);
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
      ProviderAndVoices *pav =
          g_hash_table_lookup (closure->providers, closure->provider_name);
      g_hash_table_steal (closure->providers, closure->provider_name);
      g_assert_cmpint (g_hash_table_size (closure->providers), ==, 0);
      g_task_return_pointer (
          task, pav, (GDestroyNotify) spiel_collect_free_provider_and_voices);
    }
  else
    {
      // If no provider name was specified we return a hash table of
      // results.
      g_task_return_pointer (task, g_hash_table_ref (closure->providers),
                             (GDestroyNotify) g_hash_table_unref);
    }
}

static char *
_object_path_from_service_name (const char *service_name)
{
  char **split_name = g_strsplit (service_name, ".", 0);
  char *partial_path = g_strjoinv ("/", split_name);
  char *obj_path = g_strdup_printf ("/%s", partial_path);
  g_strfreev (split_name);
  g_free (partial_path);
  return obj_path;
}

static void
_create_provider (_CollectProvidersClosure *closure, GTask *task)
{
  GList *next_provider = closure->providers_to_process;
  const char *service_name = next_provider->data;
  char *obj_path = _object_path_from_service_name (service_name);

  spiel_provider_proxy_new_for_bus (G_BUS_TYPE_SESSION, 0, service_name,
                                    obj_path, closure->cancellable,
                                    _on_provider_created, task);

  g_free (obj_path);
}

static void
_on_provider_created (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  _CollectProvidersClosure *closure = g_task_get_task_data (task);
  const char *service_name = closure->providers_to_process->data;
  GError *error = NULL;
  SpielProvider *provider =
      spiel_provider_proxy_new_for_bus_finish (result, &error);

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
          g_error_free (error);
        }
    }
  else
    {
      ProviderAndVoices *provider_and_voices =
          g_hash_table_lookup (closure->providers, service_name);

      g_assert (provider_and_voices);
      g_assert (g_str_equal (g_dbus_proxy_get_name (G_DBUS_PROXY (provider)),
                             service_name));

      if (provider_and_voices)
        {
          // Take over ownership
          provider_and_voices->provider = provider;
          provider_and_voices->voices =
              spiel_collect_provider_voices (provider);
        }
    }

  _create_next_provider (closure, task);
}

GSList *
spiel_collect_provider_voices (SpielProvider *provider)
{
  const char *provider_name = g_dbus_proxy_get_name (G_DBUS_PROXY (provider));
  GSList *voices_slist = NULL;
  GVariant *voices = spiel_provider_get_voices (provider);
  gsize voices_count = voices ? g_variant_n_children (voices) : 0;

  for (gsize i = 0; i < voices_count; i++)
    {
      const char *name = NULL;
      const char *identifier = NULL;
      const char *output_format = NULL;
      const char **languages = NULL;
      guint64 features;
      SpielVoice *voice;

      g_variant_get_child (voices, i, "(&s&s&st^a&s)", &name, &identifier,
                           &output_format, &features, &languages);
      if (features >> 32)
        {
          g_warning ("Voice features past 32 bits are ignored in %s (%s)",
                     identifier, provider_name);
        }
      voice = g_object_new (SPIEL_TYPE_VOICE, "name", name, "identifier",
                            identifier, "languages", languages, "provider-name",
                            provider_name, "features", features, NULL);
      spiel_voice_set_output_format (voice, output_format);

      voices_slist = g_slist_prepend (voices_slist, voice);
      g_free (languages);
    }

  return voices_slist;
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
  GVariant *real_ret = NULL;
  GVariantIter iter;
  GVariant *service = NULL;
  GVariant *ret = g_dbus_connection_call_finish (bus, result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return FALSE;
    }

  real_ret = g_variant_get_child_value (ret, 0);
  g_variant_unref (ret);

  g_variant_iter_init (&iter, real_ret);
  while ((service = g_variant_iter_next_value (&iter)))
    {
      const char *service_name = g_variant_get_string (service, NULL);
      if (g_str_has_suffix (service_name, PROVIDER_SUFFIX) &&
          (!closure->provider_name ||
           g_str_equal (closure->provider_name, service_name)))
        {
          ProviderAndVoices *provider_and_voices =
              g_hash_table_lookup (closure->providers, service_name);
          if (!provider_and_voices)
            {
              provider_and_voices = g_slice_new0 (ProviderAndVoices);
              g_hash_table_insert (closure->providers, g_strdup (service_name),
                                   provider_and_voices);
            }
          provider_and_voices->is_activatable |= activatable;
        }
      g_variant_unref (service);
    }
  g_variant_unref (real_ret);

  return TRUE;
}

GHashTable *
spiel_collect_providers_finish (GAsyncResult *res, GError **error)
{
  return g_task_propagate_pointer (G_TASK (res), error);
}

ProviderAndVoices *
spiel_collect_provider_finish (GAsyncResult *res, GError **error)
{
  return g_task_propagate_pointer (G_TASK (res), error);
}

GHashTable *
spiel_collect_providers_sync (GDBusConnection *connection,
                              GCancellable *cancellable,
                              GError **error)
{
  GHashTable *providers = g_hash_table_new_full (
      g_str_hash, g_str_equal, g_free,
      (GDestroyNotify) spiel_collect_free_provider_and_voices);
  const char *list_name_methods[] = { "ListActivatableNames", "ListNames",
                                      NULL };
  for (const char **method = list_name_methods; *method; method++)
    {
      GVariant *real_ret = NULL;
      GVariantIter iter;
      GVariant *service = NULL;
      GVariant *ret = g_dbus_connection_call_sync (
          connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
          "org.freedesktop.DBus", *method, NULL, NULL, G_DBUS_CALL_FLAGS_NONE,
          -1, NULL, error);
      if (*error)
        {
          g_warning ("Error calling list (%s): %s\n", *method,
                     (*error)->message);
          g_hash_table_unref (providers);
          return NULL;
        }

      real_ret = g_variant_get_child_value (ret, 0);
      g_variant_unref (ret);

      g_variant_iter_init (&iter, real_ret);
      while ((service = g_variant_iter_next_value (&iter)) &&
             !g_cancellable_is_cancelled (cancellable))
        {
          const char *service_name = g_variant_get_string (service, NULL);
          char *obj_path = NULL;
          ProviderAndVoices *provider_and_voices = NULL;
          SpielProvider *provider = NULL;
          GError *err = NULL;

          if (!g_str_has_suffix (service_name, PROVIDER_SUFFIX) ||
              g_hash_table_contains (providers, service_name))
            {
              continue;
            }
          obj_path = _object_path_from_service_name (service_name);
          provider = spiel_provider_proxy_new_sync (
              connection, 0, service_name, obj_path, cancellable, &err);
          g_free (obj_path);

          if (err)
            {
              g_warning ("Error creating proxy for '%s': %s\n", service_name,
                         err->message);
              g_error_free (err);
              continue;
            }

          provider_and_voices = g_slice_new0 (ProviderAndVoices);
          provider_and_voices->provider = provider;
          provider_and_voices->voices =
              spiel_collect_provider_voices (provider);
          provider_and_voices->is_activatable =
              g_str_equal (*method, "ListActivatableNames");
          g_hash_table_insert (providers, g_strdup (service_name),
                               provider_and_voices);
          g_variant_unref (service);
        }
      g_variant_unref (real_ret);
    }

  return providers;
}