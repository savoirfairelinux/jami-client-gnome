/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "currentcallview.h"

#include <gtk/gtk.h>
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
#include "video/videowindow.h"
#include "ringnotify.h"
#include <audio/codecmodel.h>
#include <account.h>

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
    GtkWidget *image_peer;
    GtkWidget *label_identity;
    GtkWidget *label_status;
    GtkWidget *label_duration;
    GtkWidget *frame_video;
    GtkWidget *video_widget;
    GtkWidget *revealer_chat;
    GtkWidget *togglebutton_chat;
    GtkWidget *textview_chat;
    GtkWidget *button_chat_input;
    GtkWidget *entry_chat_input;
    GtkWidget *scrolledwindow_chat;
    GtkWidget *fullscreen_window;
    GtkWidget *buttonbox_call_controls;
    GtkWidget *button_hangup;
    GtkWidget *scalebutton_quality;

    /* flag used to keep track of the video quality scale pressed state;
     * we do not want to update the codec bitrate until the user releases the
     * scale button */
    gboolean quality_scale_pressed;

    Call *call;

    QMetaObject::Connection state_change_connection;
    QMetaObject::Connection call_details_connection;
    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection remote_renderer_connection;
    QMetaObject::Connection media_added_connection;
    QMetaObject::Connection new_message_connection;
    QMetaObject::Connection incoming_msg_connection;
};

G_DEFINE_TYPE_WITH_PRIVATE(CurrentCallView, current_call_view, GTK_TYPE_BOX);

#define CURRENT_CALL_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CURRENT_CALL_VIEW_TYPE, CurrentCallViewPrivate))

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
    QObject::disconnect(priv->media_added_connection);
    QObject::disconnect(priv->new_message_connection);
    QObject::disconnect(priv->incoming_msg_connection);

    if (priv->fullscreen_window) {
        gtk_widget_destroy(priv->fullscreen_window);
        priv->fullscreen_window = NULL;
    }

    G_OBJECT_CLASS(current_call_view_parent_class)->dispose(object);
}

static void
chat_toggled(GtkToggleButton *togglebutton, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_chat),
                                  gtk_toggle_button_get_active(togglebutton));

    if (gtk_toggle_button_get_active(togglebutton)) {
        /* create an outgoing media to bring up chat history, if any */
        priv->call->addOutgoingMedia<Media::Text>();
        /* change focus to the chat entry */
        gtk_widget_grab_focus(priv->entry_chat_input);
    }
}

static void
send_chat(G_GNUC_UNUSED GtkWidget *widget, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    /* make sure there is text to send */
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(priv->entry_chat_input));
    if (text && strlen(text) > 0) {
        QMap<QString, QString> messages;
        messages["text/plain"] = text;
        priv->call->addOutgoingMedia<Media::Text>()->send(messages);
        /* clear the entry */
        gtk_entry_set_text(GTK_ENTRY(priv->entry_chat_input), "");
    }
}

static void
scroll_to_bottom(GtkAdjustment *adjustment, G_GNUC_UNUSED gpointer user_data)
{
    gtk_adjustment_set_value(adjustment,
        gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));
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
    GtkWidget *dock = gtk_scale_button_get_popup(button);

    // the dock is a popover which contains a box
    // which contains the + button, scale, and - button
    // we want to get the scale
    if (GtkWidget *box = gtk_bin_get_child(GTK_BIN(dock))) {
        if (GTK_IS_FRAME(box)) {
            // support older versions of gtk; the box used to be in a frame
            box = gtk_bin_get_child(GTK_BIN(box));
        }
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
quality_changed(GtkScaleButton *button, G_GNUC_UNUSED gdouble value, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    /* only update if the scale button is released, to reduce the number of updates */
    if (priv->quality_scale_pressed) return;

    /* we get the value directly from the widget, in case this function is not
     * called from the event */
    unsigned int bitrate = (unsigned int)gtk_scale_button_get_value(button);

    if (const auto& codecModel = priv->call->account()->codecModel()) {
        const auto& videoCodecs = codecModel->videoCodecs();
        for (int i=0; i < videoCodecs->rowCount();i++) {
            const auto& idx = videoCodecs->index(i,0);
            g_debug("setting codec bitrate to %u", bitrate);
            videoCodecs->setData(idx, QString::number(bitrate), CodecModel::Role::BITRATE);
        }
        codecModel << CodecModel::EditAction::SAVE;
    }
}

static gboolean
quality_button_pressed(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event, CurrentCallView *self)
{
    g_debug("button pressed");
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    priv->quality_scale_pressed = TRUE;

    return FALSE; // propogate the event
}

static gboolean
quality_button_released(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED GdkEvent *event, CurrentCallView *self)
{
    g_debug("button released");
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(self), FALSE);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    priv->quality_scale_pressed = FALSE;

    /* now make sure the quality gets updated */
    quality_changed(GTK_SCALE_BUTTON(priv->scalebutton_quality), 0, self);

    return FALSE; // propogate the event
}

static void
current_call_view_init(CurrentCallView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    g_signal_connect(priv->togglebutton_chat, "toggled", G_CALLBACK(chat_toggled), view);
    g_signal_connect(priv->button_chat_input, "clicked", G_CALLBACK(send_chat), view);
    g_signal_connect(priv->entry_chat_input, "activate", G_CALLBACK(send_chat), view);

    /* the adjustment params will change only when the model is created and when
     * new messages are added; in these cases we want to scroll to the bottom of
     * the chat treeview */
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(priv->scrolledwindow_chat));
    g_signal_connect(adjustment, "changed", G_CALLBACK(scroll_to_bottom), NULL);

    /* customize the quality button scale */
    if (GtkScale *scale = gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality))) {
        gtk_scale_set_draw_value(scale, TRUE);
        gtk_scale_set_value_pos(scale, GTK_POS_RIGHT);
        gtk_scale_set_digits(scale, 0);
    }
}

static void
current_call_view_class_init(CurrentCallViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = current_call_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/currentcallview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, image_peer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_identity);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_status);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, label_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, frame_video);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, revealer_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, togglebutton_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, textview_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, button_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, entry_chat_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, scrolledwindow_chat);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, buttonbox_call_controls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, button_hangup);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), CurrentCallView, scalebutton_quality);
}

GtkWidget *
current_call_view_new(void)
{
    return (GtkWidget *)g_object_new(CURRENT_CALL_VIEW_TYPE, NULL);
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
on_fullscreen_destroy(CurrentCallView *view)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(view));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* fullscreen is being destroyed, clear the pointer and un-pause the rendering
     * in this window */
    priv->fullscreen_window = NULL;
    video_widget_pause_rendering(VIDEO_WIDGET(priv->video_widget), FALSE);
}

static gboolean
on_button_press_in_video_event(GtkWidget *self, GdkEventButton *event, CurrentCallView *view)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);
    g_return_val_if_fail(IS_CURRENT_CALL_VIEW(view), FALSE);
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    /* on double click */
    if (event->type == GDK_2BUTTON_PRESS) {
        if (priv->fullscreen_window) {
            /* destroy the fullscreen */
            gtk_widget_destroy(priv->fullscreen_window);
        } else {
            /* pause rendering in this window and create fullscreen */
            video_widget_pause_rendering(VIDEO_WIDGET(priv->video_widget), TRUE);

            priv->fullscreen_window = video_window_new(priv->call,
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))));

            /* connect to destruction of fullscreen so we know when to un-pause
             * the rendering in thiw window */
            g_signal_connect_swapped(priv->fullscreen_window,
                                     "destroy",
                                     G_CALLBACK(on_fullscreen_destroy),
                                     view);

            /* present the fullscreen widnow */
            gtk_window_present(GTK_WINDOW(priv->fullscreen_window));
            gtk_window_fullscreen(GTK_WINDOW(priv->fullscreen_window));
        }
    }

    /* the event has been fully handled */
    return TRUE;
}

static void
print_message_to_buffer(const QModelIndex &idx, GtkTextBuffer *buffer)
{
    if (idx.isValid()) {
        auto message = idx.data().value<QString>().toUtf8();
        auto sender = idx.data(static_cast<int>(Media::TextRecording::Role::AuthorDisplayname)).value<QString>().toUtf8();

        GtkTextIter iter;

        /* unless its the very first message, insert a new line */
        if (idx.row() != 0) {
            gtk_text_buffer_get_end_iter(buffer, &iter);
            gtk_text_buffer_insert(buffer, &iter, "\n", -1);
        }

        auto format_sender = g_strconcat(sender.constData(), ": ", NULL);
        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert_with_tags_by_name(buffer, &iter,
                                                 format_sender, -1,
                                                 "bold", NULL);
        g_free(format_sender);

        /* if the sender name is too long, insert a new line after it */
        if (sender.length() > 20) {
            gtk_text_buffer_get_end_iter(buffer, &iter);
            gtk_text_buffer_insert(buffer, &iter, "\n", -1);
        }

        gtk_text_buffer_get_end_iter(buffer, &iter);
        gtk_text_buffer_insert(buffer, &iter, message.constData(), -1);

    } else {
        g_warning("QModelIndex in im model is not valid");
    }
}

static void
parse_chat_model(QAbstractItemModel *model, CurrentCallView *self)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    /* new model, disconnect from the old model updates and clear the text buffer */
    QObject::disconnect(priv->new_message_connection);

    GtkTextBuffer *new_buffer = gtk_text_buffer_new(NULL);
    gtk_text_view_set_buffer(GTK_TEXT_VIEW(priv->textview_chat), new_buffer);

    /* add tags to the buffer */
    gtk_text_buffer_create_tag(new_buffer, "bold", "weight", PANGO_WEIGHT_BOLD, NULL);

    g_object_unref(new_buffer);

    /* put all the messages in the im model into the text view */
    for (int row = 0; row < model->rowCount(); ++row) {
        QModelIndex idx = model->index(row, 0);
        print_message_to_buffer(idx, new_buffer);
    }

    /* append new messages */
    priv->new_message_connection = QObject::connect(
        model,
        &QAbstractItemModel::rowsInserted,
        [priv, model] (const QModelIndex &parent, int first, int last) {
            for (int row = first; row <= last; ++row) {
                QModelIndex idx = model->index(row, 0, parent);
                print_message_to_buffer(idx, gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->textview_chat)));
            }
        }
    );
}

void
monitor_incoming_message(CurrentCallView *self, Media::Text *media)
{
    g_return_if_fail(IS_CURRENT_CALL_VIEW(self));
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(self);

    /* connect to incoming chat messages to open the chat view */
    QObject::disconnect(priv->incoming_msg_connection);
    priv->incoming_msg_connection = QObject::connect(
        media,
        &Media::Text::messageReceived,
        [priv] (G_GNUC_UNUSED const QMap<QString,QString>& m) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_chat), TRUE);
        }
    );
}

void
current_call_view_set_call_info(CurrentCallView *view, const QModelIndex& idx) {
    CurrentCallViewPrivate *priv = CURRENT_CALL_VIEW_GET_PRIVATE(view);

    priv->call = CallModel::instance().getCall(idx);

    /* get call image */
    QVariant var_i = GlobalInstances::pixmapManipulator().callPhoto(priv->call, QSize(60, 60), false);
    std::shared_ptr<GdkPixbuf> image = var_i.value<std::shared_ptr<GdkPixbuf>>();
    gtk_image_set_from_pixbuf(GTK_IMAGE(priv->image_peer), image.get());

    /* get name */
    QVariant var = idx.model()->data(idx, static_cast<int>(Call::Role::Name));
    QByteArray ba_name = var.toString().toUtf8();
    gtk_label_set_text(GTK_LABEL(priv->label_identity), ba_name.constData());

    /* change some things depending on call state */
    update_state(view, priv->call);
    update_details(view, priv->call);

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

    /* video widget */
    priv->video_widget = video_widget_new();
    gtk_container_add(GTK_CONTAINER(priv->frame_video), priv->video_widget);
    gtk_widget_show_all(priv->frame_video);

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

    /* handle button event */
    g_signal_connect(priv->video_widget, "button-press-event", G_CALLBACK(video_widget_on_button_press_in_screen_event), priv->call);

    /* catch double click to make full screen */
    g_signal_connect(priv->video_widget, "button-press-event",
                     G_CALLBACK(on_button_press_in_video_event),
                     view);

    /* Drag and drop*/
    g_signal_connect(priv->video_widget, "drag-data-received", G_CALLBACK(video_widget_on_drag_data_received), priv->call);

    /* check if text media is already present */
    if (priv->call->hasMedia(Media::Media::Type::TEXT, Media::Media::Direction::IN)) {
        Media::Text *text = priv->call->firstMedia<Media::Text>(Media::Media::Direction::IN);
        parse_chat_model(text->recording()->instantMessagingModel(), view);
        monitor_incoming_message(view, text);
    } else if (priv->call->hasMedia(Media::Media::Type::TEXT, Media::Media::Direction::OUT)) {
        Media::Text *text = priv->call->firstMedia<Media::Text>(Media::Media::Direction::OUT);
        parse_chat_model(text->recording()->instantMessagingModel(), view);
        monitor_incoming_message(view, text);
    } else {
        /* monitor media for messaging text messaging */
        priv->media_added_connection = QObject::connect(
            priv->call,
            &Call::mediaAdded,
            [view, priv] (Media::Media* media) {
                if (media->type() == Media::Media::Type::TEXT) {
                    parse_chat_model(((Media::Text*)media)->recording()->instantMessagingModel(), view);
                    monitor_incoming_message(view, (Media::Text*)media);
                    QObject::disconnect(priv->media_added_connection);
                }
            }
        );
    }

    /* check if there were any chat notifications and open the chat view if so */
    if (ring_notify_close_chat_notification(priv->call))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->togglebutton_chat), TRUE);

    /* get the current codec quality and set that as the initial slider value
     * for now we assume that all codecs have the same quality */
    if (const auto& codecModel = priv->call->account()->codecModel()) {
        const auto& videoCodecs = codecModel->videoCodecs();
        if (videoCodecs->rowCount() > 0) {
            const auto& idx = videoCodecs->index(0,0);
            double value = idx.data(static_cast<int>(CodecModel::Role::BITRATE)).toDouble();
            gtk_scale_button_set_value(GTK_SCALE_BUTTON(priv->scalebutton_quality), value);
        }
    }
    g_signal_connect(priv->scalebutton_quality, "value-changed", G_CALLBACK(quality_changed), view);
    g_signal_connect(gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality)),
                     "button-press-event", G_CALLBACK(quality_button_pressed), view);
    g_signal_connect(gtk_scale_button_get_scale(GTK_SCALE_BUTTON(priv->scalebutton_quality)),
                     "button-release-event", G_CALLBACK(quality_button_released), view);
}
