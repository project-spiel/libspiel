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

#ifndef __SPIEL_PROVIDER_SRC_H__
#define __SPIEL_PROVIDER_SRC_H__

#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>
#include <spiel-provider.h>

G_BEGIN_DECLS

#define SPIEL_TYPE_PROVIDER_SRC (spiel_provider_src_get_type ())
#define SPIEL_PROVIDER_SRC(obj)                                                \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), SPIEL_TYPE_PROVIDER_SRC,                 \
                               SpielProviderSrc))
#define SPIEL_PROVIDER_SRC_CLASS(klass)                                        \
  (G_TYPE_CHECK_CLASS_CAST ((klass), SPIEL_TYPE_PROVIDER_SRC,                  \
                            SpielProviderSrcClass))
#define GST_IS_FD_SRC(obj)                                                     \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SPIEL_TYPE_PROVIDER_SRC))
#define GST_IS_FD_SRC_CLASS(klass)                                             \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), SPIEL_TYPE_PROVIDER_SRC))

typedef struct _SpielProviderSrc SpielProviderSrc;
typedef struct _SpielProviderSrcClass SpielProviderSrcClass;

/**
 * SpielProviderSrc:
 *
 * Opaque #SpielProviderSrc data structure.
 */
struct _SpielProviderSrc
{
  GstPushSrc element;

  /* fd and flag indicating whether fd is seekable */
  gint fd;

  gulong curoffset; /* current offset in file */

  SpielProviderStreamReader *reader;
};

struct _SpielProviderSrcClass
{
  GstPushSrcClass parent_class;
};

G_GNUC_INTERNAL GType spiel_provider_src_get_type (void);

SpielProviderSrc *spiel_provider_src_new (gint fd);

G_END_DECLS

#endif /* __SPIEL_PROVIDER_SRC_H__ */
