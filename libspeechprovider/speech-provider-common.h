/* speech-provider-common.h
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

#pragma once

G_BEGIN_DECLS

#define SPEECH_PROVIDER_STREAM_PROTOCOL_VERSION "0.01"

typedef struct __attribute__ ((__packed__))
{
  char version[4];
} SpeechProviderStreamHeader;

typedef enum __attribute__ ((__packed__))
{
  SPEECH_PROVIDER_CHUNK_TYPE_NONE,
  SPEECH_PROVIDER_CHUNK_TYPE_AUDIO,
  SPEECH_PROVIDER_CHUNK_TYPE_EVENT,
} SpeechProviderChunkType;

typedef enum __attribute__ ((__packed__))
{
  SPEECH_PROVIDER_EVENT_TYPE_NONE,
  SPEECH_PROVIDER_EVENT_TYPE_WORD,
  SPEECH_PROVIDER_EVENT_TYPE_SENTENCE,
  SPEECH_PROVIDER_EVENT_TYPE_RANGE,
  SPEECH_PROVIDER_EVENT_TYPE_MARK,
} SpeechProviderEventType;

typedef struct __attribute__ ((__packed__))
{
  guint8 event_type;
  guint32 range_start;
  guint32 range_end;
  guint32 mark_name_length;
} SpeechProviderEventData;

G_END_DECLS
