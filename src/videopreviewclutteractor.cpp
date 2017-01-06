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

#include "videopreviewclutteractor.h"

#include <gtk/gtk.h>

#include <math.h>

struct _VideoPreviewClutterActor
{
    ClutterActor parent;
};

struct _VideoPreviewClutterActorClass
{
    ClutterActorClass parent_class;
};

typedef struct _VideoPreviewClutterActorPrivate VideoPreviewClutterActorPrivate;

typedef enum {
        OUTSIDE,
        INSIDE,
        TOP,
        TOP_LEFT,
        TOP_RIGHT,
        BOTTOM,
        BOTTOM_LEFT,
        BOTTOM_RIGHT,
        LEFT,
        RIGHT
} Location;

typedef enum {
        BELOW,
        LOWER,
        BETWEEN,
        UPPER,
        ABOVE
} Range;

struct _VideoPreviewClutterActorPrivate
{
    GtkClutterEmbed         *embed;
    ClutterActor            *stage;
    GdkCursorType            current_cursor;
    gfloat                   last_press_x;
    gfloat                   last_press_y;
    Location                 active_region;
    gfloat                   aspect;
    ClutterActorBox          desired_box;
    gboolean                 has_content;
    gfloat                   last_x_pos_ratio;
    glfoat                   last_y_pos_ratio;
};

enum
{
    PROP_CLUTTER_EMBED = 1,
    N_PROPERTIES
};

G_DEFINE_TYPE_WITH_PRIVATE(VideoPreviewClutterActor, video_preview_clutter_actor, CLUTTER_TYPE_ACTOR);

#define VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE, VideoPreviewClutterActorPrivate))

static void
video_preview_clutter_actor_dispose(GObject *object)
{
    G_OBJECT_CLASS(video_preview_clutter_actor_parent_class)->dispose(object);
}

static constexpr int MIN_SIZE = 150;
static constexpr int INITIAL_MARGIN = 50;
static constexpr int BORDER_TOLERANCE = 12;
static constexpr int OPACITY_DEFAULT = 255; /* out of 255 */

static Range
find_range (gfloat x,
            gfloat min,
            gfloat max)
{
        if (x < min - BORDER_TOLERANCE)
                return BELOW;
        if (x <= min + BORDER_TOLERANCE)
                return LOWER;
        if (x < max - BORDER_TOLERANCE)
                return BETWEEN;
        if (x <= max + BORDER_TOLERANCE)
                return UPPER;
        return ABOVE;
}

static Location
find_location (ClutterActorBox *box,
               gfloat          x,
               gfloat          y)
{
        Range x_range, y_range;
        Location location[5][5] = {
                { OUTSIDE, OUTSIDE,     OUTSIDE, OUTSIDE,      OUTSIDE },
                { OUTSIDE, TOP_LEFT,    TOP,     TOP_RIGHT,    OUTSIDE },
                { OUTSIDE, LEFT,        INSIDE,  RIGHT,        OUTSIDE },
                { OUTSIDE, BOTTOM_LEFT, BOTTOM,  BOTTOM_RIGHT, OUTSIDE },
                { OUTSIDE, OUTSIDE,     OUTSIDE, OUTSIDE,      OUTSIDE }
        };

        x_range = find_range (x, box->x1, box->x2);
        y_range = find_range (y, box->y1, box->y2);

        return location[y_range][x_range];
}

static int
eval_radial_line (gfloat center_x, gfloat center_y,
                  gfloat bounds_x, gfloat bounds_y,
                  gfloat user_x)
{
        gfloat decision_slope;
        gfloat decision_intercept;

        decision_slope = (bounds_y - center_y) / (bounds_x - center_x);
        decision_intercept = -(decision_slope * bounds_x);

        return (int) (decision_slope * user_x + decision_intercept);
}

static void
update_cursor(gfloat x, gfloat y, ClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    GdkCursorType cursor_type;
    gint region;

    region = priv->active_region;
    if (region == OUTSIDE) {
        ClutterActorBox actor_box;
        clutter_actor_get_allocation_box(self, &actor_box);
        region = find_location(&actor_box, x, y);
    }

    switch (region) {
    case OUTSIDE:
        cursor_type = GDK_LEFT_PTR;
        break;
    case TOP_LEFT:
        cursor_type = GDK_TOP_LEFT_CORNER;
        break;
    case TOP:
        cursor_type = GDK_TOP_SIDE;
        break;
    case TOP_RIGHT:
        cursor_type = GDK_TOP_RIGHT_CORNER;
        break;
    case LEFT:
        cursor_type = GDK_LEFT_SIDE;
        break;
    case INSIDE:
        cursor_type = GDK_FLEUR;
        break;
    case RIGHT:
        cursor_type = GDK_RIGHT_SIDE;
        break;
    case BOTTOM_LEFT:
        cursor_type = GDK_BOTTOM_LEFT_CORNER;
        break;
    case BOTTOM:
        cursor_type = GDK_BOTTOM_SIDE;
        break;
    case BOTTOM_RIGHT:
        cursor_type = GDK_BOTTOM_RIGHT_CORNER;
        break;
    default:
        g_assert_not_reached ();
    }

    if (cursor_type != priv->current_cursor) {
        GdkCursor *cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(priv->embed)),
                                                       cursor_type);
        gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(priv->embed)), cursor);
        g_object_unref(cursor);
        priv->current_cursor = cursor_type;
    }
}

static gboolean
button_press(ClutterActor *stage, ClutterButtonEvent *event, ClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    if (priv->has_content) {
        priv->last_press_x = event->x;
        priv->last_press_y = event->y;

        ClutterActorBox actor_box;
        clutter_actor_get_allocation_box(self, &actor_box);
        priv->active_region = find_location(&actor_box, event->x, event->y);
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean
button_release(ClutterActor *stage, ClutterButtonEvent *event, ClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    priv->last_press_x = -1;
    priv->last_press_y = -1;
    priv->active_region = OUTSIDE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
motion_event(ClutterActor *stage, ClutterMotionEvent *event, ClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    // do nothing if there is no content
    if (!priv->has_content) return GDK_EVENT_PROPAGATE;

    update_cursor(event->x, event->y, self);

    gfloat stage_width, stage_height;
    clutter_actor_get_size(stage, &stage_width, &stage_height);

    ClutterActorBox actor_box;
    clutter_actor_get_allocation_box(self, &actor_box);

    gfloat x = event->x;
    gfloat y = event->y;

    gfloat delta_x = event->x - priv->last_press_x;
    gfloat delta_y = event->y - priv->last_press_y;
    priv->last_press_x = event->x;
    priv->last_press_y = event->y;

    gfloat left = actor_box.x1;
    gfloat right = actor_box.x2;
    gfloat top = actor_box.y1;
    gfloat bottom = actor_box.y2;

    gfloat center_x = (left + right) / 2.0;
    gfloat center_y = (top + bottom) / 2.0;

    gfloat new_width, new_height, width, height, adj_width, adj_height;

    switch (priv->active_region) {
    case INSIDE:
        width = right - left;
        height = bottom - top;

        left += delta_x;
        right += delta_x;
        top += delta_y;
        bottom += delta_y;

        if (left < 0)
                left = 0;
        if (top < 0)
                top = 0;
        if (right > stage_width)
                right = stage_width;
        if (bottom > stage_height)
                bottom = stage_height;

        adj_width = right - left;
        adj_height = bottom - top;
        if (adj_width != width) {
                if (delta_x < 0)
                        right = left + width;
                else
                        left = right - width;
        }
        if (adj_height != height) {
                if (delta_y < 0)
                        bottom = top + height;
                else
                        top = bottom - height;
        }

        break;

    case TOP_LEFT:
        if (priv->aspect < 0) {
                top = y;
                left = x;
        }
        else if (y < eval_radial_line (center_x, center_y, left, top, x)) {
                top = y;
                new_width = (bottom - top) * priv->aspect;
                left = right - new_width;
        }
        else {
                left = x;
                new_height = (right - left) / priv->aspect;
                top = bottom - new_height;
        }
        break;

    case TOP:
        top = y;
        if (priv->aspect > 0) {
                new_width = (bottom - top) * priv->aspect;
                right = left + new_width;
        }
        break;

    case TOP_RIGHT:
        if (priv->aspect < 0) {
                top = y;
                right = x;
        }
        else if (y < eval_radial_line (center_x, center_y, right, top, x)) {
                top = y;
                new_width = (bottom - top) * priv->aspect;
                right = left + new_width;
        }
        else {
                right = x;
                new_height = (right - left) / priv->aspect;
                top = bottom - new_height;
        }
        break;

    case LEFT:
        left = x;
        if (priv->aspect > 0) {
                new_height = (right - left) / priv->aspect;
                bottom = top + new_height;
        }
        break;

    case BOTTOM_LEFT:
        if (priv->aspect < 0) {
                bottom = y;
                left = x;
        }
        else if (y < eval_radial_line (center_x, center_y, left, bottom, x)) {
                left = x;
                new_height = (right - left) / priv->aspect;
                bottom = top + new_height;
        }
        else {
                bottom = y;
                new_width = (bottom - top) * priv->aspect;
                left = right - new_width;
        }
        break;

    case RIGHT:
        right = x;
        if (priv->aspect > 0) {
                new_height = (right - left) / priv->aspect;
                bottom = top + new_height;
        }
        break;

    case BOTTOM_RIGHT:
        if (priv->aspect < 0) {
                bottom = y;
                right = x;
        }
        else if (y < eval_radial_line (center_x, center_y, right, bottom, x)) {
                right = x;
                new_height = (right - left) / priv->aspect;
                bottom = top + new_height;
        }
        else {
                bottom = y;
                new_width = (bottom - top) * priv->aspect;
                right = left + new_width;
        }
        break;

    case BOTTOM:
        bottom = y;
        if (priv->aspect > 0) {
                new_width = (bottom - top) * priv->aspect;
                right= left + new_width;
        }
        break;

    default:
        return FALSE;
    }

    gfloat min_width = MIN_SIZE;
    gfloat min_height = MIN_SIZE;

    width = right - left;
    height = bottom - top;
    if (priv->aspect < 0) {
        if (left < 0)
                left = 0;
        if (top < 0)
                top = 0;
        if (right > stage_width)
                right = stage_width;
        if (bottom > stage_height)
                bottom = stage_height;

        width = right - left;
        height = bottom - top;

        switch (priv->active_region) {
        case LEFT:
        case TOP_LEFT:
        case BOTTOM_LEFT:
            if (width < min_width)
                    left = right - min_width;
            break;
        case RIGHT:
        case TOP_RIGHT:
        case BOTTOM_RIGHT:
            if (width < min_width)
                    right = left + min_width;
            break;
        case INSIDE:
        case OUTSIDE:
        default: ;
        }

        switch (priv->active_region) {
        case TOP:
        case TOP_LEFT:
        case TOP_RIGHT:
            if (height < min_height)
                    top = bottom - min_height;
            break;
        case BOTTOM:
        case BOTTOM_LEFT:
        case BOTTOM_RIGHT:
            if (height < min_height)
                    bottom = top + min_height;
            break;
        case INSIDE:
        case OUTSIDE:
        default: ;
        }
    }
    else {
        if (left < 0 || top < 0 ||
            right > stage_width || bottom > stage_height ||
            width < min_width && height < min_height) {
                left = actor_box.x1;
                right = actor_box.x2;
                top = actor_box.y1;
                bottom = actor_box.y2;

        }
    }

    priv->desired_box.x1 = left;
    priv->desired_box.y1 = top;
    priv->desired_box.x2 = right;
    priv->desired_box.y2 = bottom;

    clutter_actor_queue_relayout(self);

    return GDK_EVENT_PROPAGATE;
}

static void
stage_allocation_changed(VideoPreviewClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    /* we need to make sure that the relative position of the our actor stays the same as the stage
     * which it is located in is resized; we also don't want our actor to become bigger than the
     * stage */

    ClutterActorBox stage_box;
    clutter_actor_get_allocation_box(priv->stage, &stage_box);

    // position
    gfloat x_center = stage_box.x2 * priv->last_x_pos_ratio;
    gfloat y_center = stage_box.y2 * priv->last_y_pos_ratio;


}

static void
set_embed(VideoPreviewClutterActor *self, GtkClutterEmbed *embed)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    priv->embed = embed;
    priv->stage = gtk_clutter_embed_get_stage(priv->embed);

    // set initial size and position to be the bottom right corner, the min size, with a margin
    ClutterActorBox stage_box;
    clutter_actor_get_allocation_box(priv->stage, &stage_box);

    priv->desired_box.x1 = stage_box.x2 - MIN_SIZE - INITIAL_MARGIN;
    priv->desired_box.x2 = priv->desired_box.x1 + MIN_SIZE;
    priv->desired_box.y1 = stage_box.y2 - MIN_SIZE - INITIAL_MARGIN;
    priv->desired_box.y2 = priv->desired_box.y1 + MIN_SIZE;

    clutter_actor_queue_relayout(CLUTTER_ACTOR(self));

    g_signal_connect(priv->stage, "motion-event", G_CALLBACK(motion_event), self);
    g_signal_connect(priv->stage, "button-press-event", G_CALLBACK(button_press), self);
    g_signal_connect(priv->stage, "button-release-event", G_CALLBACK(button_release), self);
    g_signal_connect_swapped(priv->stage, "notify::allocation", G_CALLBACK(stage_allocation_changed), self);
}

static void
content_changed(VideoPreviewClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    // first assume no content
    priv->has_content = FALSE;

    if (auto content = clutter_actor_get_content(CLUTTER_ACTOR(self))) {
        gfloat width, height;
        if (clutter_content_get_preferred_size(content, &width, &height)) {
            if (width > 0 && height > 0) {
                priv->has_content = TRUE;
                gfloat aspect = width / height;
                if (aspect != priv->aspect) {
                    // need to adjust size, make sure its not bigger than current size
                    priv->aspect = aspect;
                    if (aspect > 1.0) {
                        // reduce height
                        gfloat new_height = (priv->desired_box.y2 - priv->desired_box.y1) / aspect;
                        priv->desired_box.y2 = priv->desired_box.y1 + new_height;
                    } else {
                        // reduce width
                        gfloat new_width = (priv->desired_box.x2 - priv->desired_box.x1) * aspect;
                        priv->desired_box.x2 = priv->desired_box.x1 + new_width;
                    }

                    clutter_actor_queue_relayout(CLUTTER_ACTOR(self));
                }
            }
        }
    }

}

static void
video_preview_clutter_actor_allocate(ClutterActor *self,
                                     const ClutterActorBox  *box,
                                     ClutterAllocationFlags  flags)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    if (priv->desired_box.x1 < 0 || priv->desired_box.y1 < 0 || priv->desired_box.x2 < 0 || priv->desired_box.y2 < 0 ) {
        priv->desired_box.x1 = box->x1;
        priv->desired_box.y1 = box->y1;
        priv->desired_box.x2 = box->x2;
        priv->desired_box.y2 = box->y2;
    }

    if (priv->stage) {
        ClutterActorBox stage_box;
        clutter_actor_get_allocation_box(priv->stage, &stage_box);

        gfloat center_x = (priv->desired_box.x2 - priv->desired_box.x1) / 2.0 + priv->desired_box.x1;
        gfloat center_y = (priv->desired_box.y2 - priv->desired_box.y1) / 2.0 + priv->desired_box.y1;

        priv->last_x_pos_ratio = center_x / stage_box.x2;
        priv->last_y_pos_ratio = center_y / stage_box.y2;
    }

    CLUTTER_ACTOR_CLASS(video_preview_clutter_actor_parent_class)->allocate(self, &priv->desired_box, flags);
}

static void
video_preview_clutter_actor_set_property(GObject      *object,
                                         guint         property_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
    switch (property_id) {
        case PROP_CLUTTER_EMBED:
            set_embed(VIDEO_PREVIEW_CLUTTER_ACTOR(object), GTK_CLUTTER_EMBED(g_value_get_object(value)));
            break;

        default:
            /* We don't have any other property... */
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
video_preview_clutter_actor_get_property(GObject    *object,
                                         guint       property_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(object);

    switch (property_id) {
        case PROP_CLUTTER_EMBED:
            g_value_set_object(value, priv->embed);
            break;
        default:
            /* We don't have any other property... */
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
          break;
    }
}

static void
video_preview_clutter_actor_init(VideoPreviewClutterActor *self)
{
    auto priv = VIDEO_PREVIEW_CLUTTER_ACTOR_GET_PRIVATE(self);

    priv->aspect = 1.0;
    priv->active_region = OUTSIDE;

    priv->desired_box.x1 = -1;
    priv->desired_box.y1 = -1;
    priv->desired_box.x2 = -1;
    priv->desired_box.y2 = -1;

    /* allow for actor to be moved and resized */
    clutter_actor_set_reactive(CLUTTER_ACTOR(self), TRUE);
    clutter_actor_set_opacity(CLUTTER_ACTOR(self), OPACITY_DEFAULT);

    g_signal_connect(self, "notify::content", G_CALLBACK(content_changed), self);
}

static void
video_preview_clutter_actor_class_init(VideoPreviewClutterActorClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = video_preview_clutter_actor_dispose;
    G_OBJECT_CLASS(klass)->set_property = video_preview_clutter_actor_set_property;
    G_OBJECT_CLASS(klass)->get_property = video_preview_clutter_actor_get_property;
    CLUTTER_ACTOR_CLASS(klass)->allocate = video_preview_clutter_actor_allocate;
    CLUTTER_ACTOR_CLASS(klass)->allocate = video_preview_clutter_actor_allocate;

    g_object_class_install_property(
        G_OBJECT_CLASS(klass),
        PROP_CLUTTER_EMBED,
        g_param_spec_object("clutter-embed",
            "GtkClutterEmbed Widget",
            "The GtkClutterEmbed widget inside of whose stage this actor is",
            GTK_CLUTTER_TYPE_EMBED,
            GParamFlags(G_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY)));
}

ClutterActor *
video_preview_clutter_actor_new(GtkClutterEmbed *embed)
{
    g_debug("creating new clutter actor");
    return CLUTTER_ACTOR(g_object_new(VIDEO_PREVIEW_CLUTTER_ACTOR_TYPE, "clutter-embed", embed, NULL));
}
