/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

/* size of avatar */
static constexpr int AVATAR_WIDTH  = 100; /* px */
static constexpr int AVATAR_HEIGHT = 100; /* px */

/* size of video widget */
static constexpr int VIDEO_WIDTH = 300; /* px */
static constexpr int VIDEO_HEIGHT = 200; /* px */

/* initial length of the selector */
static constexpr int INITIAL_LENTGH = 100; /* px */

/* mouse interactions with selector */
enum ActionOnSelector {
    MOVE_SELECTOR,
    PICK_SELECTOR
};

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
    GtkWidget *stack_avatar_manipulation;
    GtkWidget *video_widget;
    GtkWidget *box_views_and_controls;
    GtkWidget *box_controls;
    GtkWidget *button_take_photo;
    GtkWidget *button_choose_picture;
    GtkWidget *button_trash_avatar;
    GtkWidget *button_set_avatar;
    GtkWidget *button_export_avatar;
    GtkWidget *selector_widget;
    GtkWidget *stack_views;
    GtkWidget *image_avatar;
    GtkWidget *vbox_selector;
    GdkPixbuf *pix_scaled;

    /* selector widget properties */
    cairo_surface_t * selector_widget_surface;
    int origin[2];            /* top left coordinates of the selector */
    int relative_position[2]; /* this position refers the pointer to selector origin */
    int length;               /* selector length */
    ActionOnSelector action_on_selector;
    bool do_action;
    GdkModifierType button_pressed;

    AvatarManipulationState state;
};

G_DEFINE_TYPE_WITH_PRIVATE(AvatarManipulation, avatar_manipulation, GTK_TYPE_BOX);

#define AVATAR_MANIPULATION_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), AVATAR_MANIPULATION_TYPE, \
                                                                                             AvatarManipulationPrivate))

static void set_state(AvatarManipulation *self, AvatarManipulationState state);

static void take_a_photo(AvatarManipulation *self);
static void choose_picture(AvatarManipulation *self);
static void export_avatar(AvatarManipulation *self);
static void update_preview_cb(GtkFileChooser *file_chooser, GtkWidget *preview);
static void trash_photo(AvatarManipulation *self);
static void set_avatar(AvatarManipulation *self);
static void got_snapshot(AvatarManipulation *parent);

/* area selected */
static gboolean selector_widget_button_press_event(GtkWidget *widget, GdkEventButton *event, AvatarManipulation *self);
static gboolean selector_widget_button_release_event(GtkWidget *widget, GdkEventButton *event, AvatarManipulation *self);
static gboolean selector_widget_motion_notify_event(GtkWidget *widget, GdkEventMotion *event, AvatarManipulation *self);
static gboolean selector_widget_configure_event(GtkWidget *widget, GdkEventConfigure *event, AvatarManipulation *self);
static gboolean selector_widget_draw(GtkWidget *widget, cairo_t *cr, AvatarManipulation *self);
static void update_and_draw(gdouble x, gdouble y, AvatarManipulation *self);
static void rescale(gdouble x, gdouble y, AvatarManipulation *self);

static void
avatar_manipulation_dispose(GObject *object)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(object);

    /* make sure we stop the preview and the video widget */
    Video::PreviewManager::instance().stopPreview();
    if (priv->video_widget) {
        gtk_container_remove(GTK_CONTAINER(priv->stack_views), priv->video_widget);
        priv->video_widget = NULL;
    }

    G_OBJECT_CLASS(avatar_manipulation_parent_class)->dispose(object);
}

static void
avatar_manipulation_finalize(GObject *object)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(object);

    if (priv->selector_widget_surface)
        cairo_surface_destroy(priv->selector_widget_surface);

    if (priv->pix_scaled)
        g_object_unref(priv->pix_scaled);

    G_OBJECT_CLASS(avatar_manipulation_parent_class)->finalize(object);
}

GtkWidget*
avatar_manipulation_new(void)
{
    // a profile must exist
    g_return_val_if_fail(ProfileModel::instance().selectedProfile(), NULL);

    return (GtkWidget *)g_object_new(AVATAR_MANIPULATION_TYPE, NULL);
}

GtkWidget*
avatar_manipulation_new_from_wizard(void)
{
    auto self = avatar_manipulation_new();

    /* in this mode, we want to automatically go to the new avatar state, unless one already exists */
    if (!ProfileModel::instance().selectedProfile()->person()->photo().isValid())
        set_state(AVATAR_MANIPULATION(self), AVATAR_MANIPULATION_STATE_NEW);

    return self;
}

static void
avatar_manipulation_class_init(AvatarManipulationClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = avatar_manipulation_finalize;
    G_OBJECT_CLASS(klass)->dispose = avatar_manipulation_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/cx/ring/RingGnome/avatarmanipulation.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, box_views_and_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, box_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_take_photo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_set_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_trash_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_choose_picture);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, button_export_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, stack_views);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, image_avatar);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AvatarManipulation, vbox_selector);
}

static void
avatar_manipulation_init(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    /* selector widget */
    priv->selector_widget = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(priv->vbox_selector), priv->selector_widget, FALSE, TRUE, 0);

    /* our desired size for the image area */
    gtk_widget_set_size_request(priv->stack_views, VIDEO_WIDTH, VIDEO_HEIGHT);

    /* Signals used to handle backing surface */
    g_signal_connect(priv->selector_widget, "draw", G_CALLBACK (selector_widget_draw), self);
    g_signal_connect(priv->selector_widget,"configure-event", G_CALLBACK (selector_widget_configure_event), self);
    /* Event signals */
    g_signal_connect(priv->selector_widget, "motion-notify-event", G_CALLBACK(selector_widget_motion_notify_event), self);
    g_signal_connect(priv->selector_widget, "button-press-event", G_CALLBACK(selector_widget_button_press_event), self);
    g_signal_connect(priv->selector_widget, "button-release-event", G_CALLBACK(selector_widget_button_release_event), self);
    /* Ask to receive events the drawing area doesn't normally subscribe to */
    gtk_widget_set_events (priv->selector_widget, gtk_widget_get_events (priv->selector_widget)
                           | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                           | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);


    /* signals */
    g_signal_connect_swapped(priv->button_choose_picture, "clicked", G_CALLBACK(choose_picture), self);
    g_signal_connect_swapped(priv->button_take_photo, "clicked", G_CALLBACK(take_a_photo), self);
    g_signal_connect_swapped(priv->button_set_avatar, "clicked", G_CALLBACK(set_avatar), self);
    g_signal_connect_swapped(priv->button_trash_avatar, "clicked", G_CALLBACK(trash_photo), self);
    g_signal_connect_swapped(priv->button_export_avatar, "clicked", G_CALLBACK(export_avatar), self);

    set_state(self, AVATAR_MANIPULATION_STATE_CURRENT);

    gtk_widget_show_all(priv->stack_views);
}

static void
set_state(AvatarManipulation *self, AvatarManipulationState state)
{
    // note: this funciton does not check if the state transition is valid, this is assumed to have
    // been done by the caller
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    switch (state) {
        case AVATAR_MANIPULATION_STATE_CURRENT:
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
            // gtk_widget_set_size_request(priv->video_widget, VIDEO_WIDTH, VIDEO_HEIGHT);

            /* available actions: trash or export avatar */
            gtk_widget_set_visible(priv->button_trash_avatar,   true);
            gtk_widget_set_visible(priv->button_export_avatar,  true);
            gtk_widget_set_visible(priv->button_set_avatar,     false);
            gtk_widget_set_visible(priv->button_take_photo,     false);
            gtk_widget_set_visible(priv->button_choose_picture, false);

            /* make sure video widget and camera is no longer running */
            Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->stack_views), priv->video_widget);
                priv->video_widget = NULL;
            }
        }
        break;
        case AVATAR_MANIPULATION_STATE_NEW:
        {
            /* check if a video device is available, if so show the camera stream; otherwise we can
             * only choose a file, and we keep showing the current/default avatar
             *
             * TODO: update if a video device gets inserted while in this state
             */
            if (Video::DeviceModel::instance().rowCount() > 0) {
                priv->video_widget = video_widget_new();
                // gtk_widget_set_size_request(priv->video_widget, VIDEO_WIDTH, VIDEO_HEIGHT);
                video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                                Video::PreviewManager::instance().previewRenderer(),
                                                VIDEO_RENDERER_REMOTE);
                g_signal_connect_swapped(priv->video_widget, "snapshot-taken", G_CALLBACK (got_snapshot), self);
                gtk_widget_set_vexpand_set(priv->video_widget, FALSE);
                gtk_widget_set_hexpand_set(priv->video_widget, FALSE);
                gtk_widget_set_visible(priv->video_widget, true);
                gtk_stack_add_named(GTK_STACK(priv->stack_views), priv->video_widget, "page_photobooth");
                gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_photobooth");

                Video::PreviewManager::instance().startPreview();

                /* available action in this case: take snapshot */
                gtk_widget_set_visible(priv->button_take_photo, true);
            } else {
                gtk_widget_set_visible(priv->button_take_photo, false);
            }

            /* available actions: choose file */
            gtk_widget_set_visible(priv->button_choose_picture, true);
            gtk_widget_set_visible(priv->button_trash_avatar,   false);
            gtk_widget_set_visible(priv->button_export_avatar,  false);
            gtk_widget_set_visible(priv->button_set_avatar,     false);
        }
        break;
        case AVATAR_MANIPULATION_STATE_EDIT:
        {
            /* make sure video widget and camera is no longer running */
            Video::PreviewManager::instance().stopPreview();
            if (priv->video_widget) {
                gtk_container_remove(GTK_CONTAINER(priv->stack_views), priv->video_widget);
                priv->video_widget = NULL;
            }

            /* reset the selector */
            priv->origin[0] = 0;
            priv->origin[1] = 0;
            priv->length = INITIAL_LENTGH;

            /* available actions: set avatar, trash avatar */
            gtk_widget_set_visible(priv->button_set_avatar, true);
            gtk_widget_set_visible(priv->button_trash_avatar, true);
            gtk_widget_set_visible(priv->button_take_photo, false);
            gtk_widget_set_visible(priv->button_choose_picture, false);
            gtk_widget_set_visible(priv->button_export_avatar, false);

            gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_views), "page_edit_view");
        }
        break;
    }

    priv->state = state;
}

static void
take_a_photo(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    video_widget_take_snapshot(VIDEO_WIDGET(priv->video_widget));
}

static void
trash_photo(AvatarManipulation *self)
{
    /* clear the avatar in the profile */
    ProfileModel::instance().selectedProfile()->person()->setPhoto(QVariant());
    ProfileModel::instance().selectedProfile()->save();

    set_state(self, AVATAR_MANIPULATION_STATE_NEW);
}

static void
set_avatar(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    gchar* png_buffer_signed = nullptr;
    gsize png_buffer_size;
    GError* error =  nullptr;

    /* get the selected zone */
    GdkPixbuf *selector_pixbuf = gdk_pixbuf_new_subpixbuf(priv->pix_scaled, priv->origin[0], priv->origin[1],
                                                          priv->length, priv->length);

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
    QVariant photo = GlobalInstances::pixmapManipulator().personPhoto(png_q_byte_array);
    ProfileModel::instance().selectedProfile()->person()->setPhoto(photo);
    ProfileModel::instance().selectedProfile()->save();

    g_free(png_buffer_signed);
    g_object_unref(selector_pixbuf);
    g_object_unref(pixbuf_frame_resized);

    set_state(self, AVATAR_MANIPULATION_STATE_CURRENT);
}

static void
choose_picture(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    GtkFileFilter *filter = gtk_file_filter_new ();
    gint res;

    auto preview = gtk_image_new();

    GtkWidget *ring_main_window = gtk_widget_get_toplevel(GTK_WIDGET(self));

    auto dialog = gtk_file_chooser_dialog_new (_("Open Avatar Image"),
                                          GTK_WINDOW(ring_main_window),
                                          action,
                                          _("_Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          _("_Open"),
                                          GTK_RESPONSE_ACCEPT,
                                          NULL);

    /* add filters */
    gtk_file_filter_add_pattern (filter,"*.png");
    gtk_file_filter_add_pattern (filter,"*.jpg");
    gtk_file_chooser_set_filter (GTK_FILE_CHOOSER(dialog),filter);

    /* add an image preview inside the file choose */
    gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER(dialog), preview);
    g_signal_connect (GTK_FILE_CHOOSER(dialog), "update-preview", G_CALLBACK (update_preview_cb), preview);

    /* start the file chooser */
    res = gtk_dialog_run (GTK_DIALOG(dialog)); /* blocks until the dialog is closed */

    if (res == GTK_RESPONSE_ACCEPT) {
        if(auto filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (dialog))) {
            GError* error =  nullptr; /* initialising to null avoid trouble... */

            if (priv->pix_scaled) {
                g_object_unref(priv->pix_scaled);
                priv->pix_scaled = nullptr;
            }
            priv->pix_scaled = gdk_pixbuf_new_from_file_at_size (filename, VIDEO_WIDTH, VIDEO_HEIGHT, &error);

            if (!error) {
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

    gtk_widget_destroy(dialog);
}

static void
export_avatar(AvatarManipulation *self)
{
    GtkWidget *ring_main_window = gtk_widget_get_toplevel(GTK_WIDGET(self));

    auto dialog = gtk_file_chooser_dialog_new(_("Save Avatar Image"),
                                              GTK_WINDOW(ring_main_window),
                                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                              _("_Cancel"),
                                              GTK_RESPONSE_CANCEL,
                                              _("_Save"),
                                              GTK_RESPONSE_ACCEPT,
                                              NULL);

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    /* start the file chooser */
    auto res = gtk_dialog_run (GTK_DIALOG (dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        if (auto filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog))) {
            GError* error =  nullptr;
            auto photo = ProfileModel::instance().selectedProfile()->person()->photo();
            std::shared_ptr<GdkPixbuf> pixbuf_photo = photo.value<std::shared_ptr<GdkPixbuf>>();

            if (photo.isValid()) {
                gdk_pixbuf_save(pixbuf_photo.get(), filename, "png", &error, NULL);
                if (error){
                    g_warning("(export_avatar) could not save avatar to file: %s\n", error->message);
                    g_error_free(error);
                }
            }
            g_free (filename);
        }
    }

    gtk_widget_destroy (dialog);
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

static gboolean
selector_widget_configure_event(G_GNUC_UNUSED GtkWidget *widget,
                                G_GNUC_UNUSED GdkEventConfigure *event,
                                AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);
    if (priv->selector_widget_surface) {
        cairo_surface_destroy(priv->selector_widget_surface);
        priv->selector_widget_surface = nullptr;
    }

    priv->selector_widget_surface = gdk_window_create_similar_surface (gtk_widget_get_window (widget),
                                               CAIRO_CONTENT_COLOR,
                                               allocation.width,
                                               allocation.height);

    /* TRUE = do not propagate */
    return TRUE;
}

static gboolean
selector_widget_draw(G_GNUC_UNUSED GtkWidget *widget, cairo_t *cr, AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    if(!priv->pix_scaled)
        return FALSE;

    int w = gdk_pixbuf_get_width(priv->pix_scaled);
    int h = gdk_pixbuf_get_height(priv->pix_scaled);
    gtk_widget_set_size_request(priv->selector_widget, w, h);

    /* add the snapshot/picture on it */
    gdk_cairo_set_source_pixbuf(cr, priv->pix_scaled, 0, 0);
    cairo_paint(cr);

    /* dark around the selector : */
    cairo_set_source_rgba(cr, 0., 0., 0., 0.4);
    cairo_set_line_width(cr, 2);
    /* left */
    cairo_rectangle(cr, 0, 0, priv->origin[0], VIDEO_HEIGHT);
    cairo_fill(cr);
    /* right */
    cairo_rectangle(cr, priv->origin[0]+priv->length, 0, VIDEO_WIDTH, VIDEO_HEIGHT);
    cairo_fill(cr);
    /* up */
    cairo_rectangle(cr, priv->origin[0], 0, priv->length, priv->origin[1]);
    cairo_fill(cr);
    /* down */
    cairo_rectangle(cr, priv->origin[0], priv->origin[1]+priv->length, priv->length, VIDEO_HEIGHT);
    cairo_fill(cr);

    /* black border around the selector */
    cairo_set_source_rgb(cr, 0., 0., 0.);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, priv->origin[0], priv->origin[1], priv->length, priv->length);
    cairo_stroke(cr);

    /* white border around the selector */
    cairo_set_source_rgb(cr, 1., 1., 1.);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(cr, priv->origin[0]+2, priv->origin[1]+2, priv->length-4, priv->length-4);
    cairo_stroke(cr);

    /* crosshair */
    cairo_set_line_width(cr, 1);
    double lg = (double)priv->length;
    if(priv->do_action) {
        /* horizontales */
        cairo_move_to(cr, priv->origin[0]+((int)(lg/3.0))+2, priv->origin[1]+2);
        cairo_line_to(cr, priv->origin[0]+((int)(lg/3.0))+2, priv->origin[1]+priv->length-4);
        cairo_move_to(cr, priv->origin[0]+((int)(2.0*lg/3.0))+2, priv->origin[1]+2);
        cairo_line_to(cr, priv->origin[0]+((int)(2.0*lg/3.0))+2, priv->origin[1]+priv->length-4);
        /* verticales */
        cairo_move_to(cr, priv->origin[0]+2, priv->origin[1]+((int)(lg/3.0))+2);
        cairo_line_to(cr, priv->origin[0]+priv->length-4, priv->origin[1]+((int)(lg/3.0))+2);
        cairo_move_to(cr, priv->origin[0]+2, priv->origin[1]+((int)(2.0*lg/3.0))+2);
        cairo_line_to(cr, priv->origin[0]+priv->length-4, priv->origin[1]+((int)(2.0*lg/3.0))+2);
        cairo_stroke(cr);
    }

    return TRUE;
}

static gboolean
selector_widget_motion_notify_event(G_GNUC_UNUSED GtkWidget *widget, GdkEventMotion *event, AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    int x, y;
    GdkModifierType state;
    gdk_window_get_device_position (event->window, event->device, &x, &y, &state);

    if (priv->do_action && state == priv->button_pressed) {
        switch (priv->action_on_selector) {
        case MOVE_SELECTOR:
            update_and_draw( x - priv->relative_position[0], y - priv->relative_position[1] , self );
        break;
        case PICK_SELECTOR:
            rescale( x, y , self );
        break;
        }
    } else { /* is the pointer just over the selector ? */
        if (x > priv->origin[0] && x < priv->origin[0] +priv->length
                                && y > priv->origin[1] && y < priv->origin[1] + priv->length) {
            GdkWindow *window = gtk_widget_get_window( GTK_WIDGET(priv->selector_widget));
            GdkCursor *cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_FLEUR);
            gdk_window_set_cursor(window, cursor);
            priv->action_on_selector = MOVE_SELECTOR;
        } else {
            GdkWindow *window = gtk_widget_get_window(GTK_WIDGET(priv->selector_widget));
            GdkCursor *cursor = gdk_cursor_new_for_display(gdk_display_get_default(), GDK_CROSSHAIR);
            gdk_window_set_cursor(window, cursor);
            priv->action_on_selector = PICK_SELECTOR;
        }
    }

  return TRUE;
}

static gboolean
selector_widget_button_press_event(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    int x, y;
    GdkModifierType state;
    gdk_window_get_device_position (event->window, event->device, &x, &y, &state);

    switch (priv->action_on_selector) {
    case MOVE_SELECTOR:
        priv->do_action = true;
        priv->relative_position[0] = x - priv->origin[0];
        priv->relative_position[1] = y - priv->origin[1];
    break;
    case PICK_SELECTOR:
        priv->do_action = true;
        priv->length = 0;
        update_and_draw( x, y , self );
    break;
    }

    priv->button_pressed = state;

  return TRUE;
}

static gboolean
selector_widget_button_release_event(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    int x, y;
    GdkModifierType state;
    gdk_window_get_device_position (event->window, event->device, &x, &y, &state);

    if (priv->do_action)
        switch ( priv->action_on_selector ) {
        case MOVE_SELECTOR:
            update_and_draw( x-priv->relative_position[0], y-priv->relative_position[1], self );
        break;
        case PICK_SELECTOR:
            update_and_draw( priv->origin[0], priv->origin[1], self );
        break;
        }

    priv->do_action = false;

    return TRUE;
}

static void
update_and_draw(gdouble x, gdouble y, AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    GdkRectangle update_rect;
    cairo_t *cr;

    if (!priv->pix_scaled) {
        g_warning("(update_and_draw) pix_scaled is null");
        return;
    }
    auto width = gdk_pixbuf_get_width(priv->pix_scaled);
    auto height = gdk_pixbuf_get_height(priv->pix_scaled);

    update_rect.x = ( ( x - priv->origin[0] < 0 ) ? x : priv->origin[0] ) - 30;
    update_rect.y = ( ( y - priv->origin[1] < 0 ) ? y : priv->origin[1] ) - 30;
    update_rect.width = width - update_rect.x;
    update_rect.height = height - update_rect.y;

    if (x > width - priv->length)
        priv->origin[0] = (x > width - priv->length) ? width - priv->length : x;
    else
        priv->origin[0] = (x > 0) ? x : 0;

    if (y > height - priv->length )
        priv->origin[1] = (y > height - priv->length ) ? height - priv->length : y;
    else
        priv->origin[1] = (y > 0) ? y : 0;

    /* cairo operations */
    cr = cairo_create(priv->selector_widget_surface);
    gdk_cairo_rectangle(cr, &update_rect);
    cairo_fill(cr);
    cairo_destroy(cr);

    /* invalidate the affected region */
    gdk_window_invalidate_rect(gtk_widget_get_window (priv->selector_widget), &update_rect, FALSE);
}

static void
rescale(gdouble x, gdouble y, AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);

    GdkRectangle update_rect;
    cairo_t *cr;

    if (!priv->pix_scaled) {
        g_warning("(rescale) pix_scaled is null");
        return;
    }
    auto width = gdk_pixbuf_get_width(priv->pix_scaled);
    auto height = gdk_pixbuf_get_height(priv->pix_scaled);

    update_rect.x = ( ( x - priv->origin[0] < 0 ) ? x : priv->origin[0] ) - 3;
    update_rect.y = ( ( y - priv->origin[1] < 0 ) ? y : priv->origin[1] ) - 3;
    update_rect.width = width - update_rect.x;
    update_rect.height = height - update_rect.y;

    int old_length = priv->length;
    priv->length = sqrt( (priv->origin[0] - x)*(priv->origin[0] - x) + (priv->origin[1] - y)*(priv->origin[1] - y) );

    if (priv->length < 10)
        priv->length = 10;

    if (priv->origin[0] + priv->length > width)
        priv->length = old_length;

    if (priv->origin[1] + priv->length > height)
        priv->length = old_length;

    /* cairo operations */
    cr = cairo_create(priv->selector_widget_surface);

    gdk_cairo_rectangle(cr, &update_rect);
    cairo_fill(cr);

    cairo_destroy(cr);

    /* invalidate the affected region */
    gdk_window_invalidate_rect(gtk_widget_get_window (priv->selector_widget), &update_rect, FALSE);
}

static void
got_snapshot(AvatarManipulation *self)
{
    AvatarManipulationPrivate *priv = AVATAR_MANIPULATION_GET_PRIVATE(self);
    GdkPixbuf* pix = video_widget_get_snapshot(VIDEO_WIDGET(priv->video_widget));

    /* in this case we have to deal with the aspect ratio */
    float w = ((float)gdk_pixbuf_get_width(pix));
    float h = ((float)gdk_pixbuf_get_height(pix));
    const float ratio = h/w;
    const float W = VIDEO_WIDTH;
    const float H = VIDEO_HEIGHT;

    if (h > w) {
        h = H;
        w = h / ratio;
    } else {
        w = W;
        h = w * ratio;
    }

    if (priv->pix_scaled) {
        g_object_unref(priv->pix_scaled);
        priv->pix_scaled = nullptr;
    }
    priv->pix_scaled = gdk_pixbuf_scale_simple(pix, w, h, GDK_INTERP_HYPER);

    set_state(self, AVATAR_MANIPULATION_STATE_EDIT);
}
