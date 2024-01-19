/* spiel-speaker.c
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

#include "spiel.h"

#include "spiel-speaker.h"

#include "spiel-registry.h"
#include "spiel-voice.h"
#include "spieldbusgenerated.h"
#include <gio/gio.h>

/**
 * SpielSpeaker:
 *
 * A virtual speaker for speech synthesis
 *
 * The #SpielSpeaker class represents a single "individual" speaker. Its primary
 * method is [method@Speaker.speak] which queues utterances to be spoken.
 *
 * This class also provides a list of available voices provided by DBus speech
 * providers that are activatable on the session bus.
 *
 * #SpielSpeaker's initialization may perform blocking IO if it the first
 * instance in the process. The default constructor is asynchronous
 * ([func@Speaker.new]), although there is a synchronous blocking alternative
 * ([ctor@Speaker.new_sync]).
 *
 */

struct _SpielSpeaker
{
  GObject parent_instance;
};

typedef struct
{
  gboolean speaking;
  gboolean paused;
  SpielRegistry *registry;
  GSList *queue;
} SpielSpeakerPrivate;

static void initable_iface_init (GInitableIface *initable_iface);
static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (
    SpielSpeaker,
    spiel_speaker,
    G_TYPE_OBJECT,
    G_ADD_PRIVATE (SpielSpeaker)
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
            G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE,
                                   async_initable_iface_init))

enum
{
  UTTURANCE_STARTED,
  RANGE_STARTED,
  UTTERANCE_FINISHED,
  UTTERANCE_CANCELED,
  UTTERANCE_ERROR,
  LAST_SIGNAL
};

static guint speaker_signals[LAST_SIGNAL] = { 0 };

enum
{
  PROP_0,
  PROP_SPEAKING,
  PROP_PAUSED,
  PROP_VOICES,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct
{
  SpielUtterance *utterance;
} _QueueEntry;

static void
_queue_entry_destroy (gpointer data)
{
  _QueueEntry *entry = data;
  if (entry)
    {
      g_clear_object (&entry->utterance);
    }

  g_slice_free (_QueueEntry, entry);
}

/**
 * spiel_speaker_new:
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: User data to pass to @callback.
 *
 * Asynchronously creates a SpielSpeaker.
 *
 * When the operation is finished, @callback will be invoked in the
 * thread-default main loop of the thread you are calling this method from (see
 * g_main_context_push_thread_default()). You can then call
 * spiel_speaker_new_finish() to get the result of the operation.
 *
 * See spiel_speaker_new_sync() for the synchronous, blocking version of this
 * constructor.
 */
void
spiel_speaker_new (GCancellable *cancellable,
                   GAsyncReadyCallback callback,
                   gpointer user_data)
{
  g_async_initable_new_async (SPIEL_TYPE_SPEAKER, G_PRIORITY_DEFAULT,
                              cancellable, callback, user_data, NULL);
}

/**
 * spiel_speaker_new_finish:
 * @result: The #GAsyncResult obtained from the #GAsyncReadyCallback passed to
 * spiel_speaker_new().
 * @error: Return location for error or %NULL
 *
 * Finishes an operation started with spiel_speaker_new().
 *
 * Returns: (transfer full) (type SpielSpeaker): The constructed speaker object
 * or %NULL if @error is set.
 */
SpielSpeaker *
spiel_speaker_new_finish (GAsyncResult *result, GError **error)
{
  GObject *object;
  GObject *source_object;

  source_object = g_async_result_get_source_object (result);
  g_assert (source_object != NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        result, error);
  g_object_unref (source_object);

  if (object != NULL)
    return SPIEL_SPEAKER (object);
  else
    return NULL;
}

/**
 * spiel_speaker_new_sync:
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL
 *
 * Synchronously creates a SpielSpeaker.
 *
 * The calling thread is blocked until a reply is received.
 *
 * See spiel_speaker_new() for the asynchronous version of this constructor.
 *
 * Returns: (transfer full) (type SpielSpeaker): The constructed speaker object
 * or %NULL if @error is set.
 */
SpielSpeaker *
spiel_speaker_new_sync (GCancellable *cancellable, GError **error)
{
  return g_initable_new (SPIEL_TYPE_SPEAKER, cancellable, error, NULL);
}

static void
spiel_speaker_finalize (GObject *object)
{
  SpielSpeaker *self = SPIEL_SPEAKER (object);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);

  g_clear_object (&priv->registry);
  g_slist_free_full (g_steal_pointer (&priv->queue),
                     (GDestroyNotify) _queue_entry_destroy);

  G_OBJECT_CLASS (spiel_speaker_parent_class)->finalize (object);
}

static void
spiel_speaker_get_property (GObject *object,
                            guint prop_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  SpielSpeaker *self = SPIEL_SPEAKER (object);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SPEAKING:
      g_value_set_boolean (value, priv->speaking);
      break;
    case PROP_PAUSED:
      g_value_set_boolean (value, priv->paused);
      break;
    case PROP_VOICES:
      g_value_set_object (value, spiel_registry_get_voices (priv->registry));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_speaker_class_init (SpielSpeakerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_speaker_finalize;
  object_class->get_property = spiel_speaker_get_property;

  /**
   * SpielSpeaker:speaking:
   *
   * The speaker has an utterance queued or speaking.
   *
   */
  properties[PROP_SPEAKING] =
      g_param_spec_boolean ("speaking", NULL, NULL, FALSE /* default value */,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielSpeaker:paused:
   *
   * The speaker is in a paused state.
   *
   */
  properties[PROP_PAUSED] =
      g_param_spec_boolean ("paused", NULL, NULL, FALSE /* default value */,
                            G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  /**
   * SpielSpeaker:voices:
   *
   * The list of available [class@Voice]s that can be used in an utterance.
   *
   */
  properties[PROP_VOICES] =
      g_param_spec_object ("voices", NULL, NULL, G_TYPE_LIST_MODEL,
                           G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);

  /**
   * SpielSpeaker::utterance-started:
   * @speaker: A #SpielSpeaker
   * @utterance: A #SpielUtterance
   *
   * Emitted when the given utterance is actively being spoken.
   *
   */
  speaker_signals[UTTURANCE_STARTED] = g_signal_new (
      "utterance-started", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 1, SPIEL_TYPE_UTTERANCE);

  /**
   * SpielSpeaker::range-started:
   * @speaker: A #SpielSpeaker
   * @utterance: A #SpielUtterance
   * @start: Character start offset of speech range
   * @end: Character end offset of speech range
   *
   * Emitted when a range will be spoken in a given utterance. Not all
   * voices are capable of notifying when a range will be spoken.
   *
   */
  speaker_signals[RANGE_STARTED] =
      g_signal_new ("range-started", G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 3,
                    SPIEL_TYPE_UTTERANCE, G_TYPE_UINT64, G_TYPE_UINT64);

  /**
   * SpielSpeaker::utterance-finished:
   * @speaker: A #SpielSpeaker
   * @utterance: A #SpielUtterance
   *
   * Emitted when a given utterance has completed speaking
   *
   */
  speaker_signals[UTTERANCE_FINISHED] = g_signal_new (
      "utterance-finished", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 1, SPIEL_TYPE_UTTERANCE);

  /**
   * SpielSpeaker::utterance-canceled:
   * @speaker: A #SpielSpeaker
   * @utterance: A #SpielUtterance
   *
   * Emitted when a given utterance has been canceled after it has started
   * speaking
   *
   */
  speaker_signals[UTTERANCE_CANCELED] = g_signal_new (
      "utterance-canceled", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0,
      NULL, NULL, NULL, G_TYPE_NONE, 1, SPIEL_TYPE_UTTERANCE);

  /**
   * SpielSpeaker::utterance-error:
   * @speaker: A #SpielSpeaker
   * @utterance: A #SpielUtterance
   * @error: A #GError
   *
   * Emitted when a given utterance has failed to start or complete.
   *
   */
  speaker_signals[UTTERANCE_ERROR] = g_signal_new (
      "utterance-error", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_FIRST, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, SPIEL_TYPE_UTTERANCE, G_TYPE_ERROR);
}

static gboolean handle_provider_died (SpielProvider *provider,
                                      const char *provider_name,
                                      gpointer user_data);

static void
_connect_signals (SpielSpeaker *self)
{
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  g_object_connect (priv->registry, "object_signal::provider-died",
                    G_CALLBACK (handle_provider_died), self, NULL);
}

static void
_on_registry_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  SpielSpeaker *self = g_task_get_task_data (task);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  GError *error = NULL;

  priv->registry = spiel_registry_get_finish (result, &error);
  if (error != NULL)
    {
      g_task_return_error (task, error);
    }
  else
    {
      _connect_signals (self);
      g_task_return_boolean (task, TRUE);
    }

  g_object_unref (task);
}

static void
async_initable_init_async (GAsyncInitable *initable,
                           gint io_priority,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
  GTask *task = g_task_new (initable, cancellable, callback, user_data);
  SpielSpeaker *self = SPIEL_SPEAKER (initable);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);

  priv->speaking = FALSE;
  priv->paused = FALSE;
  priv->queue = NULL;

  g_task_set_task_data (task, g_object_ref (self), g_object_unref);
  spiel_registry_get (cancellable, _on_registry_get, task);
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
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

static gboolean
initable_init (GInitable *initable, GCancellable *cancellable, GError **error)
{
  SpielSpeaker *self = SPIEL_SPEAKER (initable);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  priv->registry = spiel_registry_get_sync (cancellable, error);
  if (*error)
    {
      g_warning ("Error initializing speaker: %s\n", (*error)->message);
      return FALSE;
    }

  _connect_signals (self);
  return TRUE;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

static void
spiel_speaker_init (SpielSpeaker *self)
{
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  priv->speaking = FALSE;
  priv->paused = FALSE;
  priv->queue = NULL;
}

GQuark
spiel_error_quark (void)
{
  return g_quark_from_static_string ("spiel-error-quark");
}

/**
 * Playback
 */

/**
 * spiel_speaker_speak:
 * @self: a #SpielSpeaker
 * @utterance: an #SpielUtterance to speak
 *
 * Speak the given utterance. If an utterance is already being spoken
 * the provided utterances will be added to a queue and will be spoken
 * in the order recieved.
 */
void
spiel_speaker_speak (SpielSpeaker *self, SpielUtterance *utterance)
{
}

/**
 * spiel_speaker_pause:
 * @self: a #SpielSpeaker
 *
 * Pause the given speaker. If an utterance is being spoken, it will pause
 * until [method@Speaker.resume] is called.
 * If the speaker isn't speaking, calling [method@Speaker.speak] will store
 * new utterances in a queue until [method@Speaker.resume] is called.
 */
void
spiel_speaker_pause (SpielSpeaker *self)
{
}

/**
 * spiel_speaker_resume:
 * @self: a #SpielSpeaker
 *
 * Resumes the given paused speaker. If the speaker isn't pause this will do
 * nothing.
 */
void
spiel_speaker_resume (SpielSpeaker *self)
{
}

/**
 * spiel_speaker_cancel:
 * @self: a #SpielSpeaker
 *
 * Stops the current utterance from being spoken and dumps the utterance queue.
 */
void
spiel_speaker_cancel (SpielSpeaker *self)
{
}

static gboolean
handle_provider_died (SpielProvider *provider,
                      const char *provider_name,
                      gpointer user_data)
{
  return TRUE;
}
