/*
 *  Copyright (C) 2018-2022 Savoir-faire Linux Inc.
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

#include "messagingwidget.h"

// std
#include <string>
#include <sstream>
#include <iomanip> // for std::put_time
#include <chrono>

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// Lrc
#include <api/avmodel.h>
#include <api/account.h>
#include <api/conversationmodel.h>

namespace { namespace details
{
class CppImpl;
}}

struct _MessagingWidget
{
    GtkVBox parent;
};

struct _MessagingWidgetClass
{
    GtkVBoxClass parent_class;
};

typedef struct _MessagingWidgetPrivate MessagingWidgetPrivate;

struct _MessagingWidgetPrivate
{
    GtkWidget *label_letmessage;
    GtkWidget *label_duration;
    GtkWidget *image_send;
    GtkWidget *image_stop;
    GtkWidget *box_red_dot;
    GtkWidget *box_timer;
    GtkWidget *image_record_audio;
    GtkWidget *button_record_audio;
    GtkWidget *button_end_without_message;

    details::CppImpl* cpp; ///< Non-UI and C++ only code
};

/* signals */
enum {
    LEAVE_ACTION,
    LAST_SIGNAL
};

static guint selfie_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(MessagingWidget, messaging_widget, GTK_TYPE_VBOX);
#define MESSAGING_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MESSAGING_WIDGET_TYPE, MessagingWidgetPrivate))

namespace { namespace details
{

class CppImpl
{
public:
    explicit CppImpl(MessagingWidget& widget);
    ~CppImpl();

    void init();
    void setup(lrc::api::AVModel& avModel,
               lrc::api::conversation::Info& conversation,
               AccountInfoPointer const & accountInfo);
    void set_state(MessagingWidgetState newState);

    MessagingWidget* self = nullptr; // The GTK widget itself
    MessagingWidgetPrivate* widgets = nullptr;

    // store current recording location
    std::string saveFileName_;

    // internal state: used to keep track of current layout
    MessagingWidgetState state_;

    // timer related
    std::chrono::seconds timeElapsed_ {0};
    static const std::chrono::seconds timeLimit;
    guint timerCallbackId_ {0};

    lrc::api::AVModel* avModel_;
    lrc::api::conversation::Info* conversation_;
    AccountInfoPointer const * accountInfo_;
};

const std::chrono::seconds CppImpl::timeLimit = std::chrono::seconds(30);

static gboolean
on_timer_update(gpointer user_data)
{
    MessagingWidget *self = MESSAGING_WIDGET(user_data);
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);
    gboolean ret = TRUE;

    if (priv->cpp->state_ != MESSAGING_WIDGET_REC_AUDIO) {
        // state changed, stop timer
        ret = FALSE;
    } else {
        priv->cpp->timeElapsed_++;
    }

    std::time_t elapsed = priv->cpp->timeElapsed_.count();
    std::stringstream ss;
    ss << std::put_time(std::localtime(&elapsed), "%M:%S");
    gtk_label_set_text(GTK_LABEL(priv->label_duration), ss.str().c_str());

    return ret;
}

static void
on_leave_widget(MessagingWidget *self) {
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    // no need to stop recording here, will be done in the destructor
    g_signal_emit(self, selfie_signals[LEAVE_ACTION], 0);
}

static void
on_record_button_pressed(MessagingWidget *self)
{
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    switch(priv->cpp->state_) {
    case MESSAGING_WIDGET_STATE_INIT:
        priv->cpp->set_state(MESSAGING_WIDGET_REC_AUDIO);
        break;
    case MESSAGING_WIDGET_REC_AUDIO:
        priv->cpp->set_state(MESSAGING_WIDGET_AUDIO_REC_SUCCESS);
        break;
    case MESSAGING_WIDGET_AUDIO_REC_SUCCESS:
        priv->cpp->set_state(MESSAGING_WIDGET_REC_SENT);
        break;
    default:
        priv->cpp->set_state(MESSAGING_WIDGET_STATE_INIT);
        g_error("on_record_button_pressed: invalid state");
    }
}

CppImpl::CppImpl(MessagingWidget& widget)
    : self {&widget}
    , widgets {MESSAGING_WIDGET_GET_PRIVATE(&widget)}
{}

CppImpl::~CppImpl()
{
    if (state_ == MESSAGING_WIDGET_REC_AUDIO) {
        // stop current recording
        set_state(MESSAGING_WIDGET_AUDIO_REC_SUCCESS);
    }
}

void
CppImpl::init()
{
    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".flat-button { border: 0; border-radius: 50%; transition: all 0.3s ease; } \
        .grey-button { background: #dfdfdf; } \
        .grey-button:hover { background: #cecece; } \
        .time-label { padding: 5px; } \
        .timer-box { border: solid 2px; border-radius: 6px; }",
        -1, nullptr
    );

    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // signals
    g_signal_connect_swapped(widgets->button_record_audio, "clicked", G_CALLBACK(on_record_button_pressed), self);
    g_signal_connect_swapped(widgets->button_end_without_message, "clicked", G_CALLBACK(on_leave_widget), self);

    set_state(MESSAGING_WIDGET_STATE_INIT);
}

void
CppImpl::setup(lrc::api::AVModel& avModel,
               lrc::api::conversation::Info& conversation,
               AccountInfoPointer const & accountInfo)
{
    conversation_ = &conversation;
    accountInfo_ = &accountInfo;
    avModel_ = &avModel;
}

void
CppImpl::set_state(MessagingWidgetState state)
{
    // state transition validity checking is expected to be done by the caller
    switch (state) {
    case MESSAGING_WIDGET_STATE_INIT:
        gtk_label_set_text(GTK_LABEL(widgets->label_letmessage), _("Contact appears to be busy. Leave a message ?"));
        gtk_widget_show(widgets->label_letmessage);
        gtk_widget_show(widgets->button_record_audio);
        gtk_widget_set_sensitive(widgets->button_record_audio, true);
        gtk_widget_show(widgets->button_end_without_message);
        gtk_widget_set_sensitive(widgets->button_end_without_message, true);
        break;
    case MESSAGING_WIDGET_REC_AUDIO:
    {
        QString file_name = avModel_->startLocalMediaRecorder(avModel_->getDefaultDevice());
        if (file_name.isEmpty()) {
            g_warning("set_state: failed to start recording");
            return;
        }

        saveFileName_ = file_name.toStdString();

        timerCallbackId_ = g_timeout_add_seconds(1, on_timer_update, self);
        gtk_widget_show(widgets->box_timer);
        gtk_widget_show(widgets->label_duration);
        gtk_widget_show(widgets->box_red_dot);
        gtk_button_set_image(GTK_BUTTON(widgets->button_record_audio), widgets->image_stop);
        gtk_widget_set_tooltip_text(GTK_WIDGET(widgets->button_record_audio), _("Stop recording"));
        break;
    }
    case MESSAGING_WIDGET_AUDIO_REC_SUCCESS:
        if (!saveFileName_.empty()) {
            avModel_->stopLocalRecorder(saveFileName_.c_str());
        }

        gtk_widget_hide(widgets->box_red_dot);
        if (timerCallbackId_) {
            g_source_remove(timerCallbackId_);
        }

        gtk_button_set_image(GTK_BUTTON(widgets->button_record_audio), widgets->image_send);
        gtk_widget_set_tooltip_text(GTK_WIDGET(widgets->button_record_audio), _("Send recorded message"));
        break;
    case MESSAGING_WIDGET_REC_SENT:
        if (auto model = (*accountInfo_)->conversationModel.get()) {
            model->sendFile(conversation_->uid, saveFileName_.c_str(), g_path_get_basename(saveFileName_.c_str()));
            saveFileName_ = "";
        }

        on_leave_widget(self);
        break;
    default:
        g_debug("set_state: invalid state.");
        return;
    }

    state_ = state;

    if (state_ == MESSAGING_WIDGET_AUDIO_REC_SUCCESS) {
        on_timer_update(self);
    }
}

}} // namespace details

static void
messaging_widget_dispose(GObject *object)
{
    MessagingWidgetPrivate *priv = MESSAGING_WIDGET_GET_PRIVATE(object);

    delete priv->cpp;
    priv->cpp = nullptr;

    G_OBJECT_CLASS(messaging_widget_parent_class)->dispose(object);
}

static void
messaging_widget_finalize(GObject *object)
{
    G_OBJECT_CLASS(messaging_widget_parent_class)->finalize(object);
}

static void
messaging_widget_class_init(MessagingWidgetClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = messaging_widget_finalize;
    G_OBJECT_CLASS(klass)->dispose = messaging_widget_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/net/jami/JamiGnome/messagingwidget.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, image_send);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, image_stop);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, image_record_audio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, box_red_dot);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, box_timer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, button_record_audio);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, button_end_without_message);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, label_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, label_letmessage);

    // Define custom signals
    selfie_signals[LEAVE_ACTION] = g_signal_new ("leave_action",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

static void
messaging_widget_init(MessagingWidget *self)
{
    MessagingWidgetPrivate *priv = MESSAGING_WIDGET_GET_PRIVATE(self);
    gtk_widget_init_template(GTK_WIDGET(self));

    // CppImpl ctor
    priv->cpp = new details::CppImpl {*self};
    priv->cpp->init();
}

GtkWidget*
messaging_widget_new(lrc::api::AVModel& avModel,
                     lrc::api::conversation::Info& conversation,
                     AccountInfoPointer const & accountInfo)
{
    auto self = g_object_new(MESSAGING_WIDGET_TYPE, NULL);
    MessagingWidgetPrivate *priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    priv->cpp->setup(avModel, conversation, accountInfo);

    return GTK_WIDGET(self);
}
