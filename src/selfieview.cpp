/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#include "selfieview.h"

/* LRC */
#include <video/configurationproxy.h>
#include <video/previewmanager.h>
#include <video/devicemodel.h>

/* client */
#include "native/pixbufmanipulator.h"
#include "video/video_widget.h"

/* system */
#include <glib/gi18n.h>

struct _SelfieView
{
    GtkBox parent;
};

struct _SelfieViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _SelfieViewPrivate SelfieViewPrivate;

struct _SelfieViewPrivate
{
    // Main box
    GtkWidget *box_views_and_controls;

    // Video widget used for getting live webcam video & snapshots
    GtkWidget *video_widget;

    // Button blocks
    GtkWidget *box_controls;
    GtkWidget *button_box_init;
    GtkWidget *button_box_timer;
    GtkWidget *button_box_recording;
    GtkWidget *button_box_validate;

    // Control buttons
    GtkWidget *button_take_photo;
    GtkWidget *button_take_video;
    GtkWidget *button_take_sound;
    GtkWidget *button_back_to_init;
    GtkWidget *button_stop_record;
    GtkWidget *button_quit;
    GtkWidget *button_send;

    // Stack view and views
    GtkWidget *stack_views;
    GtkWidget *frame_pic;
    GtkWidget *frame_video;
    GtkWidget *frame_audio;
    GtkWidget *frame_audio_video_final;

    // Internal state: used to keep track of current layout
    SelfieViewState state;

    /* this is used to keep track of the state of the preview when the camera is used to take a
     * photo; if a call is in progress, then the preview should already be started and we don't want
     * to stop it when the settings are closed, in this case
     * TODO: necessary ?
     */
    gboolean video_started_by_selfie_view;
};

G_DEFINE_TYPE_WITH_PRIVATE(SelfieView, selfie_view, GTK_TYPE_BOX);
#define SELFIE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SELFIE_VIEW_TYPE, SelfieViewPrivate))

static void set_state(SelfieView *self, SelfieViewState state);
static void take_photo(SelfieView *self);
static void record_video(SelfieView *self);
static void record_audio(SelfieView *self);
static void back_to_init(SelfieView *self);
static void stop_record(SelfieView *self);
static void action_quit(SelfieView *self);
static void action_send(SelfieView *self);
static void display_pic(SelfieView *parent);
static void got_snapshot(SelfieView *parent);

static void
selfie_view_dispose(GObject *object)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(object);

    /* make sure we stop the preview and the video widget */
    if (priv->video_started_by_selfie_view)
        Video::PreviewManager::instance().stopPreview();
    if (priv->video_widget) {
        gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
        priv->video_widget = NULL;
    }

    G_OBJECT_CLASS(selfie_view_parent_class)->dispose(object);
}

static void
selfie_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(selfie_view_parent_class)->finalize(object);
}

GtkWidget*
selfie_view_new(void)
{
    return (GtkWidget *)g_object_new(SELFIE_VIEW_TYPE, NULL);
}

static void
selfie_view_class_init(SelfieViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = selfie_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = selfie_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/cx/ring/RingGnome/selfieview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, box_views_and_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, box_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_take_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_take_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_take_sound);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_back_to_init);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_stop_record);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_quit);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_send);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, stack_views);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, frame_pic);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, frame_audio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, frame_audio_video_final);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_init);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_timer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_validate);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_recording);
}

static void
selfie_view_init(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    /* signals */
    g_signal_connect_swapped(priv->button_take_photo, "clicked", G_CALLBACK(take_photo), self);
    g_signal_connect_swapped(priv->button_take_video, "clicked", G_CALLBACK(record_video), self);
    g_signal_connect_swapped(priv->button_take_sound, "clicked", G_CALLBACK(record_audio), self);
    g_signal_connect_swapped(priv->button_back_to_init, "clicked", G_CALLBACK(back_to_init), self);
    g_signal_connect_swapped(priv->button_stop_record, "clicked", G_CALLBACK(stop_record), self);
    g_signal_connect_swapped(priv->button_quit, "clicked", G_CALLBACK(action_quit), self);
    g_signal_connect_swapped(priv->button_send, "clicked", G_CALLBACK(action_send), self);

    set_state(self, SELFIE_VIEW_STATE_INIT);

    gtk_widget_show_all(priv->stack_views);
}

static void
set_state(SelfieView *self, SelfieViewState state)
{
    // state transition validity checking is expected to be done by the caller
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);

    switch (state) {
        case SELFIE_VIEW_STATE_INIT:
        {
            g_debug("set_state: Setting state to SELFIE_VIEW_STATE_INIT");
            // start the video; if its not available we should not be in this state
            priv->video_widget = video_widget_new();
            g_signal_connect_swapped(priv->video_widget, "snapshot-taken", G_CALLBACK (got_snapshot), self);
            gtk_widget_set_vexpand_set(priv->video_widget, FALSE);
            gtk_widget_set_hexpand_set(priv->video_widget, FALSE);
            gtk_container_add(GTK_CONTAINER(priv->frame_video), priv->video_widget);
            gtk_widget_set_visible(priv->video_widget, true);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_photobooth");

            /* local renderer, but set as "remote" so that it takes up the whole screen */
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           Video::PreviewManager::instance().previewRenderer(),
                                           VIDEO_RENDERER_REMOTE);

            if (!Video::PreviewManager::instance().isPreviewing()) {
                priv->video_started_by_selfie_view = TRUE;
                Video::PreviewManager::instance().startPreview();
            } else {
                priv->video_started_by_selfie_view = FALSE;
            }

            // Select the right button set for this view
            gtk_widget_set_visible(priv->button_box_init,       true);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  false);
            gtk_widget_set_visible(priv->button_box_validate,   false);
        }
        break;
        case SELFIE_VIEW_STATE_PIC:
        {
            g_debug("set_state: Setting state to SELFIE_VIEW_STATE_PIC");
            // Only thing we have to do is update button set to timer
            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      true);
            gtk_widget_set_visible(priv->button_box_recording,  false);
            gtk_widget_set_visible(priv->button_box_validate,   false);
        }
        break;
        case SELFIE_VIEW_STATE_PIC_END:
        {
            g_debug("set_state: Setting state to SELFIE_VIEW_STATE_PIC_END");
            /* make sure video widget and camera is not running */
            if (priv->video_started_by_selfie_view)
                Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }

            display_pic(self);

            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  false);
            gtk_widget_set_visible(priv->button_box_validate,   true);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_displaypic");
        }
        break;
        case SELFIE_VIEW_STATE_VIDEO:
        {
            g_debug("set_state: Setting state to SELFIE_VIEW_STATE_VIDEO");
            // Only thing we have to do is update button set to recording
            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  true);
            gtk_widget_set_visible(priv->button_box_validate,   false);
        }
        break;
        case SELFIE_VIEW_STATE_SOUND:
        {
            g_debug("set_state: Setting state to SELFIE_VIEW_STATE_SOUND");
            /* make sure video widget and camera is not running */
            if (priv->video_started_by_selfie_view)
                Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }

            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  true);
            gtk_widget_set_visible(priv->button_box_validate,   false);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_audio_recording");
        }
        break;
        case SELFIE_VIEW_STATE_VIDEO_END:
        {
            /* make sure video widget and camera is not running */
            if (priv->video_started_by_selfie_view)
                Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }
        }
        // intentional fall through, audio and video final states are identical currently
        case SELFIE_VIEW_STATE_SOUND_END:
        {
            g_debug("set_state: Setting state to SELFIE_VIEW_STATE_VIDEO/SOUND_END");
            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  false);
            gtk_widget_set_visible(priv->button_box_validate,   true);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_audio_video_validate");
        }
        break;
        default:
            g_debug("set_state: internal bug @switch: got invalid state.");
            return;
    }

    priv->state = state;
}

static void
take_photo(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // set_state(self, SELFIE_VIEW_STATE_PIC);
    // TODO implement timer, currently we take photo directly
    video_widget_take_snapshot(VIDEO_WIDGET(priv->video_widget));
}

static void
record_audio(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
record_video(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
back_to_init(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    set_state(self, SELFIE_VIEW_STATE_INIT);
}

static void
stop_record(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
action_send(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
action_quit(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
display_pic(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
got_snapshot(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    GdkPixbuf* pix = video_widget_get_snapshot(VIDEO_WIDGET(priv->video_widget));

    set_state(self, SELFIE_VIEW_STATE_PIC_END);
}
