/*
 *  Copyright (C) 2016-2022 Savoir-faire Linux Inc.
 *  Author: Nicolas Jager <nicolas.jager@savoirfairelinux.com>
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

#include "avatarmanipulation.h"

/* LRC */
#include <api/newaccountmodel.h>
#include <api/avmodel.h>
#include <QSize>

/* client */
#include "utils/drawing.h"
#include "video/video_widget.h"
#include "cc-crop-area.h"

/* system */
#include <glib/gi18n.h>

/* size of avatar */
static constexpr int AVATAR_WIDTH  = 150; /* px */
static constexpr int AVATAR_HEIGHT = 150; /* px */

/* size of video widget */
static constexpr int VIDEO_WIDTH = 150; /* px */
static constexpr int VIDEO_HEIGHT = 150; /* px */

struct _AvatarManipulation
{
    GtkBox parent;
};

struct _AvatarManipulationClass
{
    GtkBoxClass parent_class;
};

typedef struct _AvatarManipulationPrivate AvatarManipulationPrivate;

struct _AvatarManipulationPrivate
{
    AccountInfoPointer const *accountInfo_ = nullptr;
    gchar* temporaryAvatar = nullptr;

    GtkWidget *stack_avatar_manipulation;
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
    GtkWidget *vbox_crop_area;
    GtkWidget *frame_video;

    AvatarManipulationState state;
    AvatarManipulationState last_state;

    /* this is used to keep track of the state of the preview when the camera is used to take a
     * photo; if a call is in progress, then the preview should already be started and we don't want
     * to stop it when the settings are closed, in this case
     */
    gboolean video_started_by_avatar_manipulation;

    QMetaObject::Connection local_renderer_connection;

    GtkWidget *crop_area;

    lrc::api::AVModel* avModel_;
};

G_DEFINE_TYPE_WITH_PRIVATE(AvatarManipulation, avatar_manipulation, GTK_TYPE_BOX);

#define AVATAR_MANIPULATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), AVATAR_MANIPULATION_TYPE, \
                                                                                             AvatarManipulationPrivate))

static void set_state(AvatarManipulation *self, AvatarManipulationState state);

static void start_camera(AvatarManipulation *self);
static void take_a_photo(AvatarManipulation *self);
static void choose_picture(AvatarManipulation *self);
static void return_to_previous(AvatarManipulation *self);
static void update_preview_cb(GtkFileChooser *file_chooser, GtkWidget *preview);
static void set_avatar(AvatarManipulation *self);
static void got_snapshot(AvatarManipulation *parent);

static void
avatar_manipulation_dispose(GObject *object)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(object);

    /* make sure we stop the preview and the video widget */
    if (priv->video_started_by_avatar_manipulation)
        priv->avModel_->stopPreview("camera://" + priv->avModel_->getDefaultDevice());
    if (priv->video_widget) {
        gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
        priv->video_widget = NULL;
    }

    QObject::disconnect(priv->local_renderer_connection);

    G_OBJECT_CLASS(avatar_manipulation_parent_class)->dispose(object);
}

static void
avatar_manipulation_finalize(GObject *object)
{
    G_OBJECT_CLASS(avatar_manipulation_parent_class)->finalize(object);
}

GtkWidget*
avatar_manipulation_new(AccountInfoPointer const & accountInfo, lrc::api::AVModel* avModel)
{
    // a profile must exist
    gpointer view = g_object_new(AVATAR_MANIPULATION_TYPE, NULL);

    auto* priv = AVATAR_MANIPULATION_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    priv->avModel_ = avModel;

    set_state(AVATAR_MANIPULATION(view), AVATAR_MANIPULATION_STATE_CURRENT);

    return reinterpret_cast<GtkWidget*>(view);
}

GtkWidget*
avatar_manipulation_new_from_wizard(lrc::api::AVModel* avModel)
{
    // a profile must exist
    gpointer view = g_object_new(AVATAR_MANIPULATION_TYPE, NULL);

    auto* priv = AVATAR_MANIPULATION_GET_PRIVATE(view);
    priv->accountInfo_ = nullptr;
    priv->avModel_ = avModel;

    set_state(AVATAR_MANIPULATION(view), AVATAR_MANIPULATION_STATE_CURRENT);

    return reinterpret_cast<GtkWidget*>(view);
}

gchar*
avatar_manipulation_get_temporary(AvatarManipulation *view)
{
    g_return_val_if_fail(IS_AVATAR_MANIPULATION(view), nullptr);
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(view);
    return priv->temporaryAvatar;
}

static void
avatar_manipulation_class_init(AvatarManipulationClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = avatar_manipulation_finalize;
    G_OBJECT_CLASS(klass)->dispose = avatar_manipulation_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/net/jami/JamiGnome/avatarmanipulation.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, box_views_and_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, box_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_start_camera);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_choose_picture);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_take_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_return_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_set_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_return_edit);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, stack_views);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, image_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, vbox_crop_area);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_box_current);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_box_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_box_edit);
}

static void
avatar_manipulation_init(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    /* our desired size for the image area */
    gtk_widget_set_size_request(priv->stack_views, VIDEO_WIDTH, VIDEO_HEIGHT);

    /* signals */
    g_signal_connect_swapped(priv->button_start_camera, "clicked", G_CALLBACK(start_camera), self);
    g_signal_connect_swapped(priv->button_choose_picture, "clicked", G_CALLBACK(choose_picture), self);
    g_signal_connect_swapped(priv->button_take_photo, "clicked", G_CALLBACK(take_a_photo), self);
    g_signal_connect_swapped(priv->button_return_photo, "clicked", G_CALLBACK(return_to_previous), self);
    g_signal_connect_swapped(priv->button_set_avatar, "clicked", G_CALLBACK(set_avatar), self);
    g_signal_connect_swapped(priv->button_return_edit, "clicked", G_CALLBACK(return_to_previous), self);

    set_state(self, AVATAR_MANIPULATION_STATE_CURRENT);

    gtk_widget_show_all(priv->stack_views);
}

static void
set_state(AvatarManipulation *self, AvatarManipulationState state)
{
    // note: this function does not check if the state transition is valid, this is assumed to have
    // been done by the caller
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    // save prev state
    priv->last_state = priv->state;

    switch (state) {
        case AVATAR_MANIPULATION_STATE_CURRENT:
        {
            /* get the current or default profile avatar */
            GdkPixbuf *default_avatar = draw_generate_avatar("", "");
            GdkPixbuf *photo = draw_scale_and_frame(
                default_avatar, QSize(AVATAR_WIDTH, AVATAR_HEIGHT));
            g_object_unref(default_avatar);
            if ((priv->accountInfo_ && (*priv->accountInfo_)) || priv->temporaryAvatar) {
                auto photostr = priv->temporaryAvatar? priv->temporaryAvatar : (*priv->accountInfo_)->profileInfo.avatar;
                QByteArray byteArray = photostr.toUtf8();
                GdkPixbuf *avatar = draw_person_photo(byteArray);
                if (avatar) {
                    g_object_unref(photo);
                    photo = draw_scale_and_frame(avatar, QSize(AVATAR_WIDTH, AVATAR_HEIGHT));
                    g_object_unref(avatar);
                }
            }
            gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_avatar), photo);
            g_object_unref(photo);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_avatar");

            /* available actions: start camera (if available) or choose image */
            if (priv->avModel_->getDevices().size() > 0) {
                // TODO: update if a video device gets inserted while in this state
                gtk_widget_set_visible(priv->button_start_camera, true);
            }
            gtk_widget_set_visible(priv->button_box_current, true);
            gtk_widget_set_visible(priv->button_box_photo,   false);
            gtk_widget_set_visible(priv->button_box_edit,    false);

            /* make sure video widget and camera is not running */
            if (priv->video_started_by_avatar_manipulation) {
                priv->avModel_->stopPreview("camera://" + priv->avModel_->getDefaultDevice());
                QObject::disconnect(priv->local_renderer_connection);
            }
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->frame_video), priv->video_widget);
                priv->video_widget = NULL;
            }
        }
        break;
        case AVATAR_MANIPULATION_STATE_PHOTO:
        {
            // start the video; if its not available we should not be in this state
            priv->video_widget = video_widget_new();
            g_signal_connect_swapped(priv->video_widget, "snapshot-taken", G_CALLBACK (got_snapshot), self);
            gtk_widget_set_vexpand_set(priv->video_widget, FALSE);
            gtk_widget_set_hexpand_set(priv->video_widget, FALSE);
            gtk_container_add(GTK_CONTAINER(priv->frame_video), priv->video_widget);
            gtk_widget_set_visible(priv->video_widget, true);
            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_photobooth");


            // local renderer, but set as "remote" so that it takes up the whole screen
            const lrc::api::video::Renderer* prenderer = nullptr;
            try {
                prenderer = &priv->avModel_->getRenderer("camera://" + priv->avModel_->getDefaultDevice());
            } catch (const std::out_of_range& e) {}

            priv->video_started_by_avatar_manipulation =
                prenderer && prenderer->isRendering();
            if (priv->video_started_by_avatar_manipulation) {
                video_widget_add_new_renderer(
                    VIDEO_WIDGET(priv->video_widget),
                    priv->avModel_, prenderer,
                    VIDEO_RENDERER_REMOTE);
            } else {
                priv->video_started_by_avatar_manipulation = true;
                priv->local_renderer_connection = QObject::connect(
                    &*priv->avModel_,
                    &lrc::api::AVModel::rendererStarted,
                    [=](const QString& id) {
                        if (id.indexOf( priv->avModel_->getDefaultDevice()) == -1)
                            return;
                        try {
                            const auto* prenderer =
                                &priv->avModel_->getRenderer(id);
                            video_widget_add_new_renderer(
                                VIDEO_WIDGET(priv->video_widget),
                                priv->avModel_, prenderer,
                                VIDEO_RENDERER_REMOTE);
                        }  catch (const std::out_of_range& e) {
                            g_warning("Cannot start preview");
                        }
                    });
                priv->avModel_->startPreview("camera://" + priv->avModel_->getDefaultDevice());
            }

            /* available actions: take snapshot, return*/
            gtk_widget_set_visible(priv->button_box_current, false);
            gtk_widget_set_visible(priv->button_box_photo,   true);
            gtk_widget_set_visible(priv->button_box_edit,    false);
        }
        break;
        case AVATAR_MANIPULATION_STATE_EDIT:
        {
            /* make sure video widget and camera is not running */
            if (priv->video_started_by_avatar_manipulation)
                priv->avModel_->stopPreview("camera://" + priv->avModel_->getDefaultDevice());
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
start_camera(AvatarManipulation *self)
{
    set_state(self, AVATAR_MANIPULATION_STATE_PHOTO);
}

static void
take_a_photo(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    video_widget_take_snapshot(VIDEO_WIDGET(priv->video_widget));
}

static void
set_avatar(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    gchar* png_buffer_signed = nullptr;
    gsize png_buffer_size;
    GError* error =  nullptr;

    /* get the cropped area */
    GdkPixbuf *selector_pixbuf = cc_crop_area_get_picture(CC_CROP_AREA(priv->crop_area));

    /* scale it */
    GdkPixbuf* pixbuf_frame_resized = gdk_pixbuf_scale_simple(selector_pixbuf, AVATAR_WIDTH, AVATAR_HEIGHT,
                                                              GDK_INTERP_HYPER);

    /* save the png in memory */
    gdk_pixbuf_save_to_buffer(pixbuf_frame_resized, &png_buffer_signed, &png_buffer_size, "png", &error, NULL);
    if (!png_buffer_signed) {
        g_warning("(set_avatar) failed to save pixbuffer to png: %s\n", error->message);
        g_error_free(error);
        return;
    }

    /* convert buffer to QByteArray in base 64*/
    QByteArray png_q_byte_array = QByteArray::fromRawData(png_buffer_signed, png_buffer_size).toBase64();

    /* save in profile */
    if (priv->accountInfo_ && (*priv->accountInfo_)) {
        try {
            (*priv->accountInfo_)->accountModel->setAvatar((*priv->accountInfo_)->id, png_q_byte_array);
        } catch (std::out_of_range&) {
            g_warning("Can't set avatar for unknown account");
        }
    } else {
        priv->temporaryAvatar = g_strdup(png_q_byte_array.toStdString().c_str());
    }

    g_free(png_buffer_signed);
    g_object_unref(selector_pixbuf);
    g_object_unref(pixbuf_frame_resized);

    set_state(self, AVATAR_MANIPULATION_STATE_CURRENT);
}

static void
return_to_previous(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    if (priv->state == AVATAR_MANIPULATION_STATE_PHOTO) {
        // from photo we alway go back to current
        set_state(self, AVATAR_MANIPULATION_STATE_CURRENT);
    } else {
        // otherwise, if we were in edit state, we may have come from photo or current state
        set_state(self, priv->last_state);
    }
}

static void
choose_picture(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkWidget *main_window = gtk_widget_get_toplevel(GTK_WIDGET(self));
    GtkWidget *preview = gtk_image_new();
    gint res;
    gchar *filename = nullptr;

#if GTK_CHECK_VERSION(3,20,0)
    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        _("Open Avatar Image"),
        GTK_WINDOW(main_window),
        action,
        _("_Open"),
        _("_Cancel"));

    /* add an image preview inside the file choose */
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(native),
                                        preview);
    g_signal_connect(GTK_FILE_CHOOSER(native), "update-preview",
                     G_CALLBACK(update_preview_cb), preview);

    /* start the file chooser */
    /* (blocks until the dialog is closed) */
    res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }

    g_object_unref (native);
#else
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        _("Open Avatar Image"),
        GTK_WINDOW(main_window),
        action,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Open"), GTK_RESPONSE_ACCEPT,
        NULL);

    /* add an image preview inside the file choose */
    gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(dialog),
                                        preview);
    g_signal_connect (GTK_FILE_CHOOSER(dialog), "update-preview",
                      G_CALLBACK(update_preview_cb), preview);

    /* start the file chooser */
    /* (blocks until the dialog is closed) */
    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }

    gtk_widget_destroy(dialog);
#endif

    if(filename) {
        GError* error =  nullptr; /* initialising to null avoid trouble... */

        auto picture = gdk_pixbuf_new_from_file_at_size (filename, VIDEO_WIDTH, VIDEO_HEIGHT, &error);

        if (!error) {
            /* reset crop area */
            if (priv->crop_area)
                gtk_container_remove(GTK_CONTAINER(priv->vbox_crop_area), priv->crop_area);
            priv->crop_area = cc_crop_area_new();
            gtk_widget_show(priv->crop_area);
            gtk_box_pack_start(GTK_BOX(priv->vbox_crop_area), priv->crop_area, TRUE, TRUE, 0);
            cc_crop_area_set_picture(CC_CROP_AREA(priv->crop_area), picture);
            g_object_unref(picture);

            set_state(self, AVATAR_MANIPULATION_STATE_EDIT);
        } else {
            g_warning("(choose_picture) failed to load pixbuf from file: %s", error->message);
            g_error_free(error);
        }

        g_free(filename);
    } else {
        g_warning("(choose_picture) filename empty");
    }
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
got_snapshot(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    GdkPixbuf* pix = video_widget_get_snapshot(VIDEO_WIDGET(priv->video_widget));

    if (priv->crop_area)
        gtk_container_remove(GTK_CONTAINER(priv->vbox_crop_area), priv->crop_area);
    priv->crop_area = cc_crop_area_new();
    gtk_widget_show(priv->crop_area);
    gtk_box_pack_start(GTK_BOX(priv->vbox_crop_area), priv->crop_area, TRUE, TRUE, 0);
    cc_crop_area_set_picture(CC_CROP_AREA(priv->crop_area), pix);

    set_state(self, AVATAR_MANIPULATION_STATE_EDIT);
}

void
avatar_manipulation_wizard_completed(AvatarManipulation *self)
{
    auto priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    /* Tuleap: #1441
     * if the user did not validate the avatar area selection, we still take that as the image
     * for their avatar; otherwise many users end up with no avatar by default
     * TODO: improve avatar creation process to not need this fix
     */
    if (priv->state == AVATAR_MANIPULATION_STATE_EDIT)
        set_avatar(self);
}
