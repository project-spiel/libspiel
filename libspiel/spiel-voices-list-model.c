/* spiel-utterance.c
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

#include "spiel-voices-list-model.h"

#include "spiel-provider-private.h"
#include "spiel-voice.h"

/**
 * SpielVoicesListModel:
 *
 * Represents an aggregate of all the voices available to Spiel.
 *
 *
 */

struct _SpielVoicesListModel
{
  GObject parent_instance;

  GListModel *providers;
  GListStore *mirrored_providers; // XXX: Used to track removed providers
};

static void spiel_voices_list_model_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (
    SpielVoicesListModel,
    spiel_voices_list_model,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL,
                           spiel_voices_list_model_iface_init));

static void handle_providers_changed (GListModel *providers,
                                      guint position,
                                      guint removed,
                                      guint added,
                                      SpielVoicesListModel *self);

SpielVoicesListModel *
spiel_voices_list_model_new (GListModel *providers)
{
  SpielVoicesListModel *self =
      g_object_new (SPIEL_TYPE_VOICES_LIST_MODEL, NULL);

  g_assert_cmpint (g_list_model_get_n_items (providers), ==, 0);
  self->providers = g_object_ref (providers);
  g_signal_connect (self->providers, "items-changed",
                    G_CALLBACK (handle_providers_changed), self);
  return self;
}

static void
spiel_voices_list_model_finalize (GObject *object)
{
  SpielVoicesListModel *self = (SpielVoicesListModel *) object;

  g_signal_handlers_disconnect_by_func (
      self->providers, G_CALLBACK (handle_providers_changed), self);

  g_clear_object (&(self->providers));

  G_OBJECT_CLASS (spiel_voices_list_model_parent_class)->finalize (object);
}

static void
spiel_voices_list_model_class_init (SpielVoicesListModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_voices_list_model_finalize;
}

static void
spiel_voices_list_model_init (SpielVoicesListModel *self)
{
  self->providers = NULL;
  self->mirrored_providers = g_list_store_new (SPIEL_TYPE_PROVIDER);
}

static GType
spiel_voices_list_model_get_item_type (GListModel *list)
{
  return SPIEL_TYPE_VOICE;
}

static guint
spiel_voices_list_model_get_n_items (GListModel *list)
{
  SpielVoicesListModel *self = SPIEL_VOICES_LIST_MODEL (list);
  guint total = 0;
  guint providers_count = g_list_model_get_n_items (self->providers);

  for (guint i = 0; i < providers_count; i++)
    {
      SpielProvider *provider =
          SPIEL_PROVIDER (g_list_model_get_object (self->providers, i));
      GListModel *voices = G_LIST_MODEL (spiel_provider_get_voices (provider));
      total += g_list_model_get_n_items (voices);
      g_object_unref (provider); // Just want to borrow a ref.
    }

  return total;
}

static gpointer
spiel_voices_list_model_get_item (GListModel *list, guint position)
{
  SpielVoicesListModel *self = SPIEL_VOICES_LIST_MODEL (list);
  guint total = 0;
  guint providers_count = g_list_model_get_n_items (self->providers);

  for (guint i = 0; i < providers_count; i++)
    {
      SpielProvider *provider =
          SPIEL_PROVIDER (g_list_model_get_object (self->providers, i));
      GListModel *voices = spiel_provider_get_voices (provider);
      guint voice_count = g_list_model_get_n_items (voices);
      g_object_unref (provider); // Just want to borrow a ref.

      if (position >= total && position < (total + voice_count))
        {
          return g_list_model_get_item (voices, position - total);
        }
      total += voice_count;
    }

  return NULL;
}

static void
spiel_voices_list_model_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = spiel_voices_list_model_get_item_type;
  iface->get_n_items = spiel_voices_list_model_get_n_items;
  iface->get_item = spiel_voices_list_model_get_item;
}

/* Notifications and such */

static void
handle_voices_changed (GListModel *voices,
                       guint position,
                       guint removed,
                       guint added,
                       SpielVoicesListModel *self)
{
  guint offset = 0;
  guint providers_count = g_list_model_get_n_items (self->providers);

  for (guint i = 0; i < providers_count; i++)
    {
      SpielProvider *provider =
          SPIEL_PROVIDER (g_list_model_get_object (self->providers, i));
      g_object_unref (provider); // Just want to borrow a ref.
      if (voices == spiel_provider_get_voices (provider))
        {
          g_list_model_items_changed (G_LIST_MODEL (self), position + offset,
                                      removed, added);
          break;
        }
      offset += g_list_model_get_n_items (voices);
    }
}

static void
_connect_signals (SpielVoicesListModel *self, SpielProvider *provider)
{
  GListModel *voices = spiel_provider_get_voices (provider);
  g_signal_connect (voices, "items-changed", G_CALLBACK (handle_voices_changed),
                    self);
}

static void
_disconnect_signals (SpielVoicesListModel *self, SpielProvider *provider)
{
  GListModel *voices = spiel_provider_get_voices (provider);
  g_signal_handlers_disconnect_by_func (
      voices, G_CALLBACK (handle_voices_changed), self);
}

static void
handle_providers_changed (GListModel *providers,
                          guint position,
                          guint removed,
                          guint added,
                          SpielVoicesListModel *self)
{
  guint removed_voices_count = 0;
  guint added_voices_count = 0;
  guint offset = 0;

  for (guint i = position; i < position + removed; i++)
    {
      SpielProvider *provider = SPIEL_PROVIDER (
          g_list_model_get_object (G_LIST_MODEL (self->mirrored_providers), i));
      GListModel *voices = spiel_provider_get_voices (provider);
      removed_voices_count += g_list_model_get_n_items (voices);
      _disconnect_signals (self, provider);
      g_object_unref (provider);
    }

  g_list_store_splice (self->mirrored_providers, position, removed, NULL, 0);

  for (guint i = position; i < position + added; i++)
    {
      SpielProvider *provider =
          SPIEL_PROVIDER (g_list_model_get_object (self->providers, i));
      GListModel *voices = spiel_provider_get_voices (provider);
      added_voices_count += g_list_model_get_n_items (voices);
      _connect_signals (self, provider);
      g_list_store_insert (self->mirrored_providers, i, provider);
      g_object_unref (provider);
    }

  for (guint i = 0; i < position; i++)
    {
      SpielProvider *provider =
          SPIEL_PROVIDER (g_list_model_get_object (self->providers, i));
      GListModel *voices = spiel_provider_get_voices (provider);
      offset += g_list_model_get_n_items (voices);
      g_object_unref (provider);
    }

  g_list_model_items_changed (G_LIST_MODEL (self), offset, removed_voices_count,
                              added_voices_count);
}
