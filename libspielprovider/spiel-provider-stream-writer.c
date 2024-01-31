/* spiel-provider-stream-writer.c
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

#include "spiel-provider.h"

#include "spiel-provider-stream-writer.h"

#include <fcntl.h>
#include <unistd.h>

struct _SpielProviderStreamWriter
{
  GObject parent_instance;
};

typedef struct
{
  gint fd;
  gboolean stream_header_sent;
} SpielProviderStreamWriterPrivate;

G_DEFINE_FINAL_TYPE_WITH_PRIVATE (SpielProviderStreamWriter,
                                  spiel_provider_stream_writer,
                                  G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_FD,
  N_PROPS
};

static GParamSpec *properties[N_PROPS];

/**
 * spiel_provider_stream_writer_new: (constructor)
 * @fd: The file descriptor for a pipe
 *
 * Creates a new #SpielProviderStreamWriter.
 *
 * Returns: The new #SpielProviderStreamWriter.
 */
SpielProviderStreamWriter *
spiel_provider_stream_writer_new (gint fd)
{
  if (fcntl (fd, F_GETFD) == -1)
    {
      g_warning ("Bad file descriptor");
      return NULL;
    }

  return g_object_new (SPIEL_PROVIDER_TYPE_STREAM_WRITER, "fd", fd, NULL);
}

/**
 * spiel_provider_stream_writer_close:
 *
 * Close the pipe.
 *
 */
void
spiel_provider_stream_writer_close (SpielProviderStreamWriter *self)
{
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);
  close (priv->fd);
  priv->fd = -1;
}

/**
 * spiel_provider_stream_writer_send_stream_header:
 *
 * Sends initial stream header
 *
 */
void
spiel_provider_stream_writer_send_stream_header (
    SpielProviderStreamWriter *self)
{
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);
  SpielProviderStreamHeader header = {
    .version = SPIEL_PROVIDER_STREAM_PROTOCOL_VERSION
  };
  g_assert (!priv->stream_header_sent);
  write (priv->fd, &header, sizeof (SpielProviderStreamHeader));
  priv->stream_header_sent = TRUE;
}

/**
 * spiel_provider_stream_writer_send_audio:
 * @chunk: (array length=chunk_size): audio data
 * @chunk_size: audio chunk size
 *
 * Sends audio chunk
 *
 */
void
spiel_provider_stream_writer_send_audio (SpielProviderStreamWriter *self,
                                         guint8 *chunk,
                                         guint32 chunk_size)
{
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);
  SpielProviderChunkType chunk_type = SPIEL_PROVIDER_CHUNK_TYPE_AUDIO;

  g_assert (priv->stream_header_sent);

  write (priv->fd, &chunk_type, sizeof (SpielProviderChunkType));
  write (priv->fd, &chunk_size, sizeof (guint32));
  write (priv->fd, chunk, chunk_size);
}

/**
 * spiel_provider_stream_writer_send_event:
 *
 * Sends event
 *
 */
void
spiel_provider_stream_writer_send_event (SpielProviderStreamWriter *self,
                                         SpielProviderEventType event_type,
                                         guint32 range_start,
                                         guint32 range_end,
                                         const char *mark_name)
{
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);
  SpielProviderChunkType chunk_type = SPIEL_PROVIDER_CHUNK_TYPE_EVENT;
  SpielProviderEventData event_data = { .event_type = event_type,
                                        .range_start = range_start,
                                        .range_end = range_end,
                                        .mark_name_length =
                                            g_utf8_strlen (mark_name, -1) };
  g_assert (priv->stream_header_sent);

  write (priv->fd, &chunk_type, sizeof (SpielProviderChunkType));
  write (priv->fd, &event_data, sizeof (SpielProviderEventData));
  if (event_data.mark_name_length)
    {
      write (priv->fd, mark_name, event_data.mark_name_length);
    }
}

static void
spiel_provider_stream_writer_finalize (GObject *object)
{
  SpielProviderStreamWriter *self = (SpielProviderStreamWriter *) object;
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);

  close (priv->fd);
  G_OBJECT_CLASS (spiel_provider_stream_writer_parent_class)->finalize (object);
}

static void
spiel_provider_stream_writer_get_property (GObject *object,
                                           guint prop_id,
                                           GValue *value,
                                           GParamSpec *pspec)
{
  SpielProviderStreamWriter *self = SPIEL_PROVIDER_STREAM_WRITER (object);
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);

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
spiel_provider_stream_writer_set_property (GObject *object,
                                           guint prop_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
  SpielProviderStreamWriter *self = SPIEL_PROVIDER_STREAM_WRITER (object);
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);

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
spiel_provider_stream_writer_class_init (SpielProviderStreamWriterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = spiel_provider_stream_writer_finalize;
  object_class->get_property = spiel_provider_stream_writer_get_property;
  object_class->set_property = spiel_provider_stream_writer_set_property;

  /**
   * SpielProviderStreamWriter:fd:
   *
   * File descriptor for pipe
   *
   */
  properties[PROP_FD] =
      g_param_spec_int ("fd", NULL, NULL, -1, G_MAXINT32, 0,
                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
spiel_provider_stream_writer_init (SpielProviderStreamWriter *self)
{
  SpielProviderStreamWriterPrivate *priv =
      spiel_provider_stream_writer_get_instance_private (self);
  priv->stream_header_sent = FALSE;
}
