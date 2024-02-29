/* speech-provider-stream-writer.c
 *
 * Copyright (C) 2024 Eitan Isaacson <eitan@monotonous.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "speech-provider.h"

#include "speech-provider-stream-writer.h"

#include <fcntl.h>
#include <unistd.h>

/**
 * SpeechProviderStreamWriter:
 *
 * A provider audio stream writer.
 *
 * Since: 1.0
 */
struct _SpeechProviderStreamWriter
{
  GObject parent_instance;
};

typedef struct
{
  gint fd;
  gboolean stream_header_sent;
} SpeechProviderStreamWriterPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpeechProviderStreamWriter,
                                  speech_provider_stream_writer,
                                  G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_FD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * speech_provider_stream_writer_new: (constructor)
 * @fd: The file descriptor for a pipe
 *
 * Creates a new [class@SpeechProvider.StreamWriter].
 *
 * Returns: (transfer full): The new `SpeechProviderStreamWriter`
 *
 * Since: 1.0
 */
SpeechProviderStreamWriter *
speech_provider_stream_writer_new (gint fd)
{
  if (fcntl (fd, F_GETFD) == -1)
    {
      g_warning ("Bad file descriptor");
      return NULL;
    }

  return g_object_new (SPEECH_PROVIDER_TYPE_STREAM_WRITER, "fd", fd, NULL);
}

/**
 * speech_provider_stream_writer_close:
 * @self: a `SpeechProviderStreamWriter`
 *
 * Close the writer.
 *
 * Since: 1.0
 */
void
speech_provider_stream_writer_close (SpeechProviderStreamWriter *self)
{
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);

  g_return_if_fail (SPEECH_PROVIDER_IS_STREAM_WRITER (self));

  close (priv->fd);
  priv->fd = -1;
}

/**
 * speech_provider_stream_writer_send_stream_header:
 * @self: a `SpeechProviderStreamWriter`
 *
 * Sends the initial stream header.
 *
 * Since: 1.0
 */
void
speech_provider_stream_writer_send_stream_header (
    SpeechProviderStreamWriter *self)
{
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);
  SpeechProviderStreamHeader header = {
    .version = SPEECH_PROVIDER_STREAM_PROTOCOL_VERSION
  };

  g_return_if_fail (SPEECH_PROVIDER_IS_STREAM_WRITER (self));

  g_assert (!priv->stream_header_sent);
  write (priv->fd, &header, sizeof (SpeechProviderStreamHeader));
  priv->stream_header_sent = TRUE;
}

/**
 * speech_provider_stream_writer_send_audio:
 * @self: a `SpeechProviderStreamWriter`
 * @chunk: (array length=chunk_size) (not nullable): audio data
 * @chunk_size: audio chunk size
 *
 * Sends a chunk of audio data.
 *
 * Since: 1.0
 */
void
speech_provider_stream_writer_send_audio (SpeechProviderStreamWriter *self,
                                          guint8 *chunk,
                                          guint32 chunk_size)
{
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);
  SpeechProviderChunkType chunk_type = SPEECH_PROVIDER_CHUNK_TYPE_AUDIO;

  g_return_if_fail (SPEECH_PROVIDER_IS_STREAM_WRITER (self));
  g_return_if_fail (chunk != NULL);
  g_assert (priv->stream_header_sent);

  write (priv->fd, &chunk_type, sizeof (SpeechProviderChunkType));
  write (priv->fd, &chunk_size, sizeof (guint32));
  write (priv->fd, chunk, chunk_size);
}

/**
 * speech_provider_stream_writer_send_event:
 * @self: a `SpeechProviderStreamWriter`
 *
 * Sends an event.
 *
 * Since: 1.0
 */
void
speech_provider_stream_writer_send_event (SpeechProviderStreamWriter *self,
                                          SpeechProviderEventType event_type,
                                          guint32 range_start,
                                          guint32 range_end,
                                          const char *mark_name)
{
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);
  SpeechProviderChunkType chunk_type = SPEECH_PROVIDER_CHUNK_TYPE_EVENT;
  SpeechProviderEventData event_data = { .event_type = event_type,
                                         .range_start = range_start,
                                         .range_end = range_end };

  g_return_if_fail (SPEECH_PROVIDER_IS_STREAM_WRITER (self));
  g_return_if_fail (mark_name != NULL);
  g_assert (priv->stream_header_sent);

  event_data.mark_name_length = g_utf8_strlen (mark_name, -1);
  write (priv->fd, &chunk_type, sizeof (SpeechProviderChunkType));
  write (priv->fd, &event_data, sizeof (SpeechProviderEventData));
  if (event_data.mark_name_length)
    {
      write (priv->fd, mark_name, event_data.mark_name_length);
    }
}

static void
speech_provider_stream_writer_finalize (GObject *object)
{
  SpeechProviderStreamWriter *self = (SpeechProviderStreamWriter *) object;
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);

  close (priv->fd);
  G_OBJECT_CLASS (speech_provider_stream_writer_parent_class)
      ->finalize (object);
}

static void
speech_provider_stream_writer_get_property (GObject *object,
                                            guint prop_id,
                                            GValue *value,
                                            GParamSpec *pspec)
{
  SpeechProviderStreamWriter *self = SPEECH_PROVIDER_STREAM_WRITER (object);
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, priv->fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
speech_provider_stream_writer_set_property (GObject *object,
                                            guint prop_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
  SpeechProviderStreamWriter *self = SPEECH_PROVIDER_STREAM_WRITER (object);
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_FD:
      priv->fd = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
speech_provider_stream_writer_class_init (
    SpeechProviderStreamWriterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = speech_provider_stream_writer_finalize;
  object_class->get_property = speech_provider_stream_writer_get_property;
  object_class->set_property = speech_provider_stream_writer_set_property;

  /**
   * SpeechProviderStreamWriter:fd:
   *
   * File descriptor for the stream.
   *
   * Since: 1.0
   */
  properties[PROP_FD] =
      g_param_spec_int ("fd", NULL, NULL, -1, G_MAXINT32, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
speech_provider_stream_writer_init (SpeechProviderStreamWriter *self)
{
  SpeechProviderStreamWriterPrivate *priv =
      speech_provider_stream_writer_get_instance_private (self);
  priv->stream_header_sent = FALSE;
}
