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

#include "messagingwidget.h"

// std
#include <string>

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
    GtkWindow parent;
};

struct _MessagingWidgetClass
{
    GtkGridClass parent_class;
};

typedef struct _MessagingWidgetPrivate MessagingWidgetPrivate;

struct _MessagingWidgetPrivate
{
    GtkWidget *label_letmessage;
    GtkWidget *label_duration;
    GtkWidget *image_send;
    GtkWidget *image_stop;
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

G_DEFINE_TYPE_WITH_PRIVATE(MessagingWidget, messaging_widget, GTK_TYPE_GRID);
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
               lrc::api::conversation::Info* conversation,
               AccountInfoPointer const & accountInfo);

    MessagingWidget* self = nullptr; // The GTK widget itself
    MessagingWidgetPrivate* widgets = nullptr;

    // store current recording location
    std::string save_file_name;

    // internal state: used to keep track of current layout
    MessagingWidgetState state;

    lrc::api::AVModel* avModel_;
    lrc::api::conversation::Info* conversation_;
    AccountInfoPointer const * accountInfo_;
};

static void
set_state(MessagingWidget *self, MessagingWidgetState state)
{
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    MessagingWidgetPrivate *priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    // state transition validity checking is expected to be done by the caller
    switch (state) {
    case MESSAGING_WIDGET_STATE_INIT:
        gtk_label_set_text(GTK_LABEL(priv->label_letmessage), _("Oops, contact is busy. Want to let a message ?"));
        gtk_widget_show(priv->label_letmessage);
        gtk_widget_show(priv->button_record_audio);
        gtk_widget_set_sensitive(priv->button_record_audio, true);
        gtk_widget_show(priv->button_end_without_message);
        gtk_widget_set_sensitive(priv->button_end_without_message, true);
        break;
    case MESSAGING_WIDGET_REC_AUDIO:
        gtk_button_set_image(GTK_BUTTON(priv->button_record_audio), priv->image_stop);
        gtk_widget_set_tooltip_text(GTK_WIDGET(priv->button_record_audio), _("Stop recording"));
        break;
    case MESSAGING_WIDGET_AUDIO_REC_SUCCESS:
        gtk_button_set_image(GTK_BUTTON(priv->button_record_audio), priv->image_send);
        gtk_widget_set_tooltip_text(GTK_WIDGET(priv->button_record_audio), _("Send recorded message"));
        break;
    default:
        g_debug("set_state: invalid state.");
        return;
    }

    priv->cpp->state = state;
}

static void
on_timer_update(MessagingWidget* self)
{
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    gtk_label_set_text(GTK_LABEL(priv->label_duration), "" /* TODO formatted time here */);
}

static void
on_leave_widget(MessagingWidget *self) {
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    priv->cpp->save_file_name.assign("");
    g_signal_emit(self, selfie_signals[LEAVE_ACTION], 0);
}

static void
on_send_button_pressed(GtkWidget *button, MessagingWidget *self)
{
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    if (auto model = (*priv->cpp->accountInfo_)->conversationModel.get()) {
        model->sendFile(priv->cpp->conversation_->uid, priv->cpp->save_file_name,
                        g_path_get_basename(priv->cpp->save_file_name.c_str()));
        on_leave_widget(self);
    }

    on_leave_widget(self);
}

static void
on_stop_audio_record_pressed(GtkWidget *button, MessagingWidget *self)
{
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    priv->cpp->avModel_->stopLocalRecorder(priv->cpp->save_file_name);
    set_state(self, MESSAGING_WIDGET_AUDIO_REC_SUCCESS);
}

static void
on_record_audio_pressed(GtkWidget *button, MessagingWidget *self)
{
    g_return_if_fail(IS_MESSAGING_WIDGET(self));
    auto priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    std::string save_file_name = priv->cpp->avModel_->startLocalRecorder(true);
    if (save_file_name.empty()) {
        return;
    }

    priv->cpp->save_file_name.assign(save_file_name);
    set_state(self, MESSAGING_WIDGET_REC_AUDIO);
}

CppImpl::CppImpl(MessagingWidget& widget)
    : self {&widget}
    , widgets {MESSAGING_WIDGET_GET_PRIVATE(&widget)}
{}

CppImpl::~CppImpl()
{
}

void
CppImpl::init()
{
    // CSS styles
    auto provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
        ".flat-button { border: 0; border-radius: 50%; transition: all 0.3s ease; } \
        .grey-button { background: #d7d7d7; } \
        .grey-button:hover { background: #acacac; }",
        -1, nullptr
    );

    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    // signals
    g_signal_connect(widgets->button_record_audio, "clicked", G_CALLBACK(on_record_audio_pressed), self);
    g_signal_connect(widgets->button_end_without_message, "clicked", G_CALLBACK(on_leave_widget), self);
}

void
CppImpl::setup(lrc::api::AVModel& avModel,
               lrc::api::conversation::Info* conversation,
               AccountInfoPointer const & accountInfo)
{
    conversation_ = conversation;
    accountInfo_ = &accountInfo;
    avModel_ = &avModel;
}

}} // namespace details

static void
messaging_widget_dispose(GObject *object)
{
    MessagingWidgetPrivate *priv = MESSAGING_WIDGET_GET_PRIVATE(object);
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

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass), "/cx/ring/RingGnome/messagingwidget.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, image_send);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, image_stop);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MessagingWidget, image_record_audio);
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
                     lrc::api::conversation::Info* conversation,
                     AccountInfoPointer const & accountInfo)
{
    auto self = g_object_new(MESSAGING_WIDGET_TYPE, NULL);
    MessagingWidgetPrivate *priv = MESSAGING_WIDGET_GET_PRIVATE(self);

    priv->cpp->setup(avModel, conversation, accountInfo);

    return GTK_WIDGET(self);
}

void
messaging_widget_set_peer_name(MessagingWidget *self, std::string name)
{
    /* TODO */
}
