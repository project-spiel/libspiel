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

#include "spiel-provider-src.h"
#include "spiel-registry.h"
#include "spiel-voice.h"
#include "spieldbusgenerated.h"
#include <fcntl.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gst/audio/audio.h>
#include <gst/gst.h>

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
  GstElement *pipeline;
  GstElement *convert;
  GstElement *sink;
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
  PROP_SINK,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

typedef struct
{
  SpielUtterance *utterance;
  GstElement *src;
  GstElement *parse;
  GstElement *volume;
  GError *error;
} _QueueEntry;

static void
_queue_entry_destroy (gpointer data)
{
  _QueueEntry *entry = data;
  if (entry)
    {
      g_clear_object (&entry->utterance);
      if (entry->error)
        {
          g_error_free (entry->error);
        }

      if (entry->src)
        {
          gint fd = -1;
          g_object_get (entry->src, "fd", &fd, NULL);
          if (fd > 0)
            {
              close (fd);
            }
          g_clear_object (&entry->src);
        }

      if (entry->parse)
        {
          g_clear_object (&entry->parse);
        }

      if (entry->volume)
        {
          g_clear_object (&entry->volume);
        }
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
  if (priv->pipeline)
    {
      gst_element_set_state (priv->pipeline, GST_STATE_NULL);
    }
  g_clear_object (&priv->pipeline);
  g_clear_object (&priv->convert);
  g_clear_object (&priv->sink);
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
    case PROP_SINK:
      g_value_set_object (value, priv->sink);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
spiel_speaker_set_property (GObject *object,
                            guint prop_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  SpielSpeaker *self = SPIEL_SPEAKER (object);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_SINK:
      {
        GstElement *new_sink = g_value_get_object (value);

        gst_element_unlink (priv->convert, priv->sink);
        gst_bin_remove (GST_BIN (priv->pipeline), priv->sink);
        g_object_unref (priv->sink);

        gst_bin_add (GST_BIN (priv->pipeline), new_sink);
        gst_element_link (priv->convert, new_sink);
        priv->sink = g_object_ref (new_sink);
      }
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
  object_class->set_property = spiel_speaker_set_property;

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

  /**
   * SpielSpeaker:sink:
   *
   * The gstreamer sink this speaker is connected to.
   *
   */
  properties[PROP_SINK] = g_param_spec_object (
      "sink", NULL, NULL, GST_TYPE_ELEMENT, G_PARAM_READWRITE);

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

static gboolean
_handle_gst_eos (GstBus *bus, GstMessage *msg, SpielSpeaker *self);

static gboolean
_handle_gst_state_change (GstBus *bus, GstMessage *msg, SpielSpeaker *self);

static void
_setup_pipeline (SpielSpeaker *self, GError **error)
{
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  GstBus *bus;
  GstElement *convert = gst_element_factory_make ("audioconvert", "convert");
  GstElement *sink = gst_element_factory_make (
      g_getenv ("SPIEL_TEST") ? "fakesink" : "autoaudiosink", "sink");
  priv->pipeline = gst_pipeline_new ("pipeline");

  gst_bin_add_many (GST_BIN (priv->pipeline), convert, sink, NULL);
  gst_element_link (convert, sink);

  bus = gst_element_get_bus (priv->pipeline);

  gst_bus_add_signal_watch (bus);
  g_object_connect (bus, "signal::message::eos", _handle_gst_eos, self,
                    "signal::message::state-changed", _handle_gst_state_change,
                    self, NULL);

  priv->convert = g_object_ref (convert);
  priv->sink = g_object_ref (sink);
}

static void
_on_registry_get (GObject *source, GAsyncResult *result, gpointer user_data)
{
  GTask *task = user_data;
  SpielSpeaker *self = g_task_get_task_data (task);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  GError *error = NULL;

  priv->registry = spiel_registry_get_finish (result, &error);
  if (error == NULL)
    {
      _setup_pipeline (self, &error);
    }
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
  if (*error == NULL)
    {
      _setup_pipeline (self, error);
    }

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

static void _provider_call_synthesize_done (GObject *source_object,
                                            GAsyncResult *res,
                                            gpointer user_data);

static void _speak_current_entry (SpielSpeaker *self);

static void _advance_to_next_entry_or_finish (SpielSpeaker *self,
                                              gboolean canceled);

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
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = g_slice_new0 (_QueueEntry);
  SpielProvider *provider = NULL;
  GUnixFDList *fd_list = g_unix_fd_list_new ();
  int mypipe[2];
  int fd;
  char *text = NULL;
  const char *output_format = NULL;
  const char *stream_type = NULL;
  gdouble pitch, rate, volume;
  SpielVoice *voice = NULL;
  GstStructure *gst_struct;

  g_object_get (utterance, "text", &text, "pitch", &pitch, "rate", &rate,
                "volume", &volume, "voice", &voice, NULL);

  if (voice == NULL)
    {
      voice =
          spiel_registry_get_voice_for_utterance (priv->registry, utterance);
      spiel_utterance_set_voice (utterance, voice);
      g_object_ref (voice);
    }

  provider = spiel_registry_get_provider_for_voice (priv->registry, voice);

  // XXX: Emit error on failure
  g_unix_open_pipe (mypipe, 0, NULL);
  fd = g_unix_fd_list_append (fd_list, mypipe[1], NULL);

  // XXX: Emit error on failure
  close (mypipe[1]);

  spiel_provider_call_synthesize (
      provider, g_variant_new_handle (fd), text,
      voice ? spiel_voice_get_identifier (voice) : "", pitch, rate,
      G_DBUS_CALL_FLAGS_NONE, -1, fd_list, NULL, _provider_call_synthesize_done,
      self);

  g_object_unref (fd_list);
  g_free (text);
  g_object_unref (voice);

  entry->utterance = g_object_ref (utterance);

  output_format = spiel_voice_get_output_format (voice);

  gst_struct = gst_structure_from_string (output_format, NULL);

  stream_type = gst_struct ? gst_structure_get_name (gst_struct) : NULL;

  if (g_str_equal (stream_type, "audio/x-raw"))
    {
      entry->src =
          gst_element_factory_make_full ("fdsrc", "fd", mypipe[0], NULL);
    }
  else if (g_str_equal (stream_type, "audio/x-spiel"))
    {
      entry->src = GST_ELEMENT (spiel_provider_src_new (mypipe[0]));
    }

  if (!entry->src)
    {
      g_assert (!entry->error);
      g_set_error (&entry->error, SPIEL_ERROR, SPIEL_ERROR_MISCONFIGURED_VOICE,
                   "Voice output format not set correctly: '%s'",
                   output_format);
    }
  else
    {
      gint sample_rate, channels;
      const char *pcm_format;

      entry->volume =
          gst_element_factory_make_full ("volume", "volume", volume, NULL);

      entry->parse = gst_element_factory_make ("rawaudioparse", "parse");
      if (gst_structure_get_int (gst_struct, "rate", &sample_rate))
        {
          g_object_set (entry->parse, "sample-rate", sample_rate, NULL);
        }

      if (gst_structure_get_int (gst_struct, "channels", &channels))
        {
          g_object_set (entry->parse, "num-channels", channels, NULL);
        }

      pcm_format = gst_structure_get_string (gst_struct, "format");
      if (pcm_format)
        {
          g_object_set (entry->parse, "pcm-format",
                        gst_audio_format_from_string (pcm_format), NULL);
        }
    }

  gst_structure_free (gst_struct);

  priv->queue = g_slist_append (priv->queue, entry);

  if (!priv->queue->next)
    {
      _speak_current_entry (self);
    }
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
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;
  if (priv->paused)
    {
      return;
    }

  if (!entry)
    {
      priv->paused = TRUE;
      g_object_notify (G_OBJECT (self), "paused");
      return;
    }

  gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
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
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;
  if (!priv->paused)
    {
      return;
    }

  if (!entry)
    {
      priv->paused = FALSE;
      g_object_notify (G_OBJECT (self), "paused");
      return;
    }

  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
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
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  if (!priv->queue)
    {
      return;
    }

  // Dump the rest of the queue after the current entry.
  g_slist_free_full (g_steal_pointer (&priv->queue->next),
                     (GDestroyNotify) _queue_entry_destroy);

  _advance_to_next_entry_or_finish (self, TRUE);
}

static gboolean
handle_provider_died (SpielProvider *provider,
                      const char *provider_name,
                      gpointer user_data)
{
  SpielSpeaker *self = SPIEL_SPEAKER (user_data);
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;

  if (entry != NULL)
    {
      SpielVoice *voice = spiel_utterance_get_voice (entry->utterance);
      g_assert (voice);
      if (g_str_equal (provider_name, spiel_voice_get_provider_name (voice)))
        {
          g_assert (!entry->error);
          g_set_error (&entry->error, SPIEL_ERROR,
                       SPIEL_ERROR_PROVIDER_UNEXPECTEDLY_DIED,
                       "Provider unexpectedly died: %s", provider_name);
          _advance_to_next_entry_or_finish (self, FALSE);
        }
    }

  return TRUE;
}

static gboolean
_handle_gst_state_change (GstBus *bus, GstMessage *msg, SpielSpeaker *self)
{
  GstState old_state, new_state, pending_state;
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;
  GstObject *element = GST_MESSAGE_SRC (msg);

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

  if (new_state == GST_STATE_PLAYING &&
      pending_state == GST_STATE_VOID_PENDING &&
      element == GST_OBJECT (priv->pipeline))
    {
      if (!priv->paused)
        {
          if (!priv->speaking)
            {
              priv->speaking = TRUE;
              g_object_notify (G_OBJECT (self), "speaking");
            }
          g_signal_emit (self, speaker_signals[UTTURANCE_STARTED], 0,
                         entry->utterance);
        }
      else
        {
          priv->paused = FALSE;
          g_object_notify (G_OBJECT (self), "paused");
        }
    }
  if (new_state == GST_STATE_PAUSED &&
      pending_state == GST_STATE_VOID_PENDING &&
      element == GST_OBJECT (priv->pipeline))
    {
      priv->paused = TRUE;
      g_object_notify (G_OBJECT (self), "paused");
    }

  if (new_state == GST_STATE_NULL && pending_state == GST_STATE_VOID_PENDING &&
      element == GST_OBJECT (entry->src))
    {
      _advance_to_next_entry_or_finish (self, FALSE);
    }

  return TRUE;
}

static gboolean
_handle_gst_eos (GstBus *bus, GstMessage *msg, SpielSpeaker *self)
{
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;
  GstElement *fdsrc = entry->src;
  gst_element_set_state (fdsrc, GST_STATE_NULL);

  return TRUE;
}

static void
_provider_call_synthesize_done (GObject *source_object,
                                GAsyncResult *res,
                                gpointer user_data)
{
  SpielProvider *provider = SPIEL_PROVIDER (source_object);
  GError *err = NULL;
  spiel_provider_call_synthesize_finish (provider, NULL, res, &err);
  if (err != NULL)
    {
      // XXX: Emit error on associated utterance
      g_warning ("Synthesis error: %s", err->message);
      g_error_free (err);
      return;
    }
}

static void
_speak_current_entry (SpielSpeaker *self)
{
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;

  g_assert (entry);

  if (entry->error)
    {
      g_assert (!entry->src);
      g_assert (!entry->parse);
      g_assert (!entry->volume);
      _advance_to_next_entry_or_finish (self, FALSE);
      return;
    }

  gst_bin_add_many (GST_BIN (priv->pipeline), g_object_ref (entry->src),
                    g_object_ref (entry->parse), g_object_ref (entry->volume),
                    NULL);

  gst_element_link_many (entry->src, entry->parse, entry->volume, priv->convert,
                         NULL);

  gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
}

static void
_advance_to_next_entry_or_finish (SpielSpeaker *self, gboolean canceled)
{
  SpielSpeakerPrivate *priv = spiel_speaker_get_instance_private (self);
  _QueueEntry *entry = priv->queue ? priv->queue->data : NULL;

  g_assert (entry);

  gst_element_set_state (priv->pipeline, GST_STATE_NULL);

  if (entry->error)
    {
      g_signal_emit (self, speaker_signals[UTTERANCE_ERROR], 0,
                     entry->utterance, entry->error);
    }
  else
    {
      g_signal_emit (self,
                     canceled ? speaker_signals[UTTERANCE_CANCELED]
                              : speaker_signals[UTTERANCE_FINISHED],
                     0, entry->utterance);
    }

  if (entry->src)
    {
      g_assert (entry->parse);
      g_assert (entry->volume);
      gst_bin_remove_many (GST_BIN (priv->pipeline), entry->src, entry->parse,
                           entry->volume, NULL);
    }

  if (!priv->queue->next)
    {
      priv->speaking = FALSE;
      g_object_notify (G_OBJECT (self), "speaking");
    }

  _queue_entry_destroy (entry);
  priv->queue = g_slist_delete_link (priv->queue, priv->queue);
  if (priv->queue)
    {
      _speak_current_entry (self);
    }
}
