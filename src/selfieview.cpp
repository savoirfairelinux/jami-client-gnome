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

#include "avatarmanipulation.h"

/* LRC */
#include <globalinstances.h>
#include <person.h>
#include <profile.h>
#include <profilemodel.h>
#include <video/configurationproxy.h>
#include <video/previewmanager.h>
#include <video/devicemodel.h>

/* client */
#include "native/pixbufmanipulator.h"
#include "video/video_widget.h"

/* system */
#include <glib/gi18n.h>

/* size of video widget */
static constexpr int VIDEO_WIDTH = 300; /* px */
static constexpr int VIDEO_HEIGHT = 200; /* px */

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
    GtkWidget *stack_selfie_view;
    GtkWidget *video_widget;
    GtkWidget *box_views_and_controls;
    GtkWidget *box_controls;

    GtkWidget *button_box_current;
    GtkWidget *button_box_photo;
    GtkWidget *button_box_edit;

    GtkWidget *button_start_camera;
    GtkWidget *button_choose_picture;
    GtkWidget *button_take_photo;
    GtkWidget *button_return_photo;
    GtkWidget *button_set_avatar;
    GtkWidget *button_return_edit;

    // GtkWidget *selector_widget;
    GtkWidget *stack_views;
    GtkWidget *image_avatar;
    GtkWidget *frame_video;

    SelfieViewState state;
    SelfieViewState last_state;

    /* this is used to keep track of the state of the preview when the camera is used to take a
     * photo; if a call is in progress, then the preview should already be started and we don't want
     * to stop it when the settings are closed, in this case
     */
    gboolean video_started_by_selfie_view;
};

G_DEFINE_TYPE_WITH_PRIVATE(SelfieView, selfie_view, GTK_TYPE_BOX);

#define SELFIE_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SELFIE_VIEW_TYPE, \
                                                                                             SelfieViewPrivate))

static void set_state(SelfieView *self, SelfieViewState state);

static void start_camera(SelfieView *self);
static void take_a_photo(SelfieView *self);
static void choose_picture(SelfieView *self);
static void update_preview_cb(GtkFileChooser *file_chooser, GtkWidget *preview);
static void set_avatar(SelfieView *self);
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
    // a profile must exist
    g_return_val_if_fail(ProfileModel::instance().selectedProfile(), NULL);

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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_start_camera);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_choose_picture);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_take_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_return_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_set_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_return_edit);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, stack_views);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, image_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_current);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), SelfieView, button_box_edit);
}

static void
selfie_view_init(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    /* our desired size for the image area */
    gtk_widget_set_size_request(priv->stack_views, VIDEO_WIDTH, VIDEO_HEIGHT);

    /* signals */
    g_signal_connect_swapped(priv->button_start_camera, "clicked", G_CALLBACK(start_camera), self);
    g_signal_connect_swapped(priv->button_choose_picture, "clicked", G_CALLBACK(choose_picture), self);
    g_signal_connect_swapped(priv->button_take_photo, "clicked", G_CALLBACK(take_a_photo), self);
    g_signal_connect_swapped(priv->button_set_avatar, "clicked", G_CALLBACK(set_avatar), self);

    set_state(self, SELFIE_VIEW_STATE_CURRENT);

    gtk_widget_show_all(priv->stack_views);
}

static void
set_state(SelfieView *self, SelfieViewState state)
{
    // note: this funciton does not check if the state transition is valid, this is assumed to have
    // been done by the caller
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);

    // save prev state
    priv->last_state = priv->state;

    switch (state) {
        case SELFIE_VIEW_STATE_CURRENT:
        {
            /* get the current or default profile avatar */
            auto photo = GlobalInstances::pixmapManipulator().contactPhoto(
                            ProfileModel::instance().selectedProfile()->person(),
                            QSize(AVATAR_WIDTH, AVATAR_HEIGHT),
                            false);
            std::shared_ptr<GdkPixbuf> pixbuf_photo = photo.value<std::shared_ptr<GdkPixbuf>>();

            if (photo.isValid()) {
                gtk_image_set_from_pixbuf (GTK_IMAGE(priv->image_avatar),  pixbuf_photo.get());
            } else {
                g_warning("invlid pixbuf");
            }

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_avatar");

            /* available actions: start camera (if available) or choose image */
            if (Video::DeviceModel::instance().rowCount() > 0) {
                // TODO: update if a video device gets inserted while in this state
                gtk_widget_set_visible(priv->button_start_camera, true);
            }
            gtk_widget_set_visible(priv->button_box_current, true);
            gtk_widget_set_visible(priv->button_box_photo,   false);
            gtk_widget_set_visible(priv->button_box_edit,    false);

            /* make sure video widget and camera is not running */
            if (priv->video_started_by_selfie_view)
                Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }
        }
        break;
        case SELFIE_VIEW_STATE_PHOTO:
        {
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

            /* available actions: take snapshot, return*/
            gtk_widget_set_visible(priv->button_box_current, false);
            gtk_widget_set_visible(priv->button_box_photo,   true);
            gtk_widget_set_visible(priv->button_box_edit,    false);
        }
        break;
        case SELFIE_VIEW_STATE_EDIT:
        {
            /* make sure video widget and camera is not running */
            if (priv->video_started_by_selfie_view)
                Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }

            /* available actions: set avatar, return */
            gtk_widget_set_visible(priv->button_box_current, false);
            gtk_widget_set_visible(priv->button_box_photo,   false);
            gtk_widget_set_visible(priv->button_box_edit,    true);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_edit_view");
        }
        break;
    }

    priv->state = state;
}

static void
start_camera(SelfieView *self)
{
    set_state(self, SELFIE_VIEW_STATE_PHOTO);
}

static void
take_a_photo(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    video_widget_take_snapshot(VIDEO_WIDGET(priv->video_widget));
}

static void
update_preview_cb(GtkFileChooser *file_chooser, GtkWidget *preview)
{
    gboolean have_preview = FALSE;
    if (auto filename = gtk_file_chooser_get_preview_filename(file_chooser)) {
        GError* error =  nullptr;
        auto pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 128, 128, &error);
        if (!error) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(preview), pixbuf);
            g_object_unref(pixbuf);
            have_preview = TRUE;
        } else {
            // nothing to do, the file is probably not a picture
        }
        g_free (filename);
    }
    gtk_file_chooser_set_preview_widget_active(file_chooser, have_preview);
}

static void
got_snapshot(SelfieView *self)
{
    SelfieViewPrivate *priv = SELFIE_VIEW_GET_PRIVATE(self);
    GdkPixbuf* pix = video_widget_get_snapshot(VIDEO_WIDGET(priv->video_widget));


    set_state(self, SELFIE_VIEW_STATE_EDIT);
}
