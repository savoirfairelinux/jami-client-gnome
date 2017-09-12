/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
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

#include <clutter-gtk/clutter-gtk.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <account.h>
#include <api/newcallmodel.h>
#include <api/contactmodel.h>
#include <codecmodel.h>
#include <video/previewmanager.h>
#include <globalinstances.h>
#include <smartinfohub.h>

#include "chatview.h"
#include "native/pixbufmanipulator.h"
#include "ringnotify.h"
#include "utils/drawing.h"
#include "utils/files.h"
#include "video/video_widget.h"


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
    GtkWidget *label_bestId;
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
    GtkWidget *togglebutton_muteaudio;
    GtkWidget *togglebutton_mutevideo;
    GtkWidget *togglebutton_hold;
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

    ConversationContainer* conversation_;
    AccountContainer* accountContainer_;

    QMetaObject::Connection state_change_connection;
    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection remote_renderer_connection;

    GSettings *settings;

    // for clutter animations and to know when to fade in/out the overlays
    ClutterTransition *fade_info;
    ClutterTransition *fade_controls;
    gint64 time_last_mouse_motion;
    guint timer_fade;

    gulong insert_controls_id;

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
muteaudio_toggled(GtkToggleButton *togglebutton, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));

    if (gtk_toggle_button_get_active(togglebutton)) {
        auto image_down = gtk_image_new_from_resource ("/cx/ring/RingGnome/unmute_audio");
        gtk_button_set_image(GTK_BUTTON(togglebutton), image_down);
    } else {
        auto image_up = gtk_image_new_from_resource ("/cx/ring/RingGnome/mute_audio");
        gtk_button_set_image(GTK_BUTTON(togglebutton), image_up);
    }
}

static void
mutevideo_toggled(GtkToggleButton *togglebutton, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));

    if (gtk_toggle_button_get_active(togglebutton)) {
        auto image_down = gtk_image_new_from_resource ("/cx/ring/RingGnome/unmute_video");
        gtk_button_set_image(GTK_BUTTON(togglebutton), image_down);
    } else {
        auto image_up = gtk_image_new_from_resource ("/cx/ring/RingGnome/mute_video");
        gtk_button_set_image(GTK_BUTTON(togglebutton), image_up);
    }
}

static void
hold_toggled(GtkToggleButton *togglebutton, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));

    if (gtk_toggle_button_get_active(togglebutton)) {
        auto image_down = gtk_image_new_from_resource ("/cx/ring/RingGnome/play");
        gtk_button_set_image(GTK_BUTTON(togglebutton), image_down);
    } else {
        auto image_up = gtk_image_new_from_resource ("/cx/ring/RingGnome/pause");
        gtk_button_set_image(GTK_BUTTON(togglebutton), image_up);
    }
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

    // TODO
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
    // TODO
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

    // now make sure the quality gets updated
    quality_changed(GTK_SCALE_BUTTON(priv->scalebutton_quality), 0, self);

    return GDK_EVENT_PROPAGATE;
}

static void
button_hangup_clicked(CurrentCallView *view)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    auto callId = priv->conversation_->info.callId;
    priv->accountContainer_->info.callModel->hangUp(callId);
}

static void
togglebutton_hold_clicked(CurrentCallView *view)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    auto callId = priv->conversation_->info.callId;
    priv->accountContainer_->info.callModel->togglePause(callId);
}

static void
togglebutton_muteaudio_clicked(CurrentCallView *view)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    auto callId = priv->conversation_->info.callId;
    auto muteAudioBtn = GTK_TOGGLE_BUTTON(priv->togglebutton_muteaudio);
    priv->accountContainer_->info.callModel->toggleMedia(callId,
        lrc::api::NewCallModel::Media::AUDIO,
        gtk_toggle_button_get_active(muteAudioBtn));
}

static void
togglebutton_mutevideo_clicked(CurrentCallView *view)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    auto callId = priv->conversation_->info.callId;
    auto muteVideoBtn = GTK_TOGGLE_BUTTON(priv->togglebutton_mutevideo);
    priv->accountContainer_->info.callModel->toggleMedia(callId,
        lrc::api::NewCallModel::Media::VIDEO,
        gtk_toggle_button_get_active(muteVideoBtn));
}

static void
insert_controls(CurrentCallView *view)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* only add the controls once */
    g_signal_handler_disconnect(view, priv->insert_controls_id);
    priv->insert_controls_id = 0;

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
    clutter_margin_smartInfo.left = 10;
    clutter_margin_smartInfo.bottom = 10;
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

    /* connect the controllers (new model) */
    g_signal_connect_swapped(priv->button_hangup, "clicked", G_CALLBACK(button_hangup_clicked), view);
    g_signal_connect_swapped(priv->togglebutton_hold, "clicked", G_CALLBACK(togglebutton_hold_clicked), view);
    g_signal_connect_swapped(priv->togglebutton_muteaudio, "clicked", G_CALLBACK(togglebutton_muteaudio_clicked), view);
    g_signal_connect_swapped(priv->togglebutton_mutevideo, "clicked", G_CALLBACK(togglebutton_mutevideo_clicked), view);

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
    }
    if (auto scale = gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality))) {
        g_signal_connect(scale, "button-press-event", G_CALLBACK(quality_button_pressed), view);
        g_signal_connect(scale, "button-release-event", G_CALLBACK(quality_button_released), view);
    }

    // Get if the user wants to show the smartInfo box
    auto display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()), "display-smartinfo");
    if (g_variant_get_boolean(g_action_get_state(G_ACTION(display_smartinfo)))) {
        gtk_widget_show(priv->vbox_call_smartInfo);
    } else {
        gtk_widget_hide(priv->vbox_call_smartInfo);
    }
}

static void
current_call_view_init(CurrentCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".smartinfo-block-style { color: #8ae234; background-color: rgba(1, 1, 1, 0.33); } \
        @keyframes blink { 0% {opacity: 1;} 49% {opacity: 1;} 50% {opacity: 0;} 100% {opacity: 0;} } \
        .record-button { background: rgba(0, 0, 0, 1); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .record-button:checked { animation: blink 1s; animation-iteration-count: infinite; } \
        .call-button { background: rgba(0, 0, 0, 0); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .call-button:hover { background: rgba(200, 200, 200, 0.15); } \
        .call-button:disabled { opacity: 0.2; } \
        .can-be-disabled:checked { background: rgba(219, 58, 55, 1); } \
        .hangup-button-style { background: rgba(219, 58, 55, 1); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .hangup-button-style:hover { background: rgba(219, 39, 25, 1); }",
        -1, nullptr
    );
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    priv->video_widget = video_widget_new();
    gtk_container_add(GTK_CONTAINER(priv->frame_video), priv->video_widget);
    gtk_widget_show_all(priv->frame_video);

    // add the overlay controls only once the view has been allocated a size to prevent size
    // allocation warnings in the log
    priv->insert_controls_id = g_signal_connect(view, "size-allocate", G_CALLBACK(insert_controls), nullptr);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_bestId);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_description);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_value);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_general_information);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, paned_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_hold);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_muteaudio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_mutevideo);
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
update_state(CurrentCallView *view)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    // change state label
    auto callId = priv->conversation_->info.callId;
    auto call = priv->accountContainer_->info.callModel->getCall(callId);

    gchar *status = g_strdup_printf("%s", lrc::api::NewCallModel::humanReadableStatus(call.status).c_str());
    gtk_label_set_text(GTK_LABEL(priv->label_status), status);
    g_free(status);
}

static void
update_details(CurrentCallView *view)
{
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    // TODO
    gtk_label_set_text(GTK_LABEL(priv->label_duration), "00:00");
}


static void
update_name_and_photo(CurrentCallView *view)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    QVariant var_i = GlobalInstances::pixmapManipulator().conversationPhoto(
        priv->conversation_->info,
        priv->accountContainer_->info,
        QSize(60, 60),
        false
    );
    std::shared_ptr<GdkPixbuf> image = var_i.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_peer), image.get());

    auto contact = priv->accountContainer_->info.contactModel->getContact(priv->conversation_->info.participants.front());

    auto name = contact.alias;
    gtk_label_set_text(GTK_LABEL(priv->label_name), name.c_str());

    auto bestId = contact.uri;
    if (name != bestId) {
        gtk_label_set_text(GTK_LABEL(priv->label_bestId), bestId.c_str());
        gtk_widget_show(priv->label_bestId);
    }
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

    // on double click
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
set_call_info(CurrentCallView *view) {
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    // change some things depending on call state
    update_state(view);
    update_details(view);

    // check if we already have a renderer
    video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                   Video::PreviewManager::instance().previewRenderer(),
                                   VIDEO_RENDERER_REMOTE);

    // local renderer
    if (Video::PreviewManager::instance().isPreviewing())
        video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                       Video::PreviewManager::instance().previewRenderer(),
                                       VIDEO_RENDERER_LOCAL);

    // callback for local renderer
    priv->local_renderer_connection = QObject::connect(
        &Video::PreviewManager::instance(),
        &Video::PreviewManager::previewStarted,
        [priv](Video::Renderer *renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_LOCAL);
        }
    );

    priv->smartinfo_refresh_connection = QObject::connect(
        &SmartInfoHub::instance(),
        &SmartInfoHub::changed,
        [view, priv]() { update_smartInfo(view); }
    );

    priv->remote_renderer_connection = QObject::connect(
        &*priv->accountContainer_->info.callModel,
        &lrc::api::NewCallModel::remotePreviewStarted,
        [priv](const std::string& callId, Video::Renderer *renderer) {
            if (priv->conversation_->info.callId == callId) {
                video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                    renderer,
                    VIDEO_RENDERER_REMOTE);
            }
        }
    );

    priv->state_change_connection = QObject::connect(
    &*priv->accountContainer_->info.callModel,
    &lrc::api::NewCallModel::callStatusChanged,
    [view, priv] (const std::string& callId) {
        if (callId == priv->conversation_->info.callId) {
            update_state(view);
            update_name_and_photo(view);
        }
    });


    // catch double click to make full screen
    g_signal_connect(priv->video_widget, "button-press-event",
                     G_CALLBACK(on_button_press_in_video_event),
                     view);

    // handle smartinfo in right click menu
    auto display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()), "display-smartinfo");
    priv->smartinfo_action = g_signal_connect(display_smartinfo,
                                              "notify::state",
                                              G_CALLBACK(toggle_smartinfo),
                                              priv->vbox_call_smartInfo);

    // init chat view
    auto chat_view = chat_view_new(WEBKIT_CHAT_CONTAINER(priv->webkit_chat_container), priv->accountContainer_, priv->conversation_);
    gtk_container_add(GTK_CONTAINER(priv->frame_chat), chat_view);

}

GtkWidget *
current_call_view_new(WebKitChatContainer* view, AccountContainer* accountContainer, ConversationContainer* conversationContainer)
{
    auto self = g_object_new(CURRENT_CALL_VIEW_TYPE, NULL);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    priv->webkit_chat_container = GTK_WIDGET(view);
    priv->conversation_ = conversationContainer;
    priv->accountContainer_ = accountContainer;
    set_call_info(CURRENT_CALL_VIEW(self));

    return GTK_WIDGET(self);
}

lrc::api::conversation::Info
current_call_view_get_conversation(CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), lrc::api::conversation::Info());
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    return priv->conversation_->info;
}
