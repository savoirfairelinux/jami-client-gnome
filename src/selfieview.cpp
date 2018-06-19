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

/* Qt */
#include <QtCore/QStandardPaths>

/* LRC */
#include <video/configurationproxy.h>
#include <video/previewmanager.h>
#include <video/devicemodel.h>

/* Client */
#include "native/pixbufmanipulator.h"
#include "video/video_widget.h"

/* GTK */
#include <glib/gi18n.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib/gstdio.h>

/* std */
#include <chrono>
#include <iomanip>
#include <fstream>

#define SAVES_DIR "sent_data"
#define SAVES_DIR_PERMS 755

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

    // Last snapshot
    GdkPixbuf *current_img;
    GtkWidget *current_gtk_img = NULL;
    std::string save_file_name;
    GtkAllocation current_alloc;

    // Internal state: used to keep track of current layout
    SelfieViewState state;

    /* this is used to keep track of the state of the preview when the camera is used to take a
     * photo; if a call is in progress, then the preview should already be started and we don't want
     * to stop it when the settings are closed, in this case
     * TODO: necessary ?
     */
    gboolean video_started_by_selfie_view;
};

/* signals */
enum {
    SEND_ACTION,
    QUIT_ACTION,
    LAST_SIGNAL
};

static guint selfie_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(SelfieView, selfie_view, GTK_TYPE_BOX);
#define SELFIE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SELFIE_VIEW_TYPE, SelfieViewPrivate))

static void resize_image(SelfieView *self, int height, int width);
static void set_state(SelfieView *self, SelfieViewState state);
static void got_snapshot(SelfieView *parent);

/* Button callbacks */
static void on_take_photo_button_pressed(SelfieView *self);
static void on_record_video_button_pressed(SelfieView *self);
static void on_record_audio_button_pressed(SelfieView *self);
static void on_back_to_init_button_pressed(SelfieView *self);
static void on_stop_record_button_pressed(SelfieView *self);
static void on_quit_button_pressed(SelfieView *self);
static void on_send_button_pressed(SelfieView *self);

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
on_resize(GtkWidget *widget, GtkAllocation *allocation, gpointer *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);

    if(priv->current_alloc.width == allocation->width
           && priv->current_alloc.height == allocation->height) {
        // Noise, nothing to update.
        return;
    }

    priv->current_alloc = *allocation;
    resize_image(SELFIE_VIEW((GtkWidget*)self), allocation->height, allocation->width);
}

static std::string
get_saves_directory()
{
    std::string dir = QStandardPaths::writableLocation(QStandardPaths::DataLocation).toStdString() + "/" + SAVES_DIR;
    g_mkdir (dir.c_str(), SAVES_DIR_PERMS);
    return dir;
}

static bool
directory_exists(const std::string& name)
{
    std::ifstream f(name.c_str());
    return f.good();
}

static std::string
get_file_name_with_ext(const std::string& ext)
{
    std::chrono::time_point<std::chrono::system_clock> time_now = std::chrono::system_clock::now();
    std::time_t time_now_t = std::chrono::system_clock::to_time_t(time_now);
    std::tm now_tm = *std::localtime(&time_now_t);
    std::stringstream ss;
    ss << std::put_time(&now_tm, "%Y%m%d-%H%M%S");

    std::string file_name = get_saves_directory() + "/" + ss.str();

    // Add suffix if necessary;
    if (directory_exists(!ext.empty() ? file_name + "." + ext.c_str() : file_name)) {
        int suffix = 0;
        file_name += "-";
        while (directory_exists(!ext.empty() ? file_name + std::to_string(suffix) + "." + ext.c_str()
                                            : file_name + std::to_string(suffix))) {
            suffix++;
        }
        file_name += std::to_string(suffix);
    }

    return (!ext.empty() ? file_name + "." + ext.c_str() : file_name);
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

    // Define custom signals
    selfie_signals[SEND_ACTION] = g_signal_new ("send_action",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    selfie_signals[QUIT_ACTION] = g_signal_new ("quit_action",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
selfie_view_init(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".black-bg { background-color: black; } \
        .selfie-button { background: rgba(0, 0, 0, 0.35); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .selfie-button:hover { background: rgba(0, 0, 0, 0.2); } \
        .selfie-button:disabled { opacity: 0.2; }",
        -1, nullptr
    );

    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // Connect button signals
    g_signal_connect_swapped(priv->button_take_photo, "clicked", G_CALLBACK(on_take_photo_button_pressed), self);
    g_signal_connect_swapped(priv->button_take_video, "clicked", G_CALLBACK(on_record_video_button_pressed), self);
    g_signal_connect_swapped(priv->button_take_sound, "clicked", G_CALLBACK(on_record_audio_button_pressed), self);
    g_signal_connect_swapped(priv->button_back_to_init, "clicked", G_CALLBACK(on_back_to_init_button_pressed), self);
    g_signal_connect_swapped(priv->button_stop_record, "clicked", G_CALLBACK(on_stop_record_button_pressed), self);
    g_signal_connect_swapped(priv->button_quit, "clicked", G_CALLBACK(on_quit_button_pressed), self);
    g_signal_connect_swapped(priv->button_send, "clicked", G_CALLBACK(on_send_button_pressed), self);

    // Initialize default state
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
            // start the video; if its not available we should not be in this state
            priv->video_widget = video_widget_new();
            g_signal_connect_swapped(priv->video_widget, "snapshot-taken", G_CALLBACK (got_snapshot), self);
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
            // Only thing we have to do is update button set to timer
            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      true);
            gtk_widget_set_visible(priv->button_box_recording,  false);
            gtk_widget_set_visible(priv->button_box_validate,   false);
        }
        break;
        case SELFIE_VIEW_STATE_PIC_END:
        {
            GtkAllocation allocation;
            gtk_widget_get_allocation(priv->frame_video, &allocation);
            priv->current_alloc = allocation;
            resize_image(self, allocation.height, allocation.width);
            g_signal_connect(priv->frame_pic, "size-allocate", G_CALLBACK(on_resize), (gpointer)self);

            /* make sure video widget and camera is not running */
            if (priv->video_started_by_selfie_view)
                Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }

            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  false);
            gtk_widget_set_visible(priv->button_box_validate,   true);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_displaypic");
        }
        break;
        case SELFIE_VIEW_STATE_VIDEO:
        {
            // Only thing we have to do is update button set to recording
            gtk_widget_set_visible(priv->button_box_init,       false);
            gtk_widget_set_visible(priv->button_box_timer,      false);
            gtk_widget_set_visible(priv->button_box_recording,  true);
            gtk_widget_set_visible(priv->button_box_validate,   false);
        }
        break;
        case SELFIE_VIEW_STATE_SOUND:
        {
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
on_take_photo_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // set_state(self, SELFIE_VIEW_STATE_PIC);
    // TODO implement timer, currently we take photo directly
    video_widget_take_snapshot(VIDEO_WIDGET(priv->video_widget));
}

static void
on_record_audio_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // TODO
}

static void
on_record_video_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    set_state(self, SELFIE_VIEW_STATE_VIDEO);
    priv->save_file_name.assign(get_file_name_with_ext(""));
    Video::PreviewManager::instance().startLocalRecorder(false, priv->save_file_name);
}

static void
on_back_to_init_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    set_state(self, SELFIE_VIEW_STATE_INIT);
}

static void
on_stop_record_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    // recorder should always be stopped before preview
    Video::PreviewManager::instance().stopLocalRecorder();
    set_state(self, SELFIE_VIEW_STATE_VIDEO_END);
}

static void
on_send_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);

    switch (priv->state) {
        case SELFIE_VIEW_STATE_PIC_END:
        {
            if (!priv->current_img) {
                return;
            }

            // Create file name
            priv->save_file_name.assign(get_file_name_with_ext("jpg"));

            GError *error = NULL;
            gdk_pixbuf_save (priv->current_img, priv->save_file_name.c_str(), "jpeg", &error, "quality", "100", NULL);
        }
        break;
        case SELFIE_VIEW_STATE_VIDEO_END:
        {
        }
        break;
        case SELFIE_VIEW_STATE_SOUND_END:
        {
        }
        break;
        default:
            return;
    }

    g_signal_emit(self, selfie_signals[SEND_ACTION], 0);
}

static void
on_quit_button_pressed(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    g_signal_emit(self, selfie_signals[QUIT_ACTION], 0);
}

static void
resize_image(SelfieView *self, int height, int width)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);

    if (!priv->current_img) {
        return;
    }

    double heightRatio = (double)height/(double)gdk_pixbuf_get_height(priv->current_img);
    double widthRatio = (double)width/(double)gdk_pixbuf_get_width(priv->current_img);
    double ratio = std::min(heightRatio, widthRatio);

    GdkPixbuf *img = gdk_pixbuf_scale_simple(priv->current_img,
                                             (int)((double)gdk_pixbuf_get_width(priv->current_img)*ratio),
                                             (int)((double)gdk_pixbuf_get_height(priv->current_img)*ratio),
                                             GDK_INTERP_NEAREST);

    if(!priv->current_gtk_img) {
        /* First time resize_image is called. This is no proper resize since we
           actually set the image for the first time. */
        priv->current_gtk_img = gtk_image_new_from_pixbuf(img);
        gtk_widget_set_visible(priv->current_gtk_img, true);
        gtk_container_add(GTK_CONTAINER(priv->frame_pic), priv->current_gtk_img);
    }
    else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->current_gtk_img), img);
    }
}

static void
got_snapshot(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    priv->current_img = gdk_pixbuf_copy(video_widget_get_snapshot(VIDEO_WIDGET(priv->video_widget)));
    set_state(self, SELFIE_VIEW_STATE_PIC_END);
}

const char*
selfie_view_get_save_file(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);

    const char *string;
    if (!priv->save_file_name.empty()) {
        string = priv->save_file_name.c_str();
    } else {
        string = NULL;
    }

    return string;
}
