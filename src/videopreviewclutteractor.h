/*
 *  Copyright (C) 2017 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#pragma once

#include <clutter-gtk/clutter-gtk.h>

G_BEGIN_DECLS

#define VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE            (video_preview_clutter_actor_get_type ())
#define VIDEO_PREVIEW_CLUTTER_ACTOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE, VideoPreviewClutterActor))
#define VIDEO_PREVIEW_CLUTTER_ACTOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE, VideoPreviewClutterActorClass))
#define IS_VIDEO_PREVIEW_CLUTTER_ACTOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE))
#define IS_VIDEO_PREVIEW_CLUTTER_ACTOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE))

typedef struct _VideoPreviewClutterActor      VideoPreviewClutterActor;
typedef struct _VideoPreviewClutterActorClass VideoPreviewClutterActorClass;

GType         video_preview_clutter_actor_get_type      (void) G_GNUC_CONST;
ClutterActor *video_preview_clutter_actor_new           (GtkClutterEmbed *embed);

G_END_DECLS
