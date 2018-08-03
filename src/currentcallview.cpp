/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
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

// Gtk
#include <clutter-gtk/clutter-gtk.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// Lrc
#include <api/conversationmodel.h>
#include <api/contact.h>
#include <api/contactmodel.h>
#include <api/newcallmodel.h>
#include <api/newcodecmodel.h>
#include <globalinstances.h>
#include <smartinfohub.h>
#include <video/previewmanager.h>

// Client
#include "chatview.h"
#include "native/pixbufmanipulator.h"
#include "ringnotify.h"
#include "utils/drawing.h"
#include "utils/files.h"
#include "video/video_widget.h"

#include <iostream>

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
    GtkWidget *togglebutton_transfer;
    GtkWidget* siptransfer_popover;
    GtkWidget* siptransfer_filter_entry;
    GtkWidget* list_conversations;
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
    explicit CppImpl(CurrentCallView& widget);
    ~CppImpl();

    void init();
    void setup(WebKitChatContainer* chat_widget,
               AccountInfoPointer const & account_info,
               lrc::api::conversation::Info* conversation);
    void add_transfer_contact(const std::string& uri);

    void insertControls();
    void checkControlsFading();

    CurrentCallView* self = nullptr; // The GTK widget itself
    CurrentCallViewPrivate* widgets = nullptr;

    lrc::api::conversation::Info* conversation = nullptr;
    AccountInfoPointer const *accountInfo = nullptr;

    QMetaObject::Connection state_change_connection;
    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection remote_renderer_connection;
    QMetaObject::Connection new_message_connection;
    QMetaObject::Connection smartinfo_refresh_connection;

    // for clutter animations and to know when to fade in/out the overlays
    ClutterTransition* fade_info = nullptr;
    ClutterTransition* fade_controls = nullptr;
    gint64 time_last_mouse_motion = 0;
    guint timer_fade = 0;

    /* flag used to keep track of the video quality scale pressed state;
     * we do not want to update the codec bitrate until the user releases the
     * scale button */
    gboolean quality_scale_pressed = FALSE;
    gulong insert_controls_id = 0;
    guint smartinfo_action = 0;

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
                min_bitrate = std::stoi(codec.min_bitrate);
                max_bitrate = std::stoi(codec.max_bitrate);
                min_quality = std::stoi(codec.min_quality);
                max_quality = std::stoi(codec.max_quality);
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
on_new_chat_interactions(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_chat), TRUE);
}

static void
set_record_animation(CurrentCallViewPrivate* priv)
{
    auto callToRender = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.empty())
        callToRender = priv->cpp->conversation->confId;
    bool nextStatus = (*priv->cpp->accountInfo)->callModel->isRecording(callToRender);
    bool currentStatus = (*priv->cpp->accountInfo)->callModel->isRecording(callToRender);
    if (nextStatus != currentStatus) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_record),
            (*priv->cpp->accountInfo)->callModel->isRecording(callToRender));
    }
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
    (*priv->cpp->accountInfo)->callModel->hangUp(priv->cpp->conversation->callId);
}

static void
on_togglebutton_hold_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToHold = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.empty())
        callToHold = priv->cpp->conversation->confId;
    (*priv->cpp->accountInfo)->callModel->togglePause(callToHold);
}

static void
on_togglebutton_record_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToRecord = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.empty())
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
    if (!priv->cpp->conversation->confId.empty())
        callToMute = priv->cpp->conversation->confId;
    //auto muteAudioBtn = GTK_TOGGLE_BUTTON(priv->togglebutton_muteaudio);
    (*priv->cpp->accountInfo)->callModel->toggleMedia(callToMute,
        lrc::api::NewCallModel::Media::AUDIO);

    auto togglebutton = GTK_TOGGLE_BUTTON(priv->togglebutton_muteaudio);
    auto image = gtk_image_new_from_resource ("/cx/ring/RingGnome/mute_audio");
    if (gtk_toggle_button_get_active(togglebutton))
        image = gtk_image_new_from_resource ("/cx/ring/RingGnome/unmute_audio");
    gtk_button_set_image(GTK_BUTTON(togglebutton), image);
}

static void
on_togglebutton_mutevideo_clicked(CurrentCallView* view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    auto callToMute = priv->cpp->conversation->callId;
    if (!priv->cpp->conversation->confId.empty())
        callToMute = priv->cpp->conversation->confId;
    //auto muteVideoBtn = GTK_TOGGLE_BUTTON(priv->togglebutton_mutevideo);
    (*priv->cpp->accountInfo)->callModel->toggleMedia(callToMute,
        lrc::api::NewCallModel::Media::VIDEO);

    auto togglebutton = GTK_TOGGLE_BUTTON(priv->togglebutton_mutevideo);
    auto image = gtk_image_new_from_resource ("/cx/ring/RingGnome/mute_video");
    if (gtk_toggle_button_get_active(togglebutton))
        image = gtk_image_new_from_resource ("/cx/ring/RingGnome/unmute_video");
    gtk_button_set_image(GTK_BUTTON(togglebutton), image);
}

static gboolean
on_mouse_moved(CurrentCallView* view)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);
    auto priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->cpp->time_last_mouse_motion = g_get_monotonic_time();

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

    /* no need to upate quality if auto quality is enabled */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(priv->checkbutton_autoquality))) return;

    /* only update if the scale button is released, to reduce the number of updates */
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

    // now make sure the quality gets updated
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
        on_mouse_moved(view);
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
        g_debug("double click in video");
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
    if (peerUri == priv->cpp->conversation->participants.front()) {
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
        auto callInfo = (*priv->cpp->accountInfo)->callModel->getCallFromURI(peerUri, true);
        (*priv->cpp->accountInfo)->callModel->transferToCall(
            priv->cpp->conversation->callId, callInfo.id);
    } catch (std::out_of_range&) {
        // No current call found with this URI, perform a blind transfer
        (*priv->cpp->accountInfo)->callModel->transfer(
            priv->cpp->conversation->callId, peerUri);
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
get_sip_address_label(GtkListBoxRow* row)
{
    auto* row_children = gtk_container_get_children(GTK_CONTAINER(row));
    auto* box_infos = g_list_first(row_children)->data;
    auto* children = gtk_container_get_children(GTK_CONTAINER(box_infos));
    return GTK_LABEL(g_list_last(children)->data);
}

static void
transfer_to_conversation(GtkListBox*, GtkListBoxRow* row, CurrentCallView* self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);
    auto* sip_address = get_sip_address_label(row);
    transfer_to_peer(priv, gtk_label_get_text(GTK_LABEL(sip_address)));
}

static void
filter_transfer_list(CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    std::string currentFilter = gtk_entry_get_text(GTK_ENTRY(priv->siptransfer_filter_entry));

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_conversations), row))) {
        auto* sip_address = get_sip_address_label(GTK_LIST_BOX_ROW(children));;
        if (row == 0) {
            // Update searching item
            if (currentFilter.empty() || currentFilter == priv->cpp->conversation->participants.front()) {
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

            if (item_address == priv->cpp->conversation->participants.front())
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

CppImpl::CppImpl(CurrentCallView& widget)
    : self {&widget}
    , widgets {CURRENT_CALL_VIEW_GET_PRIVATE(&widget)}
{}

CppImpl::~CppImpl()
{
    QObject::disconnect(state_change_connection);
    QObject::disconnect(local_renderer_connection);
    QObject::disconnect(remote_renderer_connection);
    QObject::disconnect(smartinfo_refresh_connection);
    QObject::disconnect(new_message_connection);
    g_clear_object(&widgets->settings);

    g_source_remove(timer_fade);

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
               lrc::api::conversation::Info* conv_info)
{
    widgets->webkit_chat_container = GTK_WIDGET(chat_widget);
    conversation = conv_info;
    accountInfo = &account_info;
    setCallInfo();

    if ((*accountInfo)->profileInfo.type == lrc::api::profile::Type::RING)
        gtk_widget_hide(widgets->togglebutton_transfer);
    else {
        // Remove previous list
        while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(widgets->list_conversations), 10)))
            gtk_container_remove(GTK_CONTAINER(widgets->list_conversations), children);
        // Fill with SIP contacts
        add_transfer_contact("");  // Temporary item
        for (const auto& c : (*accountInfo)->conversationModel->getFilteredConversations(lrc::api::profile::Type::SIP))
            add_transfer_contact(c.participants.front());
        gtk_widget_show_all(widgets->list_conversations);
        gtk_widget_show(widgets->togglebutton_transfer);
    }

    set_record_animation(widgets);
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

    // NOTE/TODO we need to rewrite the video_widget file to use the new LRC.
    g_signal_connect(widgets->video_widget, "button-press-event",
                     G_CALLBACK(video_widget_on_button_press_in_screen_event), nullptr);

    // check if we already have a renderer
    auto callToRender = conversation->callId;
    if (!conversation->confId.empty())
        callToRender = conversation->confId;
    video_widget_push_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                                   (*accountInfo)->callModel->getRenderer(callToRender),
                                   VIDEO_RENDERER_REMOTE);

    // local renderer
    if (Video::PreviewManager::instance().isPreviewing())
        video_widget_push_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                                       Video::PreviewManager::instance().previewRenderer(),
                                       VIDEO_RENDERER_LOCAL);

    // callback for local renderer
    local_renderer_connection = QObject::connect(
        &Video::PreviewManager::instance(),
        &Video::PreviewManager::previewStarted,
        [this] (Video::Renderer* renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_LOCAL);
        }
    );

    smartinfo_refresh_connection = QObject::connect(
        &SmartInfoHub::instance(),
        &SmartInfoHub::changed,
        [this] { updateSmartInfo(); }
    );

    remote_renderer_connection = QObject::connect(
        &*(*accountInfo)->callModel,
        &lrc::api::NewCallModel::remotePreviewStarted,
        [this] (const std::string& callId, Video::Renderer* renderer) {
            if (conversation->callId == callId) {
                video_widget_push_new_renderer(VIDEO_WIDGET(widgets->video_widget),
                                               renderer,
                                               VIDEO_RENDERER_REMOTE);
            }
        });

    state_change_connection = QObject::connect(
        &*(*accountInfo)->callModel,
        &lrc::api::NewCallModel::callStatusChanged,
        [this] (const std::string& callId) {
            if (callId == conversation->callId) {
                updateState();
                updateNameAndPhoto();
            }
        });

    new_message_connection = QObject::connect(
        &*(*accountInfo)->conversationModel,
        &lrc::api::ConversationModel::newInteraction,
        [this] (const std::string& uid, uint64_t msgId, lrc::api::interaction::Info msg) {
            Q_UNUSED(uid)
            Q_UNUSED(msgId)
            Q_UNUSED(msg)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->togglebutton_chat), TRUE);
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

    // init chat view
    widgets->chat_view = chat_view_new(WEBKIT_CHAT_CONTAINER(widgets->webkit_chat_container),
                                       *accountInfo, conversation);
    gtk_container_add(GTK_CONTAINER(widgets->frame_chat), widgets->chat_view);

    g_signal_connect_swapped(widgets->chat_view, "new-interactions-displayed",
                             G_CALLBACK(on_new_chat_interactions), self);
    chat_view_set_header_visible(CHAT_VIEW(widgets->chat_view), FALSE);
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
    g_signal_connect_swapped(widgets->togglebutton_transfer, "clicked", G_CALLBACK(on_button_transfer_clicked), self);
    g_signal_connect_swapped(widgets->siptransfer_filter_entry, "activate", G_CALLBACK(on_siptransfer_filter_activated), self);
    g_signal_connect(widgets->siptransfer_filter_entry, "search-changed", G_CALLBACK(on_siptransfer_text_changed), self);
    g_signal_connect(widgets->list_conversations, "row-activated", G_CALLBACK(transfer_to_conversation), self);
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
    widgets->settings = g_settings_new_full(get_ring_schema(), nullptr, nullptr);
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
    auto callRendered = conversation->callId;

    if (!conversation->confId.empty())
        callRendered = conversation->confId;

    gtk_label_set_text(GTK_LABEL(widgets->label_duration),
    (*accountInfo)->callModel->getFormattedCallDuration(callRendered).c_str());

    auto call = (*accountInfo)->callModel->getCall(callRendered);
    gtk_widget_set_sensitive(GTK_WIDGET(widgets->togglebutton_muteaudio),
                             (call.type != lrc::api::call::Type::CONFERENCE));
    gtk_widget_set_sensitive(GTK_WIDGET(widgets->togglebutton_mutevideo),
                             (call.type != lrc::api::call::Type::CONFERENCE));
}

void
CppImpl::updateState()
{
    if (conversation) return;

    auto callId = conversation->callId;

    try {
        auto call = (*accountInfo)->callModel->getCall(callId);

        auto pauseBtn = GTK_TOGGLE_BUTTON(widgets->togglebutton_hold);
        auto image = gtk_image_new_from_resource ("/cx/ring/RingGnome/pause");
        if (call.status == lrc::api::call::Status::PAUSED)
            image = gtk_image_new_from_resource ("/cx/ring/RingGnome/play");
        gtk_button_set_image(GTK_BUTTON(pauseBtn), image);

        auto audioButton = GTK_TOGGLE_BUTTON(widgets->togglebutton_muteaudio);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->togglebutton_muteaudio), call.audioMuted);
        auto imageMuteAudio = gtk_image_new_from_resource ("/cx/ring/RingGnome/mute_audio");
        if (call.audioMuted)
            imageMuteAudio = gtk_image_new_from_resource ("/cx/ring/RingGnome/unmute_audio");
        gtk_button_set_image(GTK_BUTTON(audioButton), imageMuteAudio);

        auto videoButton = GTK_TOGGLE_BUTTON(widgets->togglebutton_mutevideo);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets->togglebutton_mutevideo), call.videoMuted);
        auto imageMuteVideo = gtk_image_new_from_resource ("/cx/ring/RingGnome/mute_video");
        if (call.videoMuted)
            imageMuteVideo = gtk_image_new_from_resource ("/cx/ring/RingGnome/unmute_video");
        gtk_button_set_image(GTK_BUTTON(videoButton), imageMuteVideo);

        gchar *status = g_strdup_printf("%s", lrc::api::call::to_string(call.status).c_str());
        gtk_label_set_text(GTK_LABEL(widgets->label_status), status);
        g_free(status);
    } catch (std::out_of_range& e) {
        g_warning("Can't update state for callId=%s", callId.c_str());
    }
}

void
CppImpl::updateNameAndPhoto()
{
    QVariant var_i = GlobalInstances::pixmapManipulator().conversationPhoto(
        *conversation,
        **(accountInfo),
        QSize(60, 60),
        false
    );
    std::shared_ptr<GdkPixbuf> image = var_i.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(widgets->image_peer), image.get());

    try {
        auto contactInfo = (*accountInfo)->contactModel->getContact(conversation->participants.front());
        auto name = contactInfo.profileInfo.alias;
        gtk_label_set_text(GTK_LABEL(widgets->label_name), name.c_str());

        auto bestId = contactInfo.registeredName;
        if (name != bestId) {
            gtk_label_set_text(GTK_LABEL(widgets->label_bestId), bestId.c_str());
            gtk_widget_show(widgets->label_bestId);
        }
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

}} // namespace <anonymous>::details

//==============================================================================

lrc::api::conversation::Info
current_call_view_get_conversation(CurrentCallView *self)
{
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), lrc::api::conversation::Info());
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

//==============================================================================

static void
current_call_view_init(CurrentCallView *view)
{
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);
    gtk_widget_init_template(GTK_WIDGET(view));

    // CppImpl ctor
    priv->cpp = new details::CppImpl {*view};
    priv->cpp->init();
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
                      lrc::api::conversation::Info* conversation)
{
    auto* self = g_object_new(CURRENT_CALL_VIEW_TYPE, NULL);
    auto* priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    priv->cpp->setup(chat_widget, accountInfo, conversation);
    return GTK_WIDGET(self);
}
