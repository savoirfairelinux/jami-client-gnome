/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

#include "currentcallview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <call.h>
#include <callmodel.h>
#include "utils/drawing.h"
#include "video/video_widget.h"
#include <video/previewmanager.h>
#include <contactmethod.h>
#include <person.h>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include <media/media.h>
#include <media/text.h>
#include <media/textrecording.h>
#include "models/gtkqtreemodel.h"
#include "ringnotify.h"
#include <codecmodel.h>
#include <account.h>
#include "utils/files.h"
#include <clutter-gtk/clutter-gtk.h>
#include "chatview.h"
#include <itemdataroles.h>
#include <numbercategory.h>
#include <smartinfohub.h>

static constexpr int CONTROLS_FADE_TIMEOUT = 3000000; /* microseconds */
static constexpr int FADE_DURATION = 500; /* miliseconds */

struct _CurrentCallView
{
    GtkBox parent;
};

struct _CurrentCallViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _CurrentCallViewPrivate CurrentCallViewPrivate;

struct _CurrentCallViewPrivate
{
    GtkWidget *hbox_call_info;
    GtkWidget *hbox_call_controls;
    GtkWidget *vbox_call_smartInfo;
    GtkWidget *image_peer;
    GtkWidget *label_name;
    GtkWidget *label_uri;
    GtkWidget *label_status;
    GtkWidget *label_duration;
    GtkWidget *label_smartinfo_description;
    GtkWidget *label_smartinfo_value;
    GtkWidget *label_smartinfo_general_information;
    GtkWidget *paned_call;
    GtkWidget *frame_video;
    GtkWidget *video_widget;
    GtkWidget *frame_chat;
    GtkWidget *togglebutton_chat;
    GtkWidget *button_hangup;
    GtkWidget *scalebutton_quality;
    GtkWidget *checkbutton_autoquality;

    /* The webkit_chat_container is created once, then reused for all chat
     * views */
    GtkWidget *webkit_chat_container;

    /* flag used to keep track of the video quality scale pressed state;
     * we do not want to update the codec bitrate until the user releases the
     * scale button */
    gboolean quality_scale_pressed;

    Call *call;

    QMetaObject::Connection state_change_connection;
    QMetaObject::Connection call_details_connection;
    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection remote_renderer_connection;

    GSettings *settings;

    // for clutter animations and to know when to fade in/out the overlays
    ClutterTransition *fade_info;
    ClutterTransition *fade_controls;
    gint64 time_last_mouse_motion;
    guint timer_fade;

    // smart info
    QMetaObject::Connection smartinfo_refresh_connection;
    guint smartinfo_action;
};

G_DEFINE_TYPE_WITH_PRIVATE(CurrentCallView, current_call_view, GTK_TYPE_BOX);

#define CURRENT_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CURRENT_CALL_VIEW_TYPE, CurrentCallViewPrivate))

enum {
    VIDEO_DOUBLE_CLICKED,
    LAST_SIGNAL
};

static guint current_call_view_signals[LAST_SIGNAL] = { 0 };

static void
current_call_view_dispose(GObject *object)
{
    CurrentCallView *view;
    CurrentCallViewPrivate *priv;

    view = CURRENT_CALL_VIEW(object);
    priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->state_change_connection);
    QObject::disconnect(priv->call_details_connection);
    QObject::disconnect(priv->local_renderer_connection);
    QObject::disconnect(priv->remote_renderer_connection);
    QObject::disconnect(priv->smartinfo_refresh_connection);
    g_clear_object(&priv->settings);

    g_source_remove(priv->timer_fade);

    auto display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()), "display-smartinfo");
    g_signal_handler_disconnect(display_smartinfo, priv->smartinfo_action);

    G_OBJECT_CLASS(current_call_view_parent_class)->dispose(object);
}

static void
show_chat_view(CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_chat), TRUE);
}

static void
chat_toggled(GtkToggleButton *togglebutton, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    if (gtk_toggle_button_get_active(togglebutton)) {
        gtk_widget_show_all(priv->frame_chat);
        gtk_widget_grab_focus(priv->frame_chat);
    } else {
        gtk_widget_hide(priv->frame_chat);
    }
}

static gboolean
map_boolean_to_orientation(GValue *value, GVariant *variant, G_GNUC_UNUSED gpointer user_data)
{
    if (g_variant_is_of_type(variant, G_VARIANT_TYPE_BOOLEAN)) {
        if (g_variant_get_boolean(variant)) {
            // true, chat should be horizontal (to the right)
            g_value_set_enum(value, GTK_ORIENTATION_HORIZONTAL);
        } else {
            // false, chat should be vertical (at the bottom)
            g_value_set_enum(value, GTK_ORIENTATION_VERTICAL);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean
timeout_check_last_motion_event(CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), G_SOURCE_REMOVE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    auto current_time = g_get_monotonic_time();
    if (current_time - priv->time_last_mouse_motion >= CONTROLS_FADE_TIMEOUT) {
        // timeout has passed, hide the controls
        if (clutter_timeline_get_direction(CLUTTER_TIMELINE(priv->fade_info)) == CLUTTER_TIMELINE_BACKWARD) {
            clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->fade_info), CLUTTER_TIMELINE_FORWARD);
            clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->fade_controls), CLUTTER_TIMELINE_FORWARD);
            if (!clutter_timeline_is_playing(CLUTTER_TIMELINE(priv->fade_info))) {
                clutter_timeline_rewind(CLUTTER_TIMELINE(priv->fade_info));
                clutter_timeline_rewind(CLUTTER_TIMELINE(priv->fade_controls));
                clutter_timeline_start(CLUTTER_TIMELINE(priv->fade_info));
                clutter_timeline_start(CLUTTER_TIMELINE(priv->fade_controls));
            }
        }
    }

    return G_SOURCE_CONTINUE;
}

static gboolean
mouse_moved(CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    priv->time_last_mouse_motion = g_get_monotonic_time();

    // since the mouse moved, make sure the controls are shown
    if (clutter_timeline_get_direction(CLUTTER_TIMELINE(priv->fade_info)) == CLUTTER_TIMELINE_FORWARD) {
        clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->fade_info), CLUTTER_TIMELINE_BACKWARD);
        clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->fade_controls), CLUTTER_TIMELINE_BACKWARD);
        if (!clutter_timeline_is_playing(CLUTTER_TIMELINE(priv->fade_info))) {
            clutter_timeline_rewind(CLUTTER_TIMELINE(priv->fade_info));
            clutter_timeline_rewind(CLUTTER_TIMELINE(priv->fade_controls));
            clutter_timeline_start(CLUTTER_TIMELINE(priv->fade_info));
            clutter_timeline_start(CLUTTER_TIMELINE(priv->fade_controls));
        }
    }

    return FALSE; // propogate event
}

static ClutterTransition *
create_fade_out_transition()
{
    auto transition  = clutter_property_transition_new("opacity");
    clutter_transition_set_from(transition, G_TYPE_UINT, 255);
    clutter_transition_set_to(transition, G_TYPE_UINT, 0);
    clutter_timeline_set_duration(CLUTTER_TIMELINE(transition), FADE_DURATION);
    clutter_timeline_set_repeat_count(CLUTTER_TIMELINE(transition), 0);
    clutter_timeline_set_progress_mode(CLUTTER_TIMELINE(transition), CLUTTER_EASE_IN_OUT_CUBIC);
    return transition;
}

static gboolean
video_widget_focus(GtkWidget *widget, GtkDirectionType direction, CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    // if this widget already has focus, we want the focus to move to the next widget, otherwise we
    // will get stuck in a focus loop on the buttons
    if (gtk_widget_has_focus(widget))
        return FALSE;

    // otherwise we want the focus to go to and change between the call control buttons
    if (gtk_widget_child_focus(GTK_WIDGET(priv->hbox_call_controls), direction)) {
        // selected a child, make sure call controls are shown
        mouse_moved(self);
        return TRUE;
    }

    // did not select the next child, propogate the event
    return FALSE;
}

static GtkBox *
gtk_scale_button_get_box(GtkScaleButton *button)
{
    GtkWidget *box = NULL;
    if (auto dock = gtk_scale_button_get_popup(button)) {
        // the dock is a popover which contains the box
        box = gtk_bin_get_child(GTK_BIN(dock));
        if (box) {
            if (GTK_IS_FRAME(box)) {
                // support older versions of gtk; the box used to be in a frame
                box = gtk_bin_get_child(GTK_BIN(box));
            }
        }
    }

    return GTK_BOX(box);
}

/**
 * This gets the GtkScaleButtonScale widget (which is a GtkScale) from the
 * given GtkScaleButton in order to be able to modify its properties and connect
 * to its signals
 */
static GtkScale *
gtk_scale_button_get_scale(GtkScaleButton *button)
{
    GtkScale *scale = NULL;

    if (auto box = gtk_scale_button_get_box(button)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(box));
        for (GList *c = children; c && !scale; c = c->next) {
            if (GTK_IS_SCALE(c->data))
                scale = GTK_SCALE(c->data);
        }
        g_list_free(children);
    }

    return scale;
}

static void
set_quality(Call *call, gboolean auto_quality_on, double desired_quality)
{
    /* set auto quality true or false, also set the bitrate and quality values;
     * the slider is from 0 to 100, use the min and max vals to scale each value accordingly */
    if (const auto& codecModel = call->account()->codecModel()) {
        const auto& videoCodecs = codecModel->videoCodecs();

        for (int i=0; i < videoCodecs->rowCount();i++) {
            const auto& idx = videoCodecs->index(i,0);

            if (auto_quality_on) {
                // g_debug("enable auto quality");
                videoCodecs->setData(idx, "true", CodecModel::Role::AUTO_QUALITY_ENABLED);
            } else {
                auto min_bitrate = idx.data(static_cast<int>(CodecModel::Role::MIN_BITRATE)).toInt();
                auto max_bitrate = idx.data(static_cast<int>(CodecModel::Role::MAX_BITRATE)).toInt();
                auto min_quality = idx.data(static_cast<int>(CodecModel::Role::MIN_QUALITY)).toInt();
                auto max_quality = idx.data(static_cast<int>(CodecModel::Role::MAX_QUALITY)).toInt();

                // g_debug("bitrate min: %d, max: %d, quality min: %d, max: %d", min_bitrate, max_bitrate, min_quality, max_quality);

                double bitrate;
                bitrate = min_bitrate + (double)(max_bitrate - min_bitrate)*(desired_quality/100.0);
                if (bitrate < 0) bitrate = 0;

                double quality;
                // note: a lower value means higher quality
                quality = (double)min_quality - (min_quality - max_quality)*(desired_quality/100.0);
                if (quality < 0) quality = 0;

                // g_debug("disable auto quality; %% quality: %d; bitrate: %d; quality: %d", (int)desired_quality, (int)bitrate, (int)quality);
                videoCodecs->setData(idx, "false", CodecModel::Role::AUTO_QUALITY_ENABLED);
                videoCodecs->setData(idx, QString::number((int)bitrate), CodecModel::Role::BITRATE);
                videoCodecs->setData(idx, QString::number((int)quality), CodecModel::Role::QUALITY);
            }
        }
        codecModel << CodecModel::EditAction::SAVE;
    }
}

static void
autoquality_toggled(GtkToggleButton *button, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    gboolean auto_quality_on = gtk_toggle_button_get_active(button);

    auto scale = gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality));
    auto plus_button = gtk_scale_button_get_plus_button(GTK_SCALE_BUTTON(priv->scalebutton_quality));
    auto minus_button = gtk_scale_button_get_minus_button(GTK_SCALE_BUTTON(priv->scalebutton_quality));

    gtk_widget_set_sensitive(GTK_WIDGET(scale), !auto_quality_on);
    gtk_widget_set_sensitive(plus_button, !auto_quality_on);
    gtk_widget_set_sensitive(minus_button, !auto_quality_on);

    double desired_quality = gtk_scale_button_get_value(GTK_SCALE_BUTTON(priv->scalebutton_quality));

    if (priv->call)
        set_quality(priv->call, auto_quality_on, desired_quality);
}

static void
quality_changed(GtkScaleButton *button, G_GNUC_UNUSED gdouble value, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    /* no need to upate quality if auto quality is enabled */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkbutton_autoquality))) return;

    /* only update if the scale button is released, to reduce the number of updates */
    if (priv->quality_scale_pressed) return;

    /* we get the value directly from the widget, in case this function is not
     * called from the event */
    if (priv->call)
        set_quality(priv->call, FALSE, gtk_scale_button_get_value(button));
}

static gboolean
quality_button_pressed(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event, CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    priv->quality_scale_pressed = TRUE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
quality_button_released(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event, CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    priv->quality_scale_pressed = FALSE;

    /* now make sure the quality gets updated */
    quality_changed(GTK_SCALE_BUTTON(priv->scalebutton_quality), 0, self);

    return GDK_EVENT_PROPAGATE;
}

static void
current_call_view_init(CurrentCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* create video widget and overlay the call info and controls on it */
    priv->video_widget = video_widget_new();
    gtk_container_add(GTK_CONTAINER(priv->frame_video), priv->video_widget);
    gtk_widget_show_all(priv->frame_video);

    auto stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->video_widget));
    auto actor_info = gtk_clutter_actor_new_with_contents(priv->hbox_call_info);
    auto actor_controls = gtk_clutter_actor_new_with_contents(priv->hbox_call_controls);
    auto actor_smartInfo = gtk_clutter_actor_new_with_contents(priv->vbox_call_smartInfo);

    clutter_actor_add_child(stage, actor_info);
    clutter_actor_set_x_align(actor_info, CLUTTER_ACTOR_ALIGN_FILL);
    clutter_actor_set_y_align(actor_info, CLUTTER_ACTOR_ALIGN_START);

    clutter_actor_add_child(stage, actor_controls);
    clutter_actor_set_x_align(actor_controls, CLUTTER_ACTOR_ALIGN_CENTER);
    clutter_actor_set_y_align(actor_controls, CLUTTER_ACTOR_ALIGN_END);

    clutter_actor_add_child(stage, actor_smartInfo);
    clutter_actor_set_x_align(actor_smartInfo, CLUTTER_ACTOR_ALIGN_END);
    clutter_actor_set_y_align(actor_smartInfo, CLUTTER_ACTOR_ALIGN_START);
    ClutterMargin clutter_margin_smartInfo;
    clutter_margin_smartInfo.top = 50;
    clutter_margin_smartInfo.right = 10;
    clutter_actor_set_margin (actor_smartInfo, &clutter_margin_smartInfo);

    /* add fade in and out states to the info and controls */
    priv->time_last_mouse_motion = g_get_monotonic_time();
    priv->fade_info = create_fade_out_transition();
    priv->fade_controls = create_fade_out_transition();
    clutter_actor_add_transition(actor_info, "fade_info", priv->fade_info);
    clutter_actor_add_transition(actor_controls, "fade_controls", priv->fade_controls);
    clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->fade_info), CLUTTER_TIMELINE_BACKWARD);
    clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->fade_controls), CLUTTER_TIMELINE_BACKWARD);
    clutter_timeline_stop(CLUTTER_TIMELINE(priv->fade_info));
    clutter_timeline_stop(CLUTTER_TIMELINE(priv->fade_controls));

    /* have a timer check every 1 second if the controls should fade out */
    priv->timer_fade = g_timeout_add(1000, (GSourceFunc)timeout_check_last_motion_event, view);

    /* connect to the mouse motion event to reset the last moved time */
    g_signal_connect_swapped(priv->video_widget, "motion-notify-event", G_CALLBACK(mouse_moved), view);
    g_signal_connect_swapped(priv->video_widget, "button-press-event", G_CALLBACK(mouse_moved), view);
    g_signal_connect_swapped(priv->video_widget, "button-release-event", G_CALLBACK(mouse_moved), view);

    /* manually handle the focus of the video widget to be able to focus on the call controls */
    g_signal_connect(priv->video_widget, "focus", G_CALLBACK(video_widget_focus), view);

    /* toggle whether or not the chat is displayed */
    g_signal_connect(priv->togglebutton_chat, "toggled", G_CALLBACK(chat_toggled), view);

    /* bind the chat orientation to the gsetting */
    priv->settings = g_settings_new_full(get_ring_schema(), NULL, NULL);
    g_settings_bind_with_mapping(priv->settings, "chat-pane-horizontal",
                                 priv->paned_call, "orientation",
                                 G_SETTINGS_BIND_GET,
                                 map_boolean_to_orientation,
                                 nullptr, nullptr, nullptr);

    g_signal_connect(priv->scalebutton_quality, "value-changed", G_CALLBACK(quality_changed), view);
    /* customize the quality button scale */
    if (auto scale_box = gtk_scale_button_get_box(GTK_SCALE_BUTTON(priv->scalebutton_quality))) {
        priv->checkbutton_autoquality = gtk_check_button_new_with_label(C_("Enable automatic video quality", "Auto"));
        gtk_widget_show(priv->checkbutton_autoquality);
        gtk_box_pack_start(GTK_BOX(scale_box), priv->checkbutton_autoquality, FALSE, TRUE, 0);
        g_signal_connect(priv->checkbutton_autoquality, "toggled", G_CALLBACK(autoquality_toggled), view);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_autoquality), TRUE);
    }
    if (auto scale = gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality))) {
        g_signal_connect(scale, "button-press-event", G_CALLBACK(quality_button_pressed), view);
        g_signal_connect(scale, "button-release-event", G_CALLBACK(quality_button_released), view);
    }
}

static void
current_call_view_class_init(CurrentCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = current_call_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/currentcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, hbox_call_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, hbox_call_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, vbox_call_smartInfo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, image_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_uri);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_description);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_value);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_general_information);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, paned_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, button_hangup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, scalebutton_quality);

    current_call_view_signals[VIDEO_DOUBLE_CLICKED] = g_signal_new (
        "video-double-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
update_state(CurrentCallView *view, Call *call)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    gchar *status = g_strdup_printf("%s", call->toHumanStateName().toUtf8().constData());

    gtk_label_set_text(GTK_LABEL(priv->label_status), status);

    g_free(status);
}

static void
update_details(CurrentCallView *view, Call *call)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* update call duration */
    QByteArray ba_length = call->length().toLocal8Bit();
    gtk_label_set_text(GTK_LABEL(priv->label_duration), ba_length.constData());
}

static void
update_smartInfo(CurrentCallView *view)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    if (!SmartInfoHub::instance().isConference()) {
        gchar* general_information = g_strdup_printf("Call ID: %s", SmartInfoHub::instance().callID().toStdString().c_str());
        gtk_label_set_text(GTK_LABEL(priv->label_smartinfo_general_information), general_information);
        g_free(general_information);

        gchar* description = g_strdup_printf("You\n"
                                             "Framerate:\n"
                                             "Video codec:\n"
                                             "Audio codec:\n"
                                             "Resolution:\n\n"
                                             "Peer\n"
                                             "Framerate:\n"
                                             "Video codec:\n"
                                             "Audio codec:\n"
                                             "Resolution:");
        gtk_label_set_text(GTK_LABEL(priv->label_smartinfo_description),description);
        g_free(description);

        gchar* value = g_strdup_printf("\n%f\n%s\n%s\n%dx%d\n\n\n%f\n%s\n%s\n%dx%d",
                                       (double)SmartInfoHub::instance().localFps(),
                                       SmartInfoHub::instance().localVideoCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().localAudioCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().localWidth(),
                                       SmartInfoHub::instance().localHeight(),
                                       (double)SmartInfoHub::instance().remoteFps(),
                                       SmartInfoHub::instance().remoteVideoCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().remoteAudioCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().remoteWidth(),
                                       SmartInfoHub::instance().remoteHeight());
        gtk_label_set_text(GTK_LABEL(priv->label_smartinfo_value),value);
        g_free(value);
    } else {
        gchar* general_information = g_strdup_printf("Conference ID: %s", SmartInfoHub::instance().callID().toStdString().c_str());
        gtk_label_set_text(GTK_LABEL(priv->label_smartinfo_general_information), general_information);
        g_free(general_information);

        gchar* description = g_strdup_printf("You\n"
                                             "Framerate:\n"
                                             "Video codec:\n"
                                             "Audio codec:\n"
                                             "Resolution:");
        gtk_label_set_text(GTK_LABEL(priv->label_smartinfo_description),description);
        g_free(description);

        gchar* value = g_strdup_printf("\n%f\n%s\n%s\n%dx%d",
                                       (double)SmartInfoHub::instance().localFps(),
                                       SmartInfoHub::instance().localVideoCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().localAudioCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().localWidth(),
                                       SmartInfoHub::instance().localHeight());
        gtk_label_set_text(GTK_LABEL(priv->label_smartinfo_value),value);
        g_free(value);
    }
}


static gboolean
on_button_press_in_video_event(GtkWidget *self, GdkEventButton *event, CurrentCallView *view)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);

    /* on double click */
    if (event->type == GDK_2BUTTON_PRESS) {
        g_debug("double click in video");
        g_signal_emit(G_OBJECT(view), current_call_view_signals[VIDEO_DOUBLE_CLICKED], 0);
    }

    return GDK_EVENT_PROPAGATE;
}

static void
toggle_smartinfo(GSimpleAction* action, G_GNUC_UNUSED GVariant* state, GtkWidget* vbox_call_smartInfo)
{
    if (g_variant_get_boolean(g_action_get_state(G_ACTION(action)))) {
        gtk_widget_show(vbox_call_smartInfo);
    } else {
        gtk_widget_hide(vbox_call_smartInfo);
    }
}

static void
set_call_info(CurrentCallView *view, Call *call) {
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->call = call;

    /* get call image */
    QVariant var_i = GlobalInstances::pixmapManipulator().callPhoto(priv->call, QSize(60, 60), false);
    std::shared_ptr<GdkPixbuf> image = var_i.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_peer), image.get());

    /* get name */
    auto name = call->formattedName();
    gtk_label_set_text(GTK_LABEL(priv->label_name), name.toUtf8().constData());

    /* get uri, if different from name */
    auto uri = call->peerContactMethod()->uri();
    if (name != uri) {
        auto cat_uri = g_strdup_printf("(%s) %s"
                                       ,priv->call->peerContactMethod()->category()->name().toUtf8().constData()
                                       ,uri.toUtf8().constData());
        gtk_label_set_text(GTK_LABEL(priv->label_uri), cat_uri);
        g_free(cat_uri);
        gtk_widget_show(priv->label_uri);
    }

    /* change some things depending on call state */
    update_state(view, priv->call);
    update_details(view, priv->call);

    priv->smartinfo_refresh_connection = QObject::connect(
        &SmartInfoHub::instance(),
        &SmartInfoHub::changed,
        [view, priv]() { update_smartInfo(view); }
    );

    priv->state_change_connection = QObject::connect(
        priv->call,
        &Call::stateChanged,
        [view, priv]() { update_state(view, priv->call); }
    );

    priv->call_details_connection = QObject::connect(
        priv->call,
        static_cast<void (Call::*)(void)>(&Call::changed),
        [view, priv]() { update_details(view, priv->call); }
    );

    /* check if we already have a renderer */
    video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                   priv->call->videoRenderer(),
                                   VIDEO_RENDERER_REMOTE);

    /* callback for remote renderer */
    priv->remote_renderer_connection = QObject::connect(
        priv->call,
        &Call::videoStarted,
        [priv](Video::Renderer *renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_REMOTE);
        }
    );

    /* local renderer */
    if (Video::PreviewManager::instance().isPreviewing())
        video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                       Video::PreviewManager::instance().previewRenderer(),
                                       VIDEO_RENDERER_LOCAL);

    /* callback for local renderer */
    priv->local_renderer_connection = QObject::connect(
        &Video::PreviewManager::instance(),
        &Video::PreviewManager::previewStarted,
        [priv](Video::Renderer *renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_LOCAL);
        }
    );

    /* handle video widget button click event */
    g_signal_connect(priv->video_widget, "button-press-event", G_CALLBACK(video_widget_on_button_press_in_screen_event), priv->call);

    /* handle video widget drag and drop*/
    g_signal_connect(priv->video_widget, "drag-data-received", G_CALLBACK(video_widget_on_drag_data_received), priv->call);

    /* catch double click to make full screen */
    g_signal_connect(priv->video_widget, "button-press-event",
                     G_CALLBACK(on_button_press_in_video_event),
                     view);

    /* handle smartinfo in right click menu */
    auto display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()), "display-smartinfo");
    priv->smartinfo_action = g_signal_connect(display_smartinfo,
                                              "notify::state",
                                              G_CALLBACK(toggle_smartinfo),
                                              priv->vbox_call_smartInfo);

    /* check if auto quality is enabled or not; */
    if (const auto& codecModel = priv->call->account()->codecModel()) {
        const auto& videoCodecs = codecModel->videoCodecs();
        if (videoCodecs->rowCount() > 0) {
            /* we only need to check the first codec since by default it is ON for all, and the
             * gnome client sets its ON or OFF for all codecs as well */
            const auto& idx = videoCodecs->index(0,0);
            auto auto_quality_enabled = idx.data(static_cast<int>(CodecModel::Role::AUTO_QUALITY_ENABLED)).toString() == "true";
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_autoquality), auto_quality_enabled);

            // TODO: save the manual quality setting in the client and set the slider to that value here;
            //       the daemon resets the bitrate/quality between each call, and the default may be
            //       different for each codec, so there is no reason to check it here
        }
    }

    /* init chat view */
    auto chat_view = chat_view_new_call(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), priv->call);
    gtk_container_add(GTK_CONTAINER(priv->frame_chat), chat_view);

    /* check if there were any chat notifications and open the chat view if so */
    if (ring_notify_close_chat_notification(priv->call->peerContactMethod()))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_chat), TRUE);

    /* show chat view on any new incoming messages */
    g_signal_connect_swapped(chat_view, "new-messages-displayed", G_CALLBACK(show_chat_view), view);
}

GtkWidget *
current_call_view_new(Call *call, WebKitChatContainer *webkit_chat_container)
{
    auto self = g_object_new(CURRENT_CALL_VIEW_TYPE, NULL);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(webkit_chat_container);
    set_call_info(CURRENT_CALL_VIEW(self), call);

    return GTK_WIDGET(self);
}

Call*
current_call_view_get_call(CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), nullptr);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    return priv->call;
}
