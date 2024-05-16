/* spiel-provider-src.c
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

#include <fcntl.h>
#include <unistd.h>

#include "spiel-provider-src.h"

static GstStaticPadTemplate srctemplate = GST_STATIC_PAD_TEMPLATE (
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS_ANY);

#define DEFAULT_FD 0

enum
{
  PROP_0,

  PROP_FD,

  PROP_LAST
};

#define spiel_provider_src_parent_class parent_class
G_DEFINE_TYPE (SpielProviderSrc, spiel_provider_src, GST_TYPE_PUSH_SRC);

SpielProviderSrc *
spiel_provider_src_new (gint fd)
{
  return g_object_new (SPIEL_TYPE_PROVIDER_SRC, "fd", fd, NULL);
}

static void spiel_provider_src_set_property (GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static void spiel_provider_src_get_property (GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void spiel_provider_src_dispose (GObject *obj);

static gboolean spiel_provider_src_start (GstBaseSrc *bsrc);
static gboolean spiel_provider_src_stop (GstBaseSrc *bsrc);
static gboolean spiel_provider_src_unlock (GstBaseSrc *bsrc);
static gboolean spiel_provider_src_unlock_stop (GstBaseSrc *bsrc);
static gboolean spiel_provider_src_get_size (GstBaseSrc *src, guint64 *size);

static GstFlowReturn spiel_provider_src_create (GstPushSrc *psrc,
                                                GstBuffer **outbuf);

static void
spiel_provider_src_class_init (SpielProviderSrcClass *klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpush_src_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  gstpush_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = spiel_provider_src_set_property;
  gobject_class->get_property = spiel_provider_src_get_property;
  gobject_class->dispose = spiel_provider_src_dispose;

  g_object_class_install_property (
      gobject_class, PROP_FD,
      g_param_spec_int ("fd", "fd", "An open file descriptor to read from", 0,
                        G_MAXINT, DEFAULT_FD,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                            G_PARAM_CONSTRUCT_ONLY));

  gst_element_class_set_static_metadata (
      gstelement_class, "Spiel Provider Source", "Source",
      "Read specialized audio/event chunks from pipe",
      "Eitan Isaacson <eitan@monotonous.org>");
  gst_element_class_add_static_pad_template (gstelement_class, &srctemplate);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (spiel_provider_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (spiel_provider_src_stop);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (spiel_provider_src_unlock);
  gstbasesrc_class->unlock_stop =
      GST_DEBUG_FUNCPTR (spiel_provider_src_unlock_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (spiel_provider_src_get_size);

  gstpush_src_class->create = GST_DEBUG_FUNCPTR (spiel_provider_src_create);
}

static void
spiel_provider_src_init (SpielProviderSrc *spsrc)
{
  spsrc->curoffset = 0;
  spsrc->reader = NULL;
}

static void
spiel_provider_src_dispose (GObject *obj)
{
  SpielProviderSrc *src = SPIEL_PROVIDER_SRC (obj);

  g_clear_object (&src->reader);

  G_OBJECT_CLASS (parent_class)->dispose (obj);
}

static gboolean
spiel_provider_src_start (GstBaseSrc *bsrc)
{
  SpielProviderSrc *src = SPIEL_PROVIDER_SRC (bsrc);
  gboolean got_header =
      speech_provider_stream_reader_get_stream_header (src->reader);
  return got_header;
}

static gboolean
spiel_provider_src_stop (GstBaseSrc *bsrc)
{
  // XXX: Do we need this?
  return TRUE;
}

static gboolean
spiel_provider_src_unlock (GstBaseSrc *bsrc)
{
  // XXX: Do we need this?
  return TRUE;
}

static gboolean
spiel_provider_src_unlock_stop (GstBaseSrc *bsrc)
{
  // XXX: Do we need this?
  return TRUE;
}

static void
spiel_provider_src_set_property (GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
  SpielProviderSrc *src = SPIEL_PROVIDER_SRC (object);

  switch (prop_id)
    {
    case PROP_FD:

      GST_OBJECT_LOCK (object);
      g_assert (src->reader == NULL);
      src->fd = g_value_get_int (value);
      src->reader = speech_provider_stream_reader_new (src->fd);
      GST_OBJECT_UNLOCK (object);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
spiel_provider_src_get_property (GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
  SpielProviderSrc *src = SPIEL_PROVIDER_SRC (object);

  switch (prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, src->fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GstFlowReturn
spiel_provider_src_create (GstPushSrc *psrc, GstBuffer **outbuf)
{
  SpielProviderSrc *src;

  src = SPIEL_PROVIDER_SRC (psrc);

  while (TRUE)
    {
      guint8 *chunk = NULL;
      guint32 chunk_size = 0;
      SpeechProviderEventType event_type = SPEECH_PROVIDER_EVENT_TYPE_NONE;
      guint32 range_start = 0;
      guint32 range_end = 0;
      g_autofree char *mark_name = NULL;
      gboolean got_event, got_audio;
      got_event = speech_provider_stream_reader_get_event (
          src->reader, &event_type, &range_start, &range_end, &mark_name);
      if (got_event)
        {
          gst_element_post_message (
              GST_ELEMENT_CAST (src),
              gst_message_new_element (
                  GST_OBJECT_CAST (src),
                  gst_structure_new ("SpielGoingToSpeak", "event_type",
                                     G_TYPE_UINT, event_type, "range_start",
                                     G_TYPE_UINT, range_start, "range_end",
                                     G_TYPE_UINT, range_end, "mark_name",
                                     G_TYPE_STRING, mark_name, NULL)));
        }

      got_audio = speech_provider_stream_reader_get_audio (src->reader, &chunk,
                                                           &chunk_size);
      if (got_audio && chunk_size > 0)
        {
          GstBuffer *buf = gst_buffer_new_wrapped (chunk, chunk_size);

          GST_BUFFER_OFFSET (buf) = src->curoffset;
          GST_BUFFER_TIMESTAMP (buf) = GST_CLOCK_TIME_NONE;
          src->curoffset += chunk_size;

          *outbuf = buf;

          return GST_FLOW_OK;
        }

      if (!got_audio && !got_event)
        {
          GST_DEBUG_OBJECT (psrc, "Read 0 bytes. EOS.");
          return GST_FLOW_EOS;
        }
    }

  return GST_FLOW_OK;
}

static gboolean
spiel_provider_src_get_size (GstBaseSrc *bsrc, guint64 *size)
{
  // XXX: Get rid of this?
  return FALSE;
}
