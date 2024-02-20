/* speech-provider.h
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

#include <glib.h>

G_BEGIN_DECLS

#define LIBSPEECHPROVIDER_INSIDE
#include "speech-provider-common.h"
#include "speech-provider-stream-reader.h"
#include "speech-provider-stream-writer.h"
#undef LIBSPEECHPROVIDER_INSIDE

G_END_DECLS
