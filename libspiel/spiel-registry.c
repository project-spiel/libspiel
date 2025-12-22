/* spiel-registry.c
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

#include "spiel-registry.h"

#include "spiel.h"

#include "spiel-provider-private.h"
#include "spiel-provider-proxy.h"
#include "spiel-voices-list-model.h"
#include "spiel-providers-list-model.h"

#include <gio/gio.h>
#include <gst/gst.h>

#define GSETTINGS_SCHEMA "org.monotonous.libspiel"

struct _SpielRegistry
{
  GObject parent_instance;
  GDBusConnection *connection;
  guint subscription_ids[2];
  SpielProvidersListModel *providers;
  SpielVoicesListModel *voices;
  GSettings *settings;
};

static void initable_iface_init (GInitableIface *initable_iface);
static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SpielRegistry,
    spiel_registry,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
        G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                               async_initable_iface_init))

static SpielRegistry *sRegistry = NULL;
static GSList *sPendingTasks = NULL;

enum
{
  PROVIDER_DIED,
  LAST_SIGNAL
};

static guint registry_signals[LAST_SIGNAL] = { 0 };

void
spiel_registry_get (GCancellable *cancellable,
                    GAsyncReadyCallback callback,
                    gpointer user_data)
{
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  if (sRegistry != NULL)
    {
      GTask *task = g_task_new (g_object_ref (sRegistry), cancellable, callback,
                                user_data);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
  else if (sPendingTasks)
    {
      GObject *source_object = g_task_get_source_object (sPendingTasks->data);
      GTask *task = g_task_new (g_object_ref (source_object), cancellable,
                                callback, user_data);
      sPendingTasks = g_slist_append (sPendingTasks, task);
    }
  else
    {
      g_async_initable_new_async (SPIEL_TYPE_REGISTRY, G_PRIORITY_DEFAULT,
                                  cancellable, callback, user_data, NULL);
    }
}

SpielRegistry *
spiel_registry_get_finish (GAsyncResult *result, GError **error)
{
  GObject *object;
  g_autoptr (GObject) source_object = g_async_result_get_source_object (result);
  g_assert (source_object != NULL);

  g_return_val_if_fail (G_IS_ASYNC_INITABLE (source_object), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        result, error);
  if (object != NULL)
    {
      if (sRegistry == NULL)
        {
          gst_init_check (NULL, NULL, error);
          sRegistry = SPIEL_REGISTRY (object);
        }
      g_assert (sRegistry == SPIEL_REGISTRY (object));
      return SPIEL_REGISTRY (object);
    }
  else
    {
      return NULL;
    }
}

SpielRegistry *
spiel_registry_get_sync (GCancellable *cancellable, GError **error)
{
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable),
                        NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (sRegistry == NULL)
    {
      gst_init_check (NULL, NULL, error);
      sRegistry =
          g_initable_new (SPIEL_TYPE_REGISTRY, cancellable, error, NULL);
    }
  else
    {
      g_object_ref (sRegistry);
    }

  return sRegistry;
}

static GSettings *
_settings_new (void)
{
  GSettingsSchema *schema = NULL;
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();
  GSettings *settings = NULL;

  schema = g_settings_schema_source_lookup (source, GSETTINGS_SCHEMA, TRUE);
  if (!schema)
    {
      g_debug ("libspiel settings schema is not installed");
      return NULL;
    }

  settings = g_settings_new (GSETTINGS_SCHEMA);
  g_settings_schema_unref (schema);

  return settings;
}

static void
_on_providers_init (GObject *source, GAsyncResult *result, gpointer user_data)
{
  SpielRegistry *self = g_task_get_source_object (G_TASK (user_data));
  GError *error = NULL;

  self->providers = spiel_providers_list_model_new_finish (result, &error);
  if (error != NULL)
    {
      g_warning ("Error retrieving providers: %s\n", error->message);
      while (sPendingTasks)
        {
          GTask *task = sPendingTasks->data;
          g_task_return_error (task, g_error_copy (error));
          g_object_unref (task);
          sPendingTasks = g_slist_delete_link (sPendingTasks, sPendingTasks);
        }
      return;
    }

  self->voices = spiel_voices_list_model_new (G_LIST_MODEL (self->providers));
  self->settings = _settings_new ();

  while (sPendingTasks)
    {
      GTask *task = sPendingTasks->data;
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      sPendingTasks = g_slist_delete_link (sPendingTasks, sPendingTasks);
    }
}

static void
async_initable_init_async (GAsyncInitable *initable,
                           gint io_priority,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  GTask *task = g_task_new (initable, cancellable, callback, user_data);

  g_assert (!sPendingTasks);
  sPendingTasks = g_slist_append (sPendingTasks, task);

  if (cancellable != NULL)
    {
      g_task_set_task_data (task, g_object_ref (cancellable), g_object_unref);
    }

  spiel_providers_list_model_new (cancellable, _on_providers_init, task);
}

static gboolean
async_initable_init_finish (GAsyncInitable *initable,
                            GAsyncResult *res,
                            GError **error)
{
  g_return_val_if_fail (g_task_is_valid (res, initable), FALSE);

  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
spiel_registry_finalize (GObject *object)
{
  SpielRegistry *self = (SpielRegistry *) object;

  g_clear_object (&self->providers);
  g_clear_object (&self->voices);
  g_clear_object (&self->settings);
  if (self->connection)
    {
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->subscription_ids[0]);
      g_dbus_connection_signal_unsubscribe (self->connection,
                                            self->subscription_ids[1]);
      g_clear_object (&self->connection);
    }

  G_OBJECT_CLASS (spiel_registry_parent_class)->finalize (object);
  sRegistry = NULL;
}

static gboolean
initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  SpielRegistry *self = (SpielRegistry *) initable;

  self->providers = spiel_providers_list_model_new_sync ();
  self->voices = spiel_voices_list_model_new (G_LIST_MODEL (self->providers));
  self->settings = _settings_new ();

  return TRUE;
}

static void
spiel_registry_init (SpielRegistry *self)
{
}

static void
spiel_registry_class_init (SpielRegistryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_registry_finalize;

  registry_signals[PROVIDER_DIED] = g_signal_new (
      "provider-died", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* Public API */

SpielProvider *
spiel_registry_get_provider_for_voice (SpielRegistry *self, SpielVoice *voice)
{
  g_autoptr (SpielProvider) voice_provider = NULL;

  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);
  g_return_val_if_fail (SPIEL_IS_VOICE (voice), NULL);

  return spiel_voice_get_provider (voice);
}

static SpielVoice *
_get_voice_from_provider_and_name (SpielRegistry *self,
                                   const char *provider_name,
                                   const char *voice_id)
{
  SpielProvider *provider = spiel_providers_list_model_get_by_name (
      self->providers, provider_name, NULL);
  g_return_val_if_fail (provider, NULL);

  return spiel_provider_get_voice_by_id (provider, voice_id);
}

static SpielVoice *
_get_fallback_voice (SpielRegistry *self, const char *language)
{
  if (language)
    {
      guint voices_count =
          g_list_model_get_n_items (G_LIST_MODEL (self->voices));

      for (guint i = 0; i < voices_count; i++)
        {
          SpielVoice *voice = SPIEL_VOICE (
              g_list_model_get_object (G_LIST_MODEL (self->voices), i));
          if (g_strv_contains (spiel_voice_get_languages (voice), language))
            {
              return voice;
            }
          g_object_unref (voice); // Just want to borrow a ref.
        }
    }

  return g_list_model_get_item ((GListModel *) self->voices, 0);
}

SpielVoice *
spiel_registry_get_voice_for_utterance (SpielRegistry *self,
                                        SpielUtterance *utterance)
{
  g_autofree char *provider_name = NULL;
  g_autofree char *voice_id = NULL;
  const char *language = NULL;
  SpielVoice *voice = NULL;

  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);
  g_return_val_if_fail (SPIEL_IS_UTTERANCE (utterance), NULL);

  voice = spiel_utterance_get_voice (utterance);
  if (voice)
    {
      return voice;
    }

  language = spiel_utterance_get_language (utterance);
  if (language && self->settings)
    {
      g_autoptr (GVariant) mapping =
          g_settings_get_value (self->settings, "language-voice-mapping");
      g_autofree char *_lang = g_strdup (language);
      char *found = _lang + g_utf8_strlen (_lang, -1);
      gssize boundary = -1;

      do
        {
          *found = 0;
          g_variant_lookup (mapping, _lang, "(ss)", &provider_name, &voice_id);
          if (provider_name)
            {
              break;
            }
          found = g_utf8_strrchr (_lang, boundary, '-');
          boundary = found - _lang - 1;
        }
      while (found);
    }

  if (!provider_name && self->settings)
    {
      g_settings_get (self->settings, "default-voice", "m(ss)", NULL,
                      &provider_name, &voice_id);
    }

  if (provider_name)
    {
      g_assert (voice_id != NULL);
      voice = _get_voice_from_provider_and_name (self, provider_name, voice_id);
    }

  if (voice)
    {
      return voice;
    }

  return _get_fallback_voice (self, language);
}

GListModel *
spiel_registry_get_voices (SpielRegistry *self)
{
  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);

  return G_LIST_MODEL (self->voices);
}

GListModel *
spiel_registry_get_providers (SpielRegistry *self)
{
  g_return_val_if_fail (SPIEL_IS_REGISTRY (self), NULL);

  return G_LIST_MODEL (self->providers);
}
