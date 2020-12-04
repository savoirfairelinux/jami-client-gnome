/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
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

 // Client
#include "chatview.h"
#include "native/pixbufmanipulator.h"
#include "notifier.h"
#include "utils/drawing.h"
#include "utils/files.h"
#include "video/video_widget.h"

// Lrc
#include <api/avmodel.h>
#include <api/pluginmodel.h>
#include <api/newaccountmodel.h>
#include <api/conversationmodel.h>
#include <api/contact.h>
#include <api/contactmodel.h>
#include <api/newcallmodel.h>
#include <api/newcodecmodel.h>
#include <globalinstances.h>
#include <smartinfohub.h>

// Gtk
#include <clutter-gtk/clutter-gtk.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <QSize>
#include <QJsonObject>

#include <set>

#define PLUGIN_ICON_SIZE 25

enum class RowType {
    CONTACT,
    CALL,
    CONFERENCE,
    TITLE
};

namespace { namespace details
{
class CppImpl;
}}

struct _CurrentCallView
{
    GtkBox parent;
};

struct _CurrentCallViewClass
{
    GtkBoxClass parent_class;
};

struct CurrentCallViewPrivate
{
    GtkWidget *hbox_call_info;
    GtkWidget *hbox_call_status;
    GtkWidget *hbox_call_controls;
    GtkWidget *hbox_remote_info;
    GtkWidget *border_remote_info;
    GtkWidget *revealer_remote_recording;
    GtkWidget *vbox_call_smartInfo;
    GtkWidget *vbox_peer_identity;
    GtkWidget *image_peer;
    GtkWidget *label_name;
    GtkWidget *label_bestId;
    GtkWidget *label_status;
    GtkWidget *label_duration;
    GtkWidget *label_smartinfo_description;
    GtkWidget *label_smartinfo_value;
    GtkWidget *label_smartinfo_general_information;
    GtkWidget *label_remoteRecord_information;
    GtkWidget *paned_call;
    GtkWidget *frame_video;
    GtkWidget *video_widget;
    GtkWidget *frame_chat;
    GtkWidget *togglebutton_chat;
    GtkWidget *togglebutton_muteaudio;
    GtkWidget *togglebutton_mutevideo;
    GtkWidget *togglebutton_add_participant;
    GtkWidget *togglebutton_activate_plugin;
    GtkWidget *togglebutton_transfer;
    GtkWidget *siptransfer_popover;
    GtkWidget *siptransfer_filter_entry;
    GtkWidget *list_conversations;
    GtkWidget *add_participant_popover;
    GtkWidget *conversation_filter_entry;
    GtkWidget *list_conversations_invite;
    GtkWidget *activate_plugin_popover;
    GtkWidget *list_media_handlers_available;
    GtkWidget *togglebutton_hold;
    GtkWidget *togglebutton_record;
    GtkWidget *button_hangup;
    GtkWidget *scalebutton_quality;
    GtkWidget *checkbutton_autoquality;
    GtkWidget *chat_view;
    GtkWidget *webkit_chat_container; // The webkit_chat_container is created once, then reused for all chat views

    GSettings *settings;

    details::CppImpl* cpp; ///< Non-UI and C++ only code
};

G_DEFINE_TYPE_WITH_PRIVATE(CurrentCallView, current_call_view, GTK_TYPE_BOX);

#define CURRENT_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CURRENT_CALL_VIEW_TYPE, CurrentCallViewPrivate))

enum {
    VIDEO_DOUBLE_CLICKED,
    LAST_SIGNAL
};

//==============================================================================

namespace { namespace details
{

static constexpr int CONTROLS_FADE_TIMEOUT = 3000000; /* microseconds */
static constexpr int FADE_DURATION = 500; /* miliseconds */
static guint current_call_view_signals[LAST_SIGNAL] = { 0 };

namespace // Helpers
{

static gboolean
map_boolean_to_orientation(GValue* value, GVariant* variant, G_GNUC_UNUSED gpointer user_data)
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

static ClutterTransition*
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


static GtkWidget*
create_transition_recording()
{
    auto* revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(revealer), GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    gtk_widget_set_halign(revealer, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(revealer, GTK_ALIGN_START);
    gtk_revealer_set_transition_duration(GTK_REVEALER(revealer), 1000);
    gtk_widget_set_visible(revealer, TRUE);
    return revealer;
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
gtk_scale_button_get_scale(GtkScaleButton* button)
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

} // namespace

class CppImpl
{
public:
    explicit CppImpl(CurrentCallView& widget, const lrc::api::Lrc& lrc);
    ~CppImpl();

    void init();
    void setup(WebKitChatContainer* chat_widget,
               AccountInfoPointer const & account_info,
               lrc::api::conversation::Info& conversation,
               lrc::api::AVModel& avModel);

    void updateConvList();
    void updatePluginList();
    void add_transfer_contact(const std::string& uri);
    void add_title(const QString& title);
    void add_present_contact(const QString& uri, const QString& custom_data, RowType custom_type, const QString& accountId);
    void add_conference(const VectorString& uris, const QString& custom_data, const QString& accountId);
    void add_media_handler(lrc::api::plugin::PluginHandlerDetails mediaHandlerDetails);

    void insertControls();
    void checkControlsFading();
    void update_view();
    void update_participants_hovers(const QString& callId);
    void checkRemoteFading();
    void set_remote_record_animation(std::string callId, QSet<QString> peerName, bool state);

    CurrentCallView* self = nullptr; // The GTK widget itself
    CurrentCallViewPrivate* widgets = nullptr;

    lrc::api::conversation::Info* conversation = nullptr;
    AccountInfoPointer const *accountInfo = nullptr;
    lrc::api::AVModel* avModel_;

    QMetaObject::Connection state_change_connection;
    QMetaObject::Connection layout_change_connection;
    QMetaObject::Connection update_vcard_connection;
    QMetaObject::Connection renderer_connection;
    QMetaObject::Connection smartinfo_refresh_connection;
    QMetaObject::Connection remoteinfo_connection;

    // for clutter animations and to know when to fade in/out the overlays
    ClutterTransition* fade_info = nullptr;
    ClutterTransition* fade_controls = nullptr;
    gint64 time_last_mouse_motion = 0;
    guint timer_fade = 0;
    guint timer_record_fade = 0;

    /* flag used to keep track of the video quality scale pressed state;
     * we do not want to update the codec bitrate until the user releases the
     * scale button */
    gboolean quality_scale_pressed = FALSE;
    gulong insert_controls_id = 0;
    guint smartinfo_action = 0;

    const lrc::api::Lrc& lrc_;

    std::vector<std::string> titles_;
    std::set<std::string> hiddenTitles_;

    std::string currentCall_ {};

private:
    CppImpl() = delete;
    CppImpl(const CppImpl&) = delete;
    CppImpl& operator=(const CppImpl&) = delete;

    void setCallInfo();
    void updateDetails();
    void updateState();
    void updateNameAndPhoto();
    void updateSmartInfo();
};

inline namespace gtk_callbacks
{

static void
set_call_quality(CurrentCallView* view, bool auto_quality_on, double desired_quality)
{
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto videoCodecs = (*priv->cpp->accountInfo)->codecModel->getVideoCodecs();
    for (const auto& codec : videoCodecs) {
        if (auto_quality_on) {
            (*priv->cpp->accountInfo)->codecModel->autoQuality(codec.id, true);
        } else {
            (*priv->cpp->accountInfo)->codecModel->autoQuality(codec.id, false);
            double min_bitrate = 0., max_bitrate = 0., min_quality = 0., max_quality = 0.;
            try {
                min_bitrate = codec.min_bitrate.toInt();
                max_bitrate = codec.max_bitrate.toInt();
                min_quality = codec.min_quality.toInt();
                max_quality = codec.max_quality.toInt();
            } catch (...) {
                g_error("Cannot convert a codec value to an int, abort");
                break;
            }

            double bitrate = min_bitrate + (max_bitrate - min_bitrate)*(desired_quality/100.0);
            if (bitrate < 0) bitrate = 0;
            (*priv->cpp->accountInfo)->codecModel->bitrate(codec.id, bitrate);

            // note: a lower value means higher quality
            double quality = min_quality - (min_quality - max_quality)*(desired_quality/100.0);
            if (quality < 0) quality = 0;
            (*priv->cpp->accountInfo)->codecModel->quality(codec.id, quality);
        }
    }
}

static void
set_record_animation(CurrentCallViewPrivate* priv)
{
    auto callToRender = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToRender = priv->cpp->conversation->confId;
    bool currentStatus = (*priv->cpp->accountInfo)->callModel->isRecording(callToRender);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_record),
        currentStatus);
}

static void
on_togglebutton_chat_toggled(GtkToggleButton* widget, CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    if (gtk_toggle_button_get_active(widget)) {
        gtk_widget_show_all(priv->frame_chat);
        gtk_widget_grab_focus(priv->frame_chat);
    } else {
        gtk_widget_hide(priv->frame_chat);
    }
}

static gboolean
on_timer_fade_timeout(CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), G_SOURCE_REMOVE);
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    priv->cpp->checkControlsFading();
    return G_SOURCE_CONTINUE;
}

static gboolean
on_timer_record_fade_timeout(CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), G_SOURCE_REMOVE);
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    priv->cpp->checkRemoteFading();
    return G_SOURCE_CONTINUE;
}

static void
on_size_allocate(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->cpp->insertControls();
}

static void
on_button_hangup_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    auto callToStop = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToStop = priv->cpp->conversation->confId;
    (*priv->cpp->accountInfo)->callModel->hangUp(callToStop);
}

static void
on_togglebutton_hold_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToHold = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToHold = priv->cpp->conversation->confId;
    (*priv->cpp->accountInfo)->callModel->togglePause(callToHold);
}

static void
on_togglebutton_record_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToRecord = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToRecord = priv->cpp->conversation->confId;
    (*priv->cpp->accountInfo)->callModel->toggleAudioRecord(callToRecord);

    set_record_animation(priv);
}

static void
on_togglebutton_muteaudio_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToMute = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToMute = priv->cpp->conversation->confId;
    //auto muteAudioBtn = GTK_TOGGLE_BUTTON(priv->togglebutton_muteaudio);
    (*priv->cpp->accountInfo)->callModel->toggleMedia(callToMute,
        lrc::api::NewCallModel::Media::AUDIO);

    auto togglebutton = GTK_TOGGLE_BUTTON(priv->togglebutton_muteaudio);
    auto image = gtk_image_new_from_resource ("/net/jami/JamiGnome/mute_audio");
    if (gtk_toggle_button_get_active(togglebutton))
        image = gtk_image_new_from_resource ("/net/jami/JamiGnome/unmute_audio");
    gtk_button_set_image(GTK_BUTTON(togglebutton), image);
}

static void
on_togglebutton_mutevideo_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToMute = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToMute = priv->cpp->conversation->confId;
    //auto muteVideoBtn = GTK_TOGGLE_BUTTON(priv->togglebutton_mutevideo);
    (*priv->cpp->accountInfo)->callModel->toggleMedia(callToMute,
        lrc::api::NewCallModel::Media::VIDEO);

    auto togglebutton = GTK_TOGGLE_BUTTON(priv->togglebutton_mutevideo);
    auto image = gtk_image_new_from_resource ("/net/jami/JamiGnome/mute_video");
    if (gtk_toggle_button_get_active(togglebutton))
        image = gtk_image_new_from_resource ("/net/jami/JamiGnome/unmute_video");
    gtk_button_set_image(GTK_BUTTON(togglebutton), image);
}

static gboolean
on_mouse_moved(CurrentCallView* view, GdkEvent *event, GtkWidget* widget)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);
    g_return_val_if_fail(IS_VIDEO_WIDGET(widget), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->cpp->time_last_mouse_motion = g_get_monotonic_time();

    // HACK: Cf https://gitlab.gnome.org/GNOME/clutter-gtk/-/issues/11
    // Because the participants_hovers can't have correct mouse events,
    // we need to pass the event to the video_widget and check if hovers
    // need to be notified
    if (event)
        video_widget_on_event(VIDEO_WIDGET(priv->video_widget), event);

    // since the mouse moved, make sure the controls are shown
    if (clutter_timeline_get_direction(CLUTTER_TIMELINE(priv->cpp->fade_info)) == CLUTTER_TIMELINE_FORWARD) {
        clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->cpp->fade_info), CLUTTER_TIMELINE_BACKWARD);
        clutter_timeline_set_direction(CLUTTER_TIMELINE(priv->cpp->fade_controls), CLUTTER_TIMELINE_BACKWARD);
        if (!clutter_timeline_is_playing(CLUTTER_TIMELINE(priv->cpp->fade_info))) {
            clutter_timeline_rewind(CLUTTER_TIMELINE(priv->cpp->fade_info));
            clutter_timeline_rewind(CLUTTER_TIMELINE(priv->cpp->fade_controls));
            clutter_timeline_start(CLUTTER_TIMELINE(priv->cpp->fade_info));
            clutter_timeline_start(CLUTTER_TIMELINE(priv->cpp->fade_controls));
        }
    }

    return FALSE; // propagate event
}

static void
on_autoquality_toggled(GtkToggleButton* button, CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    gboolean auto_quality_on = gtk_toggle_button_get_active(button);

    auto scale = gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality));
    auto plus_button = gtk_scale_button_get_plus_button(GTK_SCALE_BUTTON(priv->scalebutton_quality));
    auto minus_button = gtk_scale_button_get_minus_button(GTK_SCALE_BUTTON(priv->scalebutton_quality));

    gtk_widget_set_sensitive(GTK_WIDGET(scale), !auto_quality_on);
    gtk_widget_set_sensitive(plus_button, !auto_quality_on);
    gtk_widget_set_sensitive(minus_button, !auto_quality_on);

    double desired_quality = gtk_scale_button_get_value(GTK_SCALE_BUTTON(priv->scalebutton_quality));

    set_call_quality(view, auto_quality_on, desired_quality);
}

static void
on_quality_changed(G_GNUC_UNUSED GtkScaleButton *button, G_GNUC_UNUSED gdouble value,
                   CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* no need to update quality if auto quality is enabled */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkbutton_autoquality))) return;

    /* update only if the scale button is released (reduces the number of updates) */
    if (priv->cpp->quality_scale_pressed) return;

    set_call_quality(view, false, gtk_scale_button_get_value(button));
}

static gboolean
on_quality_button_pressed(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event,
                          CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->cpp->quality_scale_pressed = TRUE;

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_quality_button_released(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event,
                           CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->cpp->quality_scale_pressed = FALSE;

    // make sure the quality gets updated
    on_quality_changed(GTK_SCALE_BUTTON(priv->scalebutton_quality), 0, view);

    return GDK_EVENT_PROPAGATE;
}

static gboolean
on_video_widget_focus(GtkWidget* widget, GtkDirectionType direction, CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    // if this widget already has focus, we want the focus to move to the next widget, otherwise we
    // will get stuck in a focus loop on the buttons
    if (gtk_widget_has_focus(widget))
        return FALSE;

    // otherwise we want the focus to go to and change between the call control buttons
    if (gtk_widget_child_focus(GTK_WIDGET(priv->hbox_call_controls), direction)) {
        // selected a child, make sure call controls are shown
        on_mouse_moved(view, nullptr, widget);
        return TRUE;
    }

    // did not select the next child, propagate the event
    return FALSE;
}

static gboolean
on_button_press_in_video_event(GtkWidget* widget, GdkEventButton *event, CurrentCallView* view)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(widget), FALSE);
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);

    // on double click
    if (event->type == GDK_2BUTTON_PRESS) {
        g_signal_emit(G_OBJECT(view), current_call_view_signals[VIDEO_DOUBLE_CLICKED], 0);
    }

    return GDK_EVENT_PROPAGATE;
}

static void
on_toggle_smartinfo(GSimpleAction* action, G_GNUC_UNUSED GVariant* state, GtkWidget* vbox_call_smartInfo)
{
    if (g_variant_get_boolean(g_action_get_state(G_ACTION(action)))) {
        gtk_widget_show(vbox_call_smartInfo);
    } else {
        gtk_widget_hide(vbox_call_smartInfo);
    }
}

static void
transfer_to_peer(CurrentCallViewPrivate* priv, const std::string& peerUri)
{
    if (peerUri == priv->cpp->conversation->participants.front().toStdString()) {
        g_warning("avoid to transfer to the same call, abort.");
#if GTK_CHECK_VERSION(3,22,0)
        gtk_popover_popdown(GTK_POPOVER(priv->siptransfer_popover));
#else
        gtk_widget_hide(GTK_WIDGET(priv->siptransfer_popover));
#endif
        return;
    }
    try {
        // If a call is already present with a peer, try an attended transfer.
        auto callInfo = (*priv->cpp->accountInfo)->callModel->getCallFromURI(peerUri.c_str(), true);
        (*priv->cpp->accountInfo)->callModel->transferToCall(
            priv->cpp->conversation->callId, callInfo.id);
    } catch (std::out_of_range&) {
        // No current call found with this URI, perform a blind transfer
        (*priv->cpp->accountInfo)->callModel->transfer(
            priv->cpp->conversation->callId, peerUri.c_str());
    }
#if GTK_CHECK_VERSION(3,22,0)
    gtk_popover_popdown(GTK_POPOVER(priv->siptransfer_popover));
#else
    gtk_widget_hide(GTK_WIDGET(priv->siptransfer_popover));
#endif
}

static void
on_siptransfer_filter_activated(CurrentCallView* self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    transfer_to_peer(priv, gtk_entry_get_text(GTK_ENTRY(priv->siptransfer_filter_entry)));
}

static GtkLabel*
get_address_label(GtkListBoxRow* row)
{
    auto* row_children = gtk_container_get_children(GTK_CONTAINER(row));
    auto* box_infos = g_list_first(row_children)->data;
    auto* children = gtk_container_get_children(GTK_CONTAINER(box_infos));
    return GTK_LABEL(g_list_last(children)->data);
}

static GtkLabel*
get_mediahandler_label(GtkListBoxRow* row)
{
    auto* row_children = gtk_container_get_children(GTK_CONTAINER(row));
    auto* box_infos = g_list_first(row_children)->data;
    auto* children = gtk_container_get_children(GTK_CONTAINER(box_infos));
    auto* button = g_list_last(children);
    return GTK_LABEL(g_list_previous(button)->data);
}

static GtkToggleButton*
get_mediahandler_btn(GtkListBoxRow* row)
{
    auto* row_children = gtk_container_get_children(GTK_CONTAINER(row));
    auto* box_infos = g_list_first(row_children)->data;
    auto* children = gtk_container_get_children(GTK_CONTAINER(box_infos));
    auto* button = g_list_last(children);
    return GTK_TOGGLE_BUTTON(button->data);
}

static GtkImage*
get_image(GtkListBoxRow* row)
{
    auto* row_children = gtk_container_get_children(GTK_CONTAINER(row));
    auto* box_infos = g_list_first(row_children)->data;
    auto* children = gtk_container_get_children(GTK_CONTAINER(box_infos));
    return GTK_IMAGE(g_list_first(children)->data);
}

static void
transfer_to_conversation(GtkListBox*, GtkListBoxRow* row, CurrentCallView* self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    auto* sip_address = get_address_label(row);
    transfer_to_peer(priv, gtk_label_get_text(GTK_LABEL(sip_address)));
}

static void
on_search_participant(GtkSearchEntry* search_entry, CurrentCallView* self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    std::string search_text = gtk_entry_get_text(GTK_ENTRY(search_entry));
    std::transform(search_text.begin(), search_text.end(), search_text.begin(), ::tolower);

    auto row = 0, lastTitleRow = -1;
    auto hideTitle = true;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(
            GTK_LIST_BOX(priv->list_conversations_invite), row))) {
        auto* addr_label = get_address_label(GTK_LIST_BOX_ROW(children));
        std::string content = gtk_label_get_text(addr_label);
        std::transform(content.begin(), content.end(), content.begin(), ::tolower);
        if (content.find(search_text) == std::string::npos) {
            bool hide = true;
            for (auto title: priv->cpp->titles_) {
                std::transform(title.begin(), title.end(), title.begin(), ::tolower);
                if (title == content) {
                    hide = false;
                    // Hide last title if needed
                    if (lastTitleRow != -1 && hideTitle) {
                        auto* lastTitle = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations_invite), lastTitleRow));
                        gtk_widget_hide(lastTitle);
                    }
                    lastTitleRow = row;
                    hideTitle = true;
                }
            }
            if (hide) gtk_widget_hide(children);
        } else {
            if (lastTitleRow != -1 && hideTitle) {
                auto* lastTitle = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations_invite), lastTitleRow));
                gtk_widget_show(lastTitle);
            }
            hideTitle = false;
            gtk_widget_show(children);
        }
        row++;
    }

    // Hide last title if needed
    if (lastTitleRow != -1 && hideTitle) {
        auto* lastTitle = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations_invite), lastTitleRow));
        gtk_widget_hide(lastTitle);
    }
}

static void
activate_media_handler(GtkToggleButton* switchBtn, GParamSpec*, CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto rowIdx = 0;
    while (auto* children = gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_media_handlers_available), rowIdx)) {
        auto* label = get_mediahandler_label(GTK_LIST_BOX_ROW(children));
        auto* btn = get_mediahandler_btn(GTK_LIST_BOX_ROW(children));
        if (switchBtn == btn) {
            auto toggled = gtk_toggle_button_get_active(btn);
            QString mediaHandlerID = QString::fromStdString((gchar*)g_object_get_data(G_OBJECT(label), "mediaHandlerID"));
            auto callToToggle = priv->cpp->conversation->callId;
            if (!priv->cpp->conversation->confId.isEmpty())
                callToToggle = priv->cpp->conversation->confId;

            priv->cpp->lrc_.getPluginModel().toggleCallMediaHandler(mediaHandlerID, callToToggle, toggled);
            auto mediaHandlerStatus = priv->cpp->lrc_.getPluginModel().getCallMediaHandlerStatus(callToToggle);
        }
        ++rowIdx;
    }
}

static void
invite_to_conversation(GtkListBox*, GtkListBoxRow* row, CurrentCallView* self)
{
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    auto* label = get_address_label(GTK_LIST_BOX_ROW(row));
    std::string content = gtk_label_get_text(label);
    for (auto title: priv->cpp->titles_) {
        if (content != title) continue;
        bool isHiddenTitle = priv->cpp->hiddenTitles_.find(content) != priv->cpp->hiddenTitles_.end();
        auto* image = get_image(row);
        if (!isHiddenTitle) {
            priv->cpp->hiddenTitles_.insert(content);
            gtk_image_set_from_icon_name(image, "pan-up-symbolic", GTK_ICON_SIZE_MENU);
        } else {
            priv->cpp->hiddenTitles_.erase(content);
            gtk_image_set_from_icon_name(image, "pan-down-symbolic", GTK_ICON_SIZE_MENU);
        }
        auto changeState = false;
        auto rowIdx = 0;
        while (auto* children = gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations_invite), rowIdx)) {
            rowIdx++;
            if (children == row) {
                changeState = true;
                continue;
            }
            if (!changeState) continue;
            auto* addr_label = get_address_label(GTK_LIST_BOX_ROW(children));
            std::string content2 = gtk_label_get_text(addr_label);
            for (auto title: priv->cpp->titles_) {
                if (content2 == title) return; // Other title, stop here.
            }
            if (!isHiddenTitle)
                gtk_widget_hide(GTK_WIDGET(children));
            else {
                gtk_widget_show(GTK_WIDGET(children));
                // refilter if needed
                std::string currentFilter = gtk_entry_get_text(GTK_ENTRY(priv->conversation_filter_entry));
                if (!currentFilter.empty())
                    on_search_participant(GTK_SEARCH_ENTRY(priv->conversation_filter_entry), self);
            }
        }
        return;
    }

    auto callToRender = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.isEmpty())
        callToRender = priv->cpp->conversation->confId;

    auto rowIdx = 0;
    while (auto* children = gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations_invite), rowIdx)) {
        if (children == row) {
            auto* custom_type = g_object_get_data(G_OBJECT(label), "custom_type");
            auto custom_data = QString::fromStdString((gchar*)g_object_get_data(G_OBJECT(label), "custom_data"));
            if (GPOINTER_TO_INT(custom_type) == (int)RowType::CONTACT) {
                try {
                    const auto& call = (*priv->cpp->accountInfo)->callModel->getCall(callToRender);
                    (*priv->cpp->accountInfo)->callModel->callAndAddParticipant(custom_data, callToRender, call.isAudioOnly);
                } catch (...) {
                    g_warning("Can't add participant to inexistent call");
                }
            } else if (GPOINTER_TO_INT(custom_type)  == (int)RowType::CALL
                    || GPOINTER_TO_INT(custom_type)  == (int)RowType::CONFERENCE) {
                (*priv->cpp->accountInfo)->callModel->joinCalls(custom_data, callToRender);
            }
            break;
        }
        ++rowIdx;
    }


#if GTK_CHECK_VERSION(3,22,0)
    gtk_popover_popdown(GTK_POPOVER(priv->add_participant_popover));
#else
    gtk_widget_hide(GTK_WIDGET(priv->add_participant_popover));
#endif
}

static void
filter_transfer_list(CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    std::string currentFilter = gtk_entry_get_text(GTK_ENTRY(priv->siptransfer_filter_entry));

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations), row))) {
        auto* sip_address = get_address_label(GTK_LIST_BOX_ROW(children));
        if (row == 0) {
            // Update searching item
            if (currentFilter.empty() || currentFilter == priv->cpp->conversation->participants.front().toStdString()) {
                // Hide temporary item if filter is empty or same number
                gtk_widget_hide(children);
            } else {
                // Else, show the temporary item (and select it)
                gtk_label_set_text(GTK_LABEL(sip_address), currentFilter.c_str());
                gtk_widget_show_all(children);
                gtk_list_box_select_row(GTK_LIST_BOX(priv->list_conversations), GTK_LIST_BOX_ROW(children));
            }
        } else {
            // It's a contact
            std::string item_address = gtk_label_get_text(GTK_LABEL(sip_address));

            if (item_address == priv->cpp->conversation->participants.front().toStdString())
                // if item is the current conversation, hide it
                gtk_widget_hide(children);
            else if (currentFilter.empty())
                // filter is empty, show all items
                gtk_widget_show_all(children);
            else if (item_address.find(currentFilter) == std::string::npos || item_address == currentFilter)
                // avoid duplicates and unwanted numbers
                gtk_widget_hide(children);
            else
                // Item is filtered
                gtk_widget_show_all(children);
        }
        ++row;
    }
}

static void
media_handler_list(CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_media_handlers_available), row))) {
        auto* mediaHandlerName = get_mediahandler_label(GTK_LIST_BOX_ROW(children));

        std::string mediaHandlerNameStr = gtk_label_get_text(GTK_LABEL(mediaHandlerName));
        gtk_widget_show_all(children);

        ++row;
    }
}

static void
on_button_activate_plugin_clicked(CurrentCallView *self)
{
    // Show and init list
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    priv->cpp->updatePluginList();
    gtk_popover_set_relative_to(GTK_POPOVER(priv->activate_plugin_popover), GTK_WIDGET(priv->togglebutton_activate_plugin));
#if GTK_CHECK_VERSION(3,22,0)
    gtk_popover_popdown(GTK_POPOVER(priv->activate_plugin_popover));
#else
    gtk_widget_show_all(GTK_WIDGET(priv->activate_plugin_popover));
#endif
    gtk_widget_show_all(priv->activate_plugin_popover);
    media_handler_list(self);
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(priv->list_media_handlers_available), GTK_SELECTION_NONE);
}

static void
on_button_add_participant_clicked(CurrentCallView *self)
{
    // Show and init list
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    priv->cpp->updateConvList();
    gtk_popover_set_relative_to(GTK_POPOVER(priv->add_participant_popover), GTK_WIDGET(priv->togglebutton_add_participant));
#if GTK_CHECK_VERSION(3,22,0)
    gtk_popover_popdown(GTK_POPOVER(priv->add_participant_popover));
#else
    gtk_widget_show_all(GTK_WIDGET(priv->add_participant_popover));
#endif
    gtk_widget_show_all(priv->add_participant_popover);
    filter_transfer_list(self);
}

static void
on_button_transfer_clicked(CurrentCallView *self)
{
    // Show and init list
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    gtk_popover_set_relative_to(GTK_POPOVER(priv->siptransfer_popover), GTK_WIDGET(priv->togglebutton_transfer));
#if GTK_CHECK_VERSION(3,22,0)
    gtk_popover_popdown(GTK_POPOVER(priv->siptransfer_popover));
#else
    gtk_widget_show_all(GTK_WIDGET(priv->siptransfer_popover));
#endif
    gtk_widget_show_all(priv->siptransfer_popover);
    filter_transfer_list(self);
}

static void
on_siptransfer_text_changed(GtkSearchEntry*, CurrentCallView* self)
{
    filter_transfer_list(self);
}

} // namespace gtk_callbacks

CppImpl::CppImpl(CurrentCallView& widget, const lrc::api::Lrc& lrc)
    : self {&widget}
    , lrc_ {lrc}
    , widgets {CURRENT_CALL_VIEW_GET_PRIVATE(&widget)}
{}

CppImpl::~CppImpl()
{
    QObject::disconnect(state_change_connection);
    QObject::disconnect(layout_change_connection);
    QObject::disconnect(update_vcard_connection);
    QObject::disconnect(renderer_connection);
    QObject::disconnect(smartinfo_refresh_connection);
    QObject::disconnect(remoteinfo_connection);
    g_clear_object(&widgets->settings);

    if (timer_fade) g_source_remove(timer_fade);
    if (timer_record_fade) g_source_remove(timer_record_fade);

    auto* display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()),
                                                        "display-smartinfo");
    g_signal_handler_disconnect(display_smartinfo, smartinfo_action);
}

void
CppImpl::init()
{
    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".search-entry-style { border: 0; border-radius: 0; } \
        .smartinfo-block-style { color: #8ae234; background-color: rgba(1, 1, 1, 0.33); } \
        .remote-recording-block-style { color:rgba(255,255,255,0.7); background-color:rgba(1, 1, 1, 0.3); padding-left: 30px; padding-right: 30px; padding-top: 3px; padding-bottom: 3px; } \
        @keyframes blink { 0% {opacity: 1;} 49% {opacity: 1;} 50% {opacity: 0;} 100% {opacity: 0;} } \
        .record-button { background: rgba(0, 0, 0, 1); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .record-button:checked { animation: blink 1s; animation-iteration-count: infinite; } \
        .call-button { background: rgba(0, 0, 0, 0.35); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .call-button:hover { background: rgba(0, 0, 0, 0.2); } \
        .call-button:disabled { opacity: 0.2; } \
        .can-be-disabled:checked { background: rgba(219, 58, 55, 1); } \
        .hangup-button-style { background: rgba(219, 58, 55, 1); border-radius: 50%; border: 0; transition: all 0.3s ease; } \
        .hangup-button-style:hover { background: rgba(219, 39, 25, 1); }",
        -1, nullptr
    );
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    widgets->video_widget = video_widget_new();
    gtk_container_add(GTK_CONTAINER(widgets->frame_video), widgets->video_widget);
    gtk_widget_show_all(widgets->frame_video);

    // add the overlay controls only once the view has been allocated a size to prevent size
    // allocation warnings in the log
    insert_controls_id = g_signal_connect(self, "size-allocate", G_CALLBACK(on_size_allocate), nullptr);
}

void
CppImpl::setup(WebKitChatContainer* chat_widget,
               AccountInfoPointer const & account_info,
               lrc::api::conversation::Info& conv_info,
               lrc::api::AVModel& avModel)
{
    widgets->webkit_chat_container = GTK_WIDGET(chat_widget);
    conversation = &conv_info;
    accountInfo = &account_info;
    avModel_ = &avModel;
    setCallInfo();

    gtk_widget_hide(widgets->togglebutton_transfer);

    updateConvList();
    updatePluginList();

    g_signal_connect(widgets->conversation_filter_entry, "search-changed", G_CALLBACK(on_search_participant), self);

    if ((*accountInfo)->profileInfo.type == lrc::api::profile::Type::SIP) {
        // Remove previous list
        while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(widgets->list_conversations), 10)))
            gtk_container_remove(GTK_CONTAINER(widgets->list_conversations), children);
        // Fill with SIP contacts
        add_transfer_contact("");  // Temporary item
        for (const auto& c : (*accountInfo)->conversationModel->getFilteredConversations(lrc::api::profile::Type::SIP).get())
            add_transfer_contact(c.get().participants.front().toStdString());
        gtk_widget_show_all(widgets->list_conversations);
        gtk_widget_show(widgets->togglebutton_transfer);
        gtk_widget_show(widgets->togglebutton_hold);
    } else {
        gtk_widget_hide(widgets->togglebutton_hold);
    }

    set_record_animation(widgets);
}

void
CppImpl::add_media_handler(lrc::api::plugin::PluginHandlerDetails mediaHandlerDetails)
{
    QString bestName = _("No name!");
    auto* mediaHandlerImage = gtk_image_new_from_icon_name("application-x-addon-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);

    auto mediaHandlerStatus = lrc_.getPluginModel().getCallMediaHandlerStatus(conversation->callId);
    bool isActive = false;
    if (!mediaHandlerDetails.name.isEmpty()) {
        bestName = mediaHandlerDetails.name;
        if (!mediaHandlerStatus.isEmpty()) {
            for (const auto& toggledMediaHandler : mediaHandlerStatus)
                if (toggledMediaHandler == mediaHandlerDetails.id) {
                    isActive = true;
                    break;
                }
        }
    }

    std::string mediaHandlerID = (mediaHandlerDetails.id).toStdString();

    if (!mediaHandlerDetails.iconPath.isEmpty()) {
        GdkPixbuf* mediaHandlerIcon = gdk_pixbuf_new_from_file_at_size((mediaHandlerDetails.iconPath).toStdString().c_str(), PLUGIN_ICON_SIZE, PLUGIN_ICON_SIZE, NULL);
        mediaHandlerImage = gtk_image_new_from_pixbuf(mediaHandlerIcon);
    }

    gchar* text = nullptr;
    text = g_markup_printf_escaped(
        "<span font=\"10\">%s</span>",
        qUtf8Printable(bestName)
    );

    auto* box_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(box_item));
    gtk_style_context_add_class(context_box, "boxitem");
    gtk_widget_set_name(box_item, mediaHandlerID.c_str());

    auto* activate_mediahandler_button = gtk_check_button_new();
    gtk_widget_set_can_focus(activate_mediahandler_button, false);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(activate_mediahandler_button), isActive);
    g_signal_connect(activate_mediahandler_button, "notify::active", G_CALLBACK(activate_media_handler), self);

    auto* info = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(info), text);
    g_object_set(G_OBJECT(info), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set_data(G_OBJECT(info), "mediaHandlerID", (void*)g_strdup(qUtf8Printable(mediaHandlerDetails.id)));

    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(mediaHandlerImage));
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(info));
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(activate_mediahandler_button));

    gtk_list_box_insert(GTK_LIST_BOX(widgets->list_media_handlers_available), GTK_WIDGET(box_item), -1);
}

void
CppImpl::updatePluginList()
{
    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(widgets->list_media_handlers_available), row))) {
        gtk_container_remove(GTK_CONTAINER(widgets->list_media_handlers_available), children);
    }

    auto callMediaHandlers = lrc_.getPluginModel().getCallMediaHandlers();

    for(const auto& callMediaHandler : callMediaHandlers)
    {
        lrc::api::plugin::PluginHandlerDetails mediaHandlerDetails = lrc_.getPluginModel().getCallMediaHandlerDetails(callMediaHandler);
        add_media_handler(mediaHandlerDetails);
    }
}

void
CppImpl::updateConvList()
{
    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(widgets->list_conversations_invite), row))) {
        gtk_container_remove(GTK_CONTAINER(widgets->list_conversations_invite), children);
    }

    auto callToRender = conversation->callId;
    if (!conversation->confId.isEmpty())
        callToRender = conversation->confId;

    VectorString callsId;
    VectorString uris;
    bool first = true;
    for (const auto& c : lrc_.getConferences()) {
        // Get subcalls
        auto cid = lrc_.getConferenceSubcalls(c);
        callsId.append(cid);
        // Get participants
        QString uri, accountId;
        VectorString curis;
        for (const auto& callId: cid) {
            for (const auto &account_id : lrc_.getAccountModel().getAccountList()) {
                try {
                    auto &accountInfo = lrc_.getAccountModel().getAccountInfo(account_id);
                    if (accountInfo.callModel->hasCall(callId)) {
                        const auto& call = accountInfo.callModel->getCall(callId);
                        uri = QString(call.peerUri).remove("ring:");
                        uris.push_back(uri);
                        curis.push_back(uri);
                        accountId = account_id;
                        break;
                    }
                } catch (...) {}
            }
        }

        if (c == callToRender) {
            continue;
        }

        if (first) {
            add_title(_("Current conference (all accounts)"));
            first = false;
        }
        if (!uri.isEmpty()) {
            add_conference(curis, c, accountId);
        }
    }

    first = true;
    for (const auto& c : lrc_.getCalls()) {
        QString uri, accountId;
        for (const auto &account_id : lrc_.getAccountModel().getAccountList()) {
            try {
                auto &accountInfo = lrc_.getAccountModel().getAccountInfo(account_id);
                if (accountInfo.callModel->hasCall(c)) {
                    const auto& call = accountInfo.callModel->getCall(c);
                    if (call.status != lrc::api::call::Status::PAUSED
                        && call.status != lrc::api::call::Status::IN_PROGRESS) {
                        // Ignore non active calls
                        callsId.push_back(call.id);
                        continue;
                    }
                    uri = QString(call.peerUri).remove("ring:");
                    accountId = account_id;
                    uris.push_back(uri);
                    break;
                }
            } catch (...) {}
        }
        auto isPresent = callsId.contains(c);
        if (c == callToRender || isPresent) {
            continue;
        }

        if (first) {
            add_title(_("Current calls (all accounts)"));
            first = false;
        }
        if (!uri.isEmpty()) {
            add_present_contact(uri, c, RowType::CALL, accountId);
        }
    }

    first = true;
    for (const auto& c : (*accountInfo)->conversationModel->getFilteredConversations((*accountInfo)->profileInfo.type).get()) {
        try {
            auto participant = c.get().participants.front();
            auto contactInfo = (*accountInfo)->contactModel->getContact(participant);
            auto isPresent = std::find(uris.cbegin(), uris.cend(), participant) != uris.cend();
            if (!isPresent
                && ((*accountInfo)->profileInfo.type == lrc::api::profile::Type::SIP || contactInfo.isPresent)) {
                if (first) {
                    add_title(_("Online contacts"));
                    first = false;
                }
                add_present_contact(participant, participant, RowType::CONTACT, (*accountInfo)->id);
            }
        } catch (...) {}
    }
}

void
CppImpl::add_title(const QString& title) {
    auto* box_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    auto* avatar = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_MENU);
    auto* info = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(info), qUtf8Printable(title));
    gtk_widget_set_halign(info, GTK_ALIGN_CENTER);
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(avatar));
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(info));
    g_object_set(G_OBJECT(info), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_list_box_insert(GTK_LIST_BOX(widgets->list_conversations_invite), GTK_WIDGET(box_item), -1);

    titles_.emplace_back(title.toStdString());
}

void
CppImpl::add_present_contact(const QString& uri, const QString& custom_data, RowType custom_type, const QString& accountId)
{
    QString bestName = uri, bestUri = uri;
    auto default_avatar = Interfaces::PixbufManipulator().generateAvatar("", "");
    auto default_scaled = Interfaces::PixbufManipulator().scaleAndFrame(default_avatar.get(), QSize(48, 48), true, IconStatus::PRESENT);
    auto photo = default_scaled;

    try {
        auto &accInfo = lrc_.getAccountModel().getAccountInfo(accountId);
        auto contactInfo = accInfo.contactModel->getContact(uri);
        auto photostr = contactInfo.profileInfo.avatar;
        auto alias = contactInfo.profileInfo.alias;

        if (!alias.isEmpty()) {
            bestName = alias;
        }

        if (!contactInfo.registeredName.isEmpty()) {
            bestUri = contactInfo.registeredName;
        }

        if (!photostr.isEmpty()) {
            QByteArray byteArray = photostr.toUtf8();
            QVariant avatar = Interfaces::PixbufManipulator().personPhoto(byteArray);
            auto pixbuf_photo = Interfaces::PixbufManipulator().scaleAndFrame(avatar.value<std::shared_ptr<GdkPixbuf>>().get(), QSize(48, 48), true, IconStatus::PRESENT);
            if (avatar.isValid()) {
                photo = pixbuf_photo;
            }
        } else {
            auto name = alias.isEmpty()? contactInfo.registeredName : alias;
            auto firstLetter = (name == contactInfo.profileInfo.uri || name.isEmpty()) ?
            "" : QString(name.at(0));  // NOTE best way to be compatible with UTF-8
            auto fullUri = contactInfo.profileInfo.uri;
            if (accInfo.profileInfo.type != lrc::api::profile::Type::SIP)
                fullUri = "ring:" + fullUri;
            else
                fullUri = "sip:" + fullUri;
            photo = Interfaces::PixbufManipulator().generateAvatar(firstLetter.toStdString(), fullUri.toStdString());
            photo = Interfaces::PixbufManipulator().scaleAndFrame(photo.get(), QSize(48, 48), true, IconStatus::PRESENT);
        }
    } catch (const std::out_of_range&) {
        // ContactModel::getContact() exception
    }

    gchar* text = nullptr;
    if (uri != bestName) {
        bestName.remove('\r');
        bestName.remove('\n');
        text = g_markup_printf_escaped(
            "<span font_weight=\"bold\">%s</span>\n<span size=\"smaller\" color=\"#666\">%s</span>",
            qUtf8Printable(bestName),
            qUtf8Printable(bestUri)
        );
    } else {
        text = g_markup_printf_escaped(
            "<span font=\"10\">%s</span>",
            qUtf8Printable(bestUri)
        );
    }

    auto* box_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    auto* avatar = gtk_image_new_from_pixbuf(photo.get());
    auto* info = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(info), text);
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(avatar));
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(info));
    g_object_set(G_OBJECT(info), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    g_object_set_data(G_OBJECT(info), "custom_type", GINT_TO_POINTER(custom_type));
    g_object_set_data(G_OBJECT(info), "custom_data", (void*)g_strdup(qUtf8Printable(custom_data)));

    gtk_list_box_insert(GTK_LIST_BOX(widgets->list_conversations_invite), GTK_WIDGET(box_item), -1);
}

void
CppImpl::add_conference(const VectorString& uris, const QString& custom_data, const QString& accountId)
{
    GError *error = nullptr;
    auto default_avatar = std::shared_ptr<GdkPixbuf>(
        gdk_pixbuf_new_from_resource_at_scale("/net/jami/JamiGnome/contacts_list", 50, 50, true, &error),
        g_object_unref
    );
    if (default_avatar == nullptr) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
        return;
    }
    auto default_scaled = Interfaces::PixbufManipulator().scaleAndFrame(default_avatar.get(), QSize(50, 50));
    auto photo = default_scaled;

    std::string label;
    auto idx = 0;

    for (const auto& uri: uris) {
        try {
            auto bestName = uri;
            auto &accInfo = lrc_.getAccountModel().getAccountInfo(accountId);
            auto contactInfo = accInfo.contactModel->getContact(uri);
            auto alias = contactInfo.profileInfo.alias;

            if (!alias.isEmpty()) {
                bestName = alias;
            } else if (!contactInfo.registeredName.isEmpty()) {
                bestName = contactInfo.registeredName;
            }
            bestName.remove('\r');
            bestName.remove('\n');
            label += bestName.toStdString();
            if (idx != static_cast<int>(uris.size()) - 1)
                label += ", ";
            idx ++;
        } catch (const std::out_of_range&) {
            // ContactModel::getContact() exception
        }
    }


    gchar* text = g_markup_printf_escaped(
        "<span font=\"10\">%s</span>",
        label.c_str()
    );

    auto* box_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    auto* avatar = gtk_image_new_from_pixbuf(photo.get());
    auto* info = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(info), text);
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(avatar));
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(info));
    g_object_set(G_OBJECT(info), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    g_object_set_data(G_OBJECT(info), "custom_type", GINT_TO_POINTER(RowType::CONFERENCE));
    g_object_set_data(G_OBJECT(info), "custom_data", (void*)g_strdup(qUtf8Printable(custom_data)));

    gtk_list_box_insert(GTK_LIST_BOX(widgets->list_conversations_invite), GTK_WIDGET(box_item), -1);
}

void
CppImpl::add_transfer_contact(const std::string& uri)
{
    auto* box_item = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    auto pixbufmanipulator = Interfaces::PixbufManipulator();
    auto image_buf = pixbufmanipulator.generateAvatar("", uri.empty() ? uri : "sip" + uri);
    auto scaled = pixbufmanipulator.scaleAndFrame(image_buf.get(), QSize(48, 48));
    auto* avatar = gtk_image_new_from_pixbuf(scaled.get());
    auto* address = gtk_label_new(uri.c_str());
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(avatar));
    gtk_container_add(GTK_CONTAINER(box_item), GTK_WIDGET(address));
    gtk_list_box_insert(GTK_LIST_BOX(widgets->list_conversations), GTK_WIDGET(box_item), -1);
}

void
CppImpl::setCallInfo()
{
    // change some things depending on call state
    updateState();
    updateDetails();
    updateNameAndPhoto();

    g_signal_connect(widgets->video_widget, "button-press-event",
                     G_CALLBACK(video_widget_on_button_press_in_screen_event), nullptr);

    // check if we already have a renderer
    auto callToRender = conversation->callId;
    if (!conversation->confId.isEmpty())
        callToRender = conversation->confId;

    (*accountInfo)->callModel->setCurrentCall(callToRender);
    currentCall_ = callToRender.toStdString();
    try {
        // local renderer
        const lrc::api::video::Renderer* previewRenderer =
             &avModel_->getRenderer(
             lrc::api::video::PREVIEW_RENDERER_ID);
        if (previewRenderer->isRendering()) {
            video_widget_add_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                avModel_, previewRenderer, VIDEO_RENDERER_LOCAL);
        }
        try {
            auto call = (*accountInfo)->callModel->getCall(callToRender);
            video_widget_set_preview_visible(VIDEO_WIDGET(widgets->video_widget),
                        call.status != lrc::api::call::Status::PAUSED
                        && call.type != lrc::api::call::Type::CONFERENCE
                        && call.participantsInfos.empty());
        } catch (...) {
            g_warning("Can't change preview visibility for non existent call");
        }

        const lrc::api::video::Renderer* vRenderer =
            &avModel_->getRenderer(
            callToRender);

        video_widget_add_new_renderer(VIDEO_WIDGET(widgets->video_widget),
            avModel_, vRenderer, VIDEO_RENDERER_REMOTE);
    } catch (...) {
        // The renderer doesn't exist for now. Ignore
    }

    widgets->revealer_remote_recording = create_transition_recording();
    gtk_container_add(GTK_CONTAINER(widgets->revealer_remote_recording), widgets->border_remote_info);
    gtk_container_add(GTK_CONTAINER(widgets->hbox_remote_info), widgets->revealer_remote_recording);
    gtk_revealer_set_reveal_child(GTK_REVEALER(widgets->revealer_remote_recording), FALSE);

    // Set remote recording indicator
    auto call = (*accountInfo)->callModel->getCall(callToRender);
    if (not call.peerRec.isEmpty())
        set_remote_record_animation(currentCall_, call.peerRec, true);

    update_participants_hovers(callToRender);

    // callback for local renderer
    renderer_connection = QObject::connect(
        &*avModel_,
        &lrc::api::AVModel::rendererStarted,
        [=](const QString& id) {
            if (id == lrc::api::video::PREVIEW_RENDERER_ID) {
                try {
                    // local renderer
                    const lrc::api::video::Renderer* previewRenderer =
                        &avModel_->getRenderer(lrc::api::video::PREVIEW_RENDERER_ID);
                    if (previewRenderer->isRendering()) {
                        video_widget_add_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                            avModel_, previewRenderer, VIDEO_RENDERER_LOCAL);
                        auto call = (*accountInfo)->callModel->getCall(callToRender);
                        video_widget_set_preview_visible(VIDEO_WIDGET(widgets->video_widget),
                            call.status != lrc::api::call::Status::PAUSED
                            && call.type != lrc::api::call::Type::CONFERENCE
                            && call.participantsInfos.empty());
                    }
                } catch (...) {
                    g_warning("Preview renderer is not accessible! This should not happen");
                }
            } else if (id == callToRender) {
                try {
                    const lrc::api::video::Renderer* vRenderer =
                        &avModel_->getRenderer(
                        callToRender);
                    video_widget_add_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                        avModel_, vRenderer, VIDEO_RENDERER_REMOTE);
                } catch (...) {
                    g_warning("Remote renderer is not accessible! This should not happen");
                }
            }
        });

    smartinfo_refresh_connection = QObject::connect(
        &SmartInfoHub::instance(),
        &SmartInfoHub::changed,
        [this] { updateSmartInfo(); }
    );

    state_change_connection = QObject::connect(
        &*(*accountInfo)->callModel,
        &lrc::api::NewCallModel::callStatusChanged,
        [this] (const QString& callId) {
            auto callToRender = conversation->callId;
            if (!conversation->confId.isEmpty())
                callToRender = conversation->confId;
            if (callId == conversation->callId) {
                try {
                    auto call = (*accountInfo)->callModel->getCall(callToRender);
                    video_widget_set_preview_visible(VIDEO_WIDGET(widgets->video_widget),
                        call.status != lrc::api::call::Status::PAUSED
                        && call.type != lrc::api::call::Type::CONFERENCE
                        && call.participantsInfos.empty());
                } catch (...) {
                    g_warning("Can't set preview visible for inexistent call");
                }
                updateNameAndPhoto();
                updateState();
            }
        });

    layout_change_connection = QObject::connect(
        &*(*accountInfo)->callModel,
        &lrc::api::NewCallModel::onParticipantsChanged,
        [this] (const QString& callId) {
            update_participants_hovers(callId);
        });

    update_vcard_connection = QObject::connect(
        &*(*accountInfo)->contactModel,
        &lrc::api::ContactModel::contactAdded,
        [this] (const QString& uri) {
            if (uri == conversation->participants.front()) {
                updateNameAndPhoto();
            }
        });


    // catch double click to make full screen
    g_signal_connect(widgets->video_widget, "button-press-event",
                     G_CALLBACK(on_button_press_in_video_event), self);

    // handle smartinfo in right click menu
    auto display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()),
                                                        "display-smartinfo");
    smartinfo_action = g_signal_connect(display_smartinfo,
                                        "notify::state",
                                        G_CALLBACK(on_toggle_smartinfo),
                                        widgets->vbox_call_smartInfo);

    remoteinfo_connection = QObject::connect(
        &*(*accountInfo)->callModel,
        &lrc::api::NewCallModel::remoteRecordingChanged,
        [this] (const QString& callId, const QSet<QString>& peerRec, bool state) {
            g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
            set_remote_record_animation(callId.toStdString(), peerRec, state);
        });

    // init chat view
    widgets->chat_view = chat_view_new(WEBKIT_CHAT_CONTAINER(widgets->webkit_chat_container),
                                       *accountInfo, *conversation, *avModel_, lrc_.getPluginModel());
    gtk_container_add(GTK_CONTAINER(widgets->frame_chat), widgets->chat_view);

    chat_view_set_header_visible(CHAT_VIEW(widgets->chat_view), FALSE);
    chat_view_set_record_visible(CHAT_VIEW(widgets->chat_view), FALSE);
    chat_view_set_plugin_visible(CHAT_VIEW(widgets->chat_view), FALSE);
}

void
CppImpl::insertControls()
{
    /* only add the controls once */
    g_signal_handler_disconnect(self, insert_controls_id);
    insert_controls_id = 0;

    auto stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(widgets->video_widget));
    auto actor_info = gtk_clutter_actor_new_with_contents(widgets->hbox_call_info);
    auto actor_controls = gtk_clutter_actor_new_with_contents(widgets->hbox_call_controls);
    auto actor_smartInfo = gtk_clutter_actor_new_with_contents(widgets->vbox_call_smartInfo);

    auto audioOnly = false;
    auto callId = conversation->callId;
    try {
        auto call = (*accountInfo)->callModel->getCall(callId);
        audioOnly = call.isAudioOnly;
    } catch (std::out_of_range& e) {
    }

    clutter_actor_add_child(stage, actor_info);
    clutter_actor_set_x_align(actor_info, CLUTTER_ACTOR_ALIGN_FILL);
    if (!audioOnly) {
        clutter_actor_set_y_align(actor_info, CLUTTER_ACTOR_ALIGN_START);
    } else {
        clutter_actor_set_y_align(actor_info, CLUTTER_ACTOR_ALIGN_CENTER);
        gtk_orientable_set_orientation(GTK_ORIENTABLE(widgets->hbox_call_info), GTK_ORIENTATION_VERTICAL);
        gtk_widget_set_halign(widgets->vbox_peer_identity, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(widgets->hbox_call_status, GTK_ALIGN_CENTER);
        gtk_widget_set_halign(widgets->label_bestId, GTK_ALIGN_CENTER);
    }

    clutter_actor_add_child(stage, actor_controls);
    clutter_actor_set_x_align(actor_controls, CLUTTER_ACTOR_ALIGN_CENTER);
    clutter_actor_set_y_align(actor_controls, CLUTTER_ACTOR_ALIGN_END);
    clutter_actor_set_z_position(actor_controls, 4); // Controls should be in front of the hovers

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
    time_last_mouse_motion = g_get_monotonic_time();
    fade_info = create_fade_out_transition();
    fade_controls = create_fade_out_transition();
    clutter_actor_add_transition(actor_info, "fade_info", fade_info);
    clutter_actor_add_transition(actor_controls, "fade_controls", fade_controls);
    clutter_timeline_set_direction(CLUTTER_TIMELINE(fade_info), CLUTTER_TIMELINE_BACKWARD);
    clutter_timeline_set_direction(CLUTTER_TIMELINE(fade_controls), CLUTTER_TIMELINE_BACKWARD);
    clutter_timeline_stop(CLUTTER_TIMELINE(fade_info));
    clutter_timeline_stop(CLUTTER_TIMELINE(fade_controls));

    /* have a timer check every 1 second if the controls should fade out */
    timer_fade = g_timeout_add(1000, (GSourceFunc)on_timer_fade_timeout, self);

    /* connect the controllers (new model) */
    g_signal_connect_swapped(widgets->button_hangup, "clicked", G_CALLBACK(on_button_hangup_clicked), self);
    g_signal_connect_swapped(widgets->togglebutton_add_participant, "clicked", G_CALLBACK(on_button_add_participant_clicked), self);
    g_signal_connect_swapped(widgets->togglebutton_activate_plugin, "clicked", G_CALLBACK(on_button_activate_plugin_clicked), self);
    g_signal_connect_swapped(widgets->togglebutton_transfer, "clicked", G_CALLBACK(on_button_transfer_clicked), self);
    g_signal_connect_swapped(widgets->siptransfer_filter_entry, "activate", G_CALLBACK(on_siptransfer_filter_activated), self);
    g_signal_connect(widgets->siptransfer_filter_entry, "search-changed", G_CALLBACK(on_siptransfer_text_changed), self);
    g_signal_connect(widgets->list_conversations, "row-activated", G_CALLBACK(transfer_to_conversation), self);
    g_signal_connect(widgets->list_conversations_invite, "row-activated", G_CALLBACK(invite_to_conversation), self);
    g_signal_connect_swapped(widgets->togglebutton_hold, "clicked", G_CALLBACK(on_togglebutton_hold_clicked), self);
    g_signal_connect_swapped(widgets->togglebutton_muteaudio, "clicked", G_CALLBACK(on_togglebutton_muteaudio_clicked), self);
    g_signal_connect_swapped(widgets->togglebutton_record, "clicked", G_CALLBACK(on_togglebutton_record_clicked), self);
    g_signal_connect_swapped(widgets->togglebutton_mutevideo, "clicked", G_CALLBACK(on_togglebutton_mutevideo_clicked), self);

    /* connect to the mouse motion event to reset the last moved time */
    g_signal_connect_swapped(widgets->video_widget, "motion-notify-event", G_CALLBACK(on_mouse_moved), self);
    g_signal_connect_swapped(widgets->video_widget, "button-press-event", G_CALLBACK(on_mouse_moved), self);
    g_signal_connect_swapped(widgets->video_widget, "button-release-event", G_CALLBACK(on_mouse_moved), self);

    /* manually handle the focus of the video widget to be able to focus on the call controls */
    g_signal_connect(widgets->video_widget, "focus", G_CALLBACK(on_video_widget_focus), self);


    /* toggle whether or not the chat is displayed */
    g_signal_connect(widgets->togglebutton_chat, "toggled", G_CALLBACK(on_togglebutton_chat_toggled), self);

    /* bind the chat orientation to the gsetting */
    widgets->settings = g_settings_new_full(get_settings_schema(), nullptr, nullptr);
    g_settings_bind_with_mapping(widgets->settings, "chat-pane-horizontal",
                                 widgets->paned_call, "orientation",
                                 G_SETTINGS_BIND_GET,
                                 map_boolean_to_orientation,
                                 nullptr, nullptr, nullptr);

    g_signal_connect(widgets->scalebutton_quality, "value-changed", G_CALLBACK(on_quality_changed), self);

    /* customize the quality button scale */
    if (auto scale_box = gtk_scale_button_get_box(GTK_SCALE_BUTTON(widgets->scalebutton_quality))) {
        widgets->checkbutton_autoquality = gtk_check_button_new_with_label(C_("Enable automatic video quality",
                                                                              "Auto"));
        gtk_widget_show(widgets->checkbutton_autoquality);
        gtk_box_pack_start(GTK_BOX(scale_box), widgets->checkbutton_autoquality, FALSE, TRUE, 0);
        g_signal_connect(widgets->checkbutton_autoquality, "toggled", G_CALLBACK(on_autoquality_toggled), self);
    }
    if (auto scale = gtk_scale_button_get_scale(GTK_SCALE_BUTTON(widgets->scalebutton_quality))) {
        g_signal_connect(scale, "button-press-event", G_CALLBACK(on_quality_button_pressed), self);
        g_signal_connect(scale, "button-release-event", G_CALLBACK(on_quality_button_released), self);
    }

    g_signal_connect(widgets->video_widget, "drag-data-received",
                     G_CALLBACK(video_widget_on_drag_data_received), nullptr);

    auto videoCodecs = (*accountInfo)->codecModel->getVideoCodecs();
    if (!videoCodecs.empty()) {
        bool autoQualityEnabled = videoCodecs.front().auto_quality_enabled;
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->checkbutton_autoquality),
                                     autoQualityEnabled);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->checkbutton_autoquality),
                                     false);
    }

    // Get if the user wants to show the smartInfo box
    auto display_smartinfo = g_action_map_lookup_action(G_ACTION_MAP(g_application_get_default()),
                                                        "display-smartinfo");
    if (g_variant_get_boolean(g_action_get_state(G_ACTION(display_smartinfo)))) {
        gtk_widget_show(widgets->vbox_call_smartInfo);
    } else {
        gtk_widget_hide(widgets->vbox_call_smartInfo);
    }
}

void
CppImpl::updateDetails()
{
    if (!conversation) {
        g_warning("Could not update currentcallview details (null conversation)");
        return;
    }

    auto callRendered = conversation->callId;

    if (!conversation->confId.isEmpty())
        callRendered = conversation->confId;

    gtk_label_set_text(GTK_LABEL(widgets->label_duration),
        qUtf8Printable((*accountInfo)->callModel->getFormattedCallDuration(callRendered)));

    update_view();
}

void
CppImpl::update_participants_hovers(const QString& callId)
{
    if (callId == conversation->callId or callId == conversation->confId) {
        // Remove previouses hovers, because they are now invalid
        video_widget_remove_hovers(
            VIDEO_WIDGET(widgets->video_widget)
        );
        // Update callInfo for the video_widget
        video_widget_set_call_info(VIDEO_WIDGET(widgets->video_widget), *accountInfo, callId);
        try {
            auto call = (*accountInfo)->callModel->getCall(callId);
            // Create participant hovers
            for (const auto& participant: call.participantsInfos) {
                QJsonObject data;
                data["x"] = participant["x"].toInt();
                data["y"] = participant["y"].toInt();
                data["w"] = participant["w"].toInt();
                data["h"] = participant["h"].toInt();
                data["active"] = participant["active"] == "true";
                auto bestName = participant["uri"];
                data["isLocal"] = false;
                if (bestName == (*accountInfo)->profileInfo.uri) {
                    bestName = _("me");
                    data["isLocal"] = true;
                } else {
                    try {
                        auto &contact = (*accountInfo)->contactModel->getContact(participant["uri"]);
                        bestName = contact.profileInfo.alias;
                        if (bestName.isEmpty())
                            bestName = contact.registeredName;
                        if (bestName.isEmpty())
                            bestName = contact.profileInfo.uri;
                        bestName.remove('\r');
                    } catch (...) {}
                }
                data["bestName"] = bestName;
                data["uri"] = participant["uri"];
                video_widget_add_participant_hover(
                    VIDEO_WIDGET(widgets->video_widget),
                    data
                );
            }
            // Update preview visibility, show preview if the call is not hold and
            // not a conference host or participant
            video_widget_set_preview_visible(VIDEO_WIDGET(widgets->video_widget),
                        call.status != lrc::api::call::Status::PAUSED
                        && call.type != lrc::api::call::Type::CONFERENCE
                        && call.participantsInfos.empty());
        } catch (...) {
            g_warning("Can't set preview visible for inexistent call");
        }
    }
}

void
CppImpl::updateState()
{
    if (!conversation) {
        g_warning("Could not update currentcallview state (null conversation)");
        return;
    }

    auto callId = conversation->callId;

    try {
        auto call = (*accountInfo)->callModel->getCall(callId);

        auto pauseBtn = GTK_TOGGLE_BUTTON(widgets->togglebutton_hold);
        auto image = gtk_image_new_from_resource ("/net/jami/JamiGnome/pause");
        if (call.status == lrc::api::call::Status::PAUSED)
            image = gtk_image_new_from_resource ("/net/jami/JamiGnome/play");
        gtk_button_set_image(GTK_BUTTON(pauseBtn), image);

        auto audioButton = GTK_TOGGLE_BUTTON(widgets->togglebutton_muteaudio);
        gtk_widget_set_sensitive(GTK_WIDGET(widgets->togglebutton_muteaudio),
                                 (conversation->confId.isEmpty()));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->togglebutton_muteaudio), call.audioMuted);
        auto imageMuteAudio = gtk_image_new_from_resource ("/net/jami/JamiGnome/mute_audio");
        if (call.audioMuted)
            imageMuteAudio = gtk_image_new_from_resource ("/net/jami/JamiGnome/unmute_audio");
        gtk_button_set_image(GTK_BUTTON(audioButton), imageMuteAudio);

        if (!call.isAudioOnly) {
            auto videoButton = GTK_TOGGLE_BUTTON(widgets->togglebutton_mutevideo);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->togglebutton_mutevideo), call.videoMuted);
            auto imageMuteVideo = gtk_image_new_from_resource ("/net/jami/JamiGnome/mute_video");
            if (call.videoMuted)
                imageMuteVideo = gtk_image_new_from_resource ("/net/jami/JamiGnome/unmute_video");
            gtk_button_set_image(GTK_BUTTON(videoButton), imageMuteVideo);
            gtk_widget_set_sensitive(GTK_WIDGET(widgets->togglebutton_mutevideo),
                                 (conversation->confId.isEmpty()));

            gtk_widget_show(widgets->togglebutton_mutevideo);
            gtk_widget_show(widgets->scalebutton_quality);
        } else {
            gtk_widget_hide(widgets->scalebutton_quality);
            gtk_widget_hide(widgets->togglebutton_mutevideo);
        }

        gchar *status = g_strdup_printf("%s", qUtf8Printable(lrc::api::call::to_string(call.status)));
        gtk_label_set_text(GTK_LABEL(widgets->label_status), status);
        g_free(status);
    } catch (std::out_of_range& e) {
        g_warning("Can't update state for callId=%s", qUtf8Printable(callId));
    }
}

void
CppImpl::updateNameAndPhoto()
{
    QSize photoSize = QSize(60, 60);
    auto callId = conversation->callId;
    try {
        auto call = (*accountInfo)->callModel->getCall(callId);
        if (call.isAudioOnly)
            photoSize = QSize(150, 150);
    } catch (std::out_of_range& e) {
    }

    QVariant var_i = GlobalInstances::pixmapManipulator().conversationPhoto(
        *conversation,
        **(accountInfo),
        photoSize,
        false
    );
    std::shared_ptr<GdkPixbuf> image = var_i.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image_peer), image.get());

    try {
        auto contactInfo = (*accountInfo)->contactModel->getContact(conversation->participants.front());
        auto alias = contactInfo.profileInfo.alias;
        auto bestName = contactInfo.registeredName;
        if (bestName.isEmpty())
            bestName = contactInfo.profileInfo.uri;
        if (bestName == alias)
            alias = "";
        bestName.remove('\r');
        alias.remove('\r');

        if (!alias.isEmpty()) {
            gtk_label_set_text(GTK_LABEL(widgets->label_name), qUtf8Printable(alias));
            gtk_widget_show(widgets->label_name);
        }

        gtk_label_set_text(GTK_LABEL(widgets->label_bestId), qUtf8Printable(bestName));
        gtk_widget_show(widgets->label_bestId);
    } catch (const std::out_of_range&) {
        // ContactModel::getContact() exception
    }
}

void
CppImpl::updateSmartInfo()
{
    if (!SmartInfoHub::instance().isConference()) {
        gchar* general_information = g_strdup_printf(
            "Call ID: %s", SmartInfoHub::instance().callID().toStdString().c_str());
        gtk_label_set_text(GTK_LABEL(widgets->label_smartinfo_general_information), general_information);
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
        gtk_label_set_text(GTK_LABEL(widgets->label_smartinfo_description),description);
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
        gtk_label_set_text(GTK_LABEL(widgets->label_smartinfo_value),value);
        g_free(value);
    } else {
        gchar* general_information = g_strdup_printf(
            "Conference ID: %s", SmartInfoHub::instance().callID().toStdString().c_str());
        gtk_label_set_text(GTK_LABEL(widgets->label_smartinfo_general_information), general_information);
        g_free(general_information);

        gchar* description = g_strdup_printf("You\n"
                                             "Framerate:\n"
                                             "Video codec:\n"
                                             "Audio codec:\n"
                                             "Resolution:");
        gtk_label_set_text(GTK_LABEL(widgets->label_smartinfo_description),description);
        g_free(description);

        gchar* value = g_strdup_printf("\n%f\n%s\n%s\n%dx%d",
                                       (double)SmartInfoHub::instance().localFps(),
                                       SmartInfoHub::instance().localVideoCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().localAudioCodec().toStdString().c_str(),
                                       SmartInfoHub::instance().localWidth(),
                                       SmartInfoHub::instance().localHeight());
        gtk_label_set_text(GTK_LABEL(widgets->label_smartinfo_value),value);
        g_free(value);
    }
}

void
CppImpl::checkControlsFading()
{
    auto current_time = g_get_monotonic_time();
    if (current_time - time_last_mouse_motion >= CONTROLS_FADE_TIMEOUT) {
        // timeout has passed, hide the controls
        if (clutter_timeline_get_direction(CLUTTER_TIMELINE(fade_info)) == CLUTTER_TIMELINE_BACKWARD) {
            clutter_timeline_set_direction(CLUTTER_TIMELINE(fade_info), CLUTTER_TIMELINE_FORWARD);
            clutter_timeline_set_direction(CLUTTER_TIMELINE(fade_controls), CLUTTER_TIMELINE_FORWARD);
            if (!clutter_timeline_is_playing(CLUTTER_TIMELINE(fade_info))) {
                clutter_timeline_rewind(CLUTTER_TIMELINE(fade_info));
                clutter_timeline_rewind(CLUTTER_TIMELINE(fade_controls));
                clutter_timeline_start(CLUTTER_TIMELINE(fade_info));
                clutter_timeline_start(CLUTTER_TIMELINE(fade_controls));
            }
        }
    }

    updateDetails();
}

void
CppImpl::update_view()
{
    // only shows the plugin button if plugins are enabled AND there is any plugin loaded
    if (lrc_.getPluginModel().getPluginsEnabled() && lrc_.getPluginModel().getCallMediaHandlers().size() > 0)
    {
        gtk_widget_show_all(widgets->togglebutton_activate_plugin);
    }
    else
    {
        gtk_widget_hide(widgets->togglebutton_activate_plugin);
    }
}

void
CppImpl::checkRemoteFading()
{
    g_debug("Remote recording timeout ended");
    gtk_revealer_set_reveal_child(GTK_REVEALER(widgets->revealer_remote_recording), FALSE);
    if (timer_record_fade) g_source_remove(timer_record_fade);
}

void
CppImpl::set_remote_record_animation(std::string callId, QSet<QString> peerName, bool state)
{
    if (callId == currentCall_) {
        // Show control
        if (clutter_timeline_get_direction(CLUTTER_TIMELINE(fade_info)) == CLUTTER_TIMELINE_FORWARD) {
            clutter_timeline_set_direction(CLUTTER_TIMELINE(fade_info), CLUTTER_TIMELINE_BACKWARD);
            clutter_timeline_set_direction(CLUTTER_TIMELINE(fade_controls), CLUTTER_TIMELINE_BACKWARD);
            if (!clutter_timeline_is_playing(CLUTTER_TIMELINE(fade_info))) {
                clutter_timeline_rewind(CLUTTER_TIMELINE(fade_info));
                clutter_timeline_rewind(CLUTTER_TIMELINE(fade_controls));
                clutter_timeline_start(CLUTTER_TIMELINE(fade_info));
                clutter_timeline_start(CLUTTER_TIMELINE(fade_controls));
            }
        }
        time_last_mouse_motion = g_get_monotonic_time();

        std::string label;
        auto idx = 0;

        for (const auto& uri: peerName) {
            auto bestName = (*accountInfo)->contactModel->bestNameForContact(uri);
            if (bestName.isEmpty())
                continue;
            label += bestName.toStdString();
            if (idx != static_cast<int>(peerName.size()) - 1)
                    label += ", ";
            idx ++;
        }

        if (not peerName.isEmpty()) {
            g_debug("Remote recording activated");
            gchar* value = g_strdup_printf("%s %s recording", label.c_str(), peerName.size() == 1 ? "is" : "are");
            gtk_label_set_text(GTK_LABEL(widgets->label_remoteRecord_information),value);
            g_free(value);
            if (timer_record_fade) g_source_remove(timer_record_fade);
            gtk_revealer_set_reveal_child(GTK_REVEALER(widgets->revealer_remote_recording), TRUE);
        } else if (!state and peerName.isEmpty()) {
            g_debug("Remote recording disactivated");
            gchar* value = g_strdup_printf("Peer stopped recording");
            gtk_label_set_text(GTK_LABEL(widgets->label_remoteRecord_information),value);
            g_free(value);
            timer_record_fade = g_timeout_add(2000, (GSourceFunc)on_timer_record_fade_timeout, self);
        }
    }
}

}} // namespace <anonymous>::details

//==============================================================================

lrc::api::conversation::Info&
current_call_view_get_conversation(CurrentCallView *self)
{
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    return *priv->cpp->conversation;
}

GtkWidget *
current_call_view_get_chat_view(CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), nullptr);
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    return priv->chat_view;
}

std::string
current_call_view_get_rendered_call(CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), "");
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    if (!priv->cpp) return {};

    return priv->cpp->currentCall_;
}

void
current_call_view_show_chat(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_chat), TRUE);
}

//==============================================================================

static void
current_call_view_init(CurrentCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
current_call_view_dispose(GObject *object)
{
    auto* view = CURRENT_CALL_VIEW(object);
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    // navbar was hidden during setCallInfo, we need to make it visible again before view destruction
    auto children = gtk_container_get_children(GTK_CONTAINER(priv->frame_chat));
    auto chat_view = children->data;
    chat_view_set_header_visible(CHAT_VIEW(chat_view), TRUE);

    delete priv->cpp;
    priv->cpp = nullptr;

    G_OBJECT_CLASS(current_call_view_parent_class)->dispose(object);
}

static void
current_call_view_class_init(CurrentCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = current_call_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/currentcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, hbox_call_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, hbox_call_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, hbox_call_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, vbox_call_smartInfo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, hbox_remote_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, border_remote_info);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, vbox_peer_identity);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, image_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_bestId);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_description);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_value);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_smartinfo_general_information);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_remoteRecord_information);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, paned_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_add_participant);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_activate_plugin);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_transfer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_hold);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_muteaudio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_record);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_mutevideo);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, button_hangup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, scalebutton_quality);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, siptransfer_popover);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, siptransfer_filter_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, list_conversations);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, add_participant_popover);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, conversation_filter_entry);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, list_conversations_invite);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, activate_plugin_popover);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, list_media_handlers_available);

    details::current_call_view_signals[VIDEO_DOUBLE_CLICKED] = g_signal_new (
        "video-double-clicked",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

GtkWidget *
current_call_view_new(WebKitChatContainer* chat_widget,
                      AccountInfoPointer const & accountInfo,
                      lrc::api::conversation::Info& conversation,
                      lrc::api::AVModel& avModel,
                      const lrc::api::Lrc& lrc)
{
    auto* self = g_object_new(CURRENT_CALL_VIEW_TYPE, NULL);
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    // CppImpl ctor
    CurrentCallView* view = CURRENT_CALL_VIEW(self);
    priv->cpp = new details::CppImpl(*view, lrc);
    priv->cpp->init();
    priv->cpp->setup(chat_widget, accountInfo, conversation, avModel);

    return GTK_WIDGET(self);
}

void
current_call_view_handup_focus(GtkWidget *current_call_view)
{
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(current_call_view);
    g_return_if_fail(priv);
    gtk_widget_set_can_focus (priv->button_hangup, true);
    gtk_widget_grab_focus(priv->button_hangup);
}
