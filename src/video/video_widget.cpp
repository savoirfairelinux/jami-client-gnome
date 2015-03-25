/*
 *  Copyright (C) 2004-2015 Savoir-Faire Linux Inc.
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
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "video_widget.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <video/renderer.h>
#include <video/sourcemodel.h>
#include <video/devicemodel.h>
#include <QtCore/QUrl>
#include "../defines.h"
#include <stdlib.h>
#include "xrectsel.h"

#define VIDEO_LOCAL_SIZE            150
#define VIDEO_LOCAL_OPACITY_DEFAULT 150 /* out of 255 */

/* check video frame queues at this rate;
 * use 30 ms (about 30 fps) since we don't expect to
 * receive video frames faster than that */
#define FRAME_RATE_PERIOD           30

struct _VideoWidgetClass {
    GtkBinClass parent_class;
};

struct _VideoWidget {
    GtkBin parent;
};

typedef struct _VideoWidgetPrivate VideoWidgetPrivate;

typedef struct _VideoWidgetRenderer VideoWidgetRenderer;

struct _VideoWidgetPrivate {
    GtkWidget               *clutter_widget;
    ClutterActor            *stage;
    ClutterActor            *video_container;

    /* remote peer data */
    VideoWidgetRenderer     *remote;

    /* local peer data */
    VideoWidgetRenderer     *local;

    guint                    frame_timeout_source;
};

struct _VideoWidgetRenderer {
    GAsyncQueue             *frame_queue;
    ClutterActor            *actor;
    Video::Renderer         *renderer;
    QMetaObject::Connection  frame_update;
    QMetaObject::Connection  render_stop;
    QMetaObject::Connection  render_start;
};

G_DEFINE_TYPE_WITH_PRIVATE(VideoWidget, video_widget, GTK_TYPE_BIN);

#define VIDEO_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VIDEO_WIDGET_TYPE, VideoWidgetPrivate))

typedef struct _FrameInfo
{
    ClutterActor   *image_actor;
    ClutterContent *image;
    guchar         *data;
    gint            data_size;
    gint            width;
    gint            height;
} FrameInfo;

/* static prototypes */
static FrameInfo *prepare_framedata(Video::Renderer *renderer, ClutterActor* image_actor);
static void free_framedata(gpointer data);
static void clutter_render_image(FrameInfo *frame);
static gboolean check_frame_queue(VideoWidget *self);
static void renderer_stop(VideoWidgetRenderer *renderer);
static void renderer_start(VideoWidgetRenderer *renderer);
static void video_widget_set_renderer(VideoWidgetRenderer *renderer);
static void on_drag_data_received(GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint32, gpointer);
static gboolean on_button_press_in_screen_event(GtkWidget *, GdkEventButton *, gpointer);

/*
 * video_widget_dispose()
 *
 * The dispose function for the video_widget class.
 */
static void
video_widget_dispose(GObject *object)
{
    VideoWidget *self = VIDEO_WIDGET(object);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    QObject::disconnect(priv->remote->frame_update);
    QObject::disconnect(priv->remote->render_stop);
    QObject::disconnect(priv->remote->render_start);

    QObject::disconnect(priv->local->frame_update);
    QObject::disconnect(priv->local->render_stop);
    QObject::disconnect(priv->local->render_start);

    /* dispose may be called multiple times, make sure
     * not to call g_source_remove more than once */
    if (priv->frame_timeout_source) {
        g_source_remove(priv->frame_timeout_source);
        priv->frame_timeout_source = 0;
    }

    if (priv->remote->frame_queue) {
        g_async_queue_unref(priv->remote->frame_queue);
        priv->remote->frame_queue = NULL;
    }

    if (priv->local->frame_queue) {
        g_async_queue_unref(priv->local->frame_queue);
        priv->local->frame_queue = NULL;
    }

    G_OBJECT_CLASS(video_widget_parent_class)->dispose(object);
}


/*
 * video_widget_finalize()
 *
 * The finalize function for the video_widget class.
 */
static void
video_widget_finalize(GObject *object)
{
    VideoWidget *self = VIDEO_WIDGET(object);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    g_free(priv->remote);
    g_free(priv->local);

    G_OBJECT_CLASS(video_widget_parent_class)->finalize(object);
}


/*
 * video_widget_class_init()
 *
 * This function init the video_widget_class.
 */
static void
video_widget_class_init(VideoWidgetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    /* override method */
    object_class->dispose = video_widget_dispose;
    object_class->finalize = video_widget_finalize;
}


/*
 * video_widget_init()
 *
 * This function init the video_widget.
 * - init clutter
 * - init all the widget members
 */
static void
video_widget_init(VideoWidget *self)
{
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* init clutter widget */
    priv->clutter_widget = gtk_clutter_embed_new();
    /* add it to the video_widget */
    gtk_container_add(GTK_CONTAINER(self), priv->clutter_widget);
    /* get the stage */
    priv->stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->clutter_widget));

    /* layout manager is used to arrange children in space, here we ask clutter
     * to align children to fill the space when resizing the window */
    clutter_actor_set_layout_manager(priv->stage,
        clutter_bin_layout_new(CLUTTER_BIN_ALIGNMENT_FILL, CLUTTER_BIN_ALIGNMENT_FILL));

    /* add a scene container where we can add and remove our actors */
    priv->video_container = clutter_actor_new();
    clutter_actor_set_background_color(priv->video_container, CLUTTER_COLOR_Black);
    clutter_actor_add_child(priv->stage, priv->video_container);

    /* init the remote and local structs */
    priv->remote = g_new0(VideoWidgetRenderer, 1);
    priv->local = g_new0(VideoWidgetRenderer, 1);

    /* arrange remote actors */
    priv->remote->actor = clutter_actor_new();
    clutter_actor_insert_child_below(priv->video_container, priv->remote->actor, NULL);
    /* the remote camera must always fill the container size */
    ClutterConstraint *constraint = clutter_bind_constraint_new(priv->video_container,
                                                                CLUTTER_BIND_SIZE, 0);
    clutter_actor_add_constraint(priv->remote->actor, constraint);

    /* arrange local actor */
    priv->local->actor = clutter_actor_new();
    clutter_actor_insert_child_above(priv->video_container, priv->local->actor, NULL);
    /* set size to square, but it will stay the aspect ratio when the image is rendered */
    clutter_actor_set_size(priv->local->actor, VIDEO_LOCAL_SIZE, VIDEO_LOCAL_SIZE);
    /* set position constraint to right cornder */
    constraint = clutter_align_constraint_new(priv->video_container,
                                              CLUTTER_ALIGN_BOTH, 0.99);
    clutter_actor_add_constraint(priv->local->actor, constraint);
    clutter_actor_set_opacity(priv->local->actor,
                              VIDEO_LOCAL_OPACITY_DEFAULT);

    /* init frame queues and the timeout sources to check them */
    priv->remote->frame_queue = g_async_queue_new_full((GDestroyNotify)free_framedata);
    priv->local->frame_queue = g_async_queue_new_full((GDestroyNotify)free_framedata);
    priv->frame_timeout_source = g_timeout_add(FRAME_RATE_PERIOD, (GSourceFunc)check_frame_queue, self);

    /* handle button event */
    g_signal_connect(GTK_WIDGET(self), "button-press-event", G_CALLBACK(on_button_press_in_screen_event), NULL);

    /* drag & drop files as video sources */
    gtk_drag_dest_set(GTK_WIDGET(self), GTK_DEST_DEFAULT_ALL, NULL, 0, (GdkDragAction)(GDK_ACTION_COPY | GDK_ACTION_PRIVATE));
    gtk_drag_dest_add_uri_targets(GTK_WIDGET(self));
    g_signal_connect(GTK_WIDGET(self), "drag-data-received", G_CALLBACK(on_drag_data_received), NULL);
}

/*
 * on_drag_data_received()
 *
 * Handle dragged data in the video widget window.
 * Dropping an image causes the client to switch the video input to that image.
 */
static void
on_drag_data_received(G_GNUC_UNUSED GtkWidget *self,
                      G_GNUC_UNUSED GdkDragContext *context,
                      G_GNUC_UNUSED gint x,
                      G_GNUC_UNUSED gint y,
                      GtkSelectionData *selection_data,
                      G_GNUC_UNUSED guint info,
                      G_GNUC_UNUSED guint32 time,
                      G_GNUC_UNUSED gpointer data)
{
    gchar **uris = gtk_selection_data_get_uris(selection_data);

    /* only play the first selection */
    if (uris && *uris)
        Video::SourceModel::instance()->setFile(QUrl(*uris));

    g_strfreev(uris);
}

static void
switch_video_input(G_GNUC_UNUSED GtkWidget *widget, Video::Device *device)
{
    Video::SourceModel::instance()->switchTo(device);
}

static void
switch_video_input_screen(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer user_data)
{
    unsigned x, y;
    unsigned width, height;

    /* try to get the dispaly or default to 0 */
    QString display_env{getenv("DISPLAY")};
    int display = 0;

    if (!display_env.isEmpty()) {
        auto list = display_env.split(":", QString::SkipEmptyParts);
        /* should only be one display, so get the first one */
        if (list.size() > 0) {
            display = list.at(0).toInt();
            g_debug("sharing screen from DISPLAY %d", display);
        }
    }

    x = y = width = height = 0;

    xrectsel(&x, &y, &width, &height);

    if (!width || !height) {
        x = y = 0;
        width = gdk_screen_width();
        height = gdk_screen_height();
    }

    Video::SourceModel::instance()->setDisplay(display, QRect(x,y,width,height));
}

/*
 * on_button_press_in_screen_event()
 *
 * Handle button event in the video screen.
 */
static gboolean
on_button_press_in_screen_event(G_GNUC_UNUSED GtkWidget *widget,
                                GdkEventButton *event,
                                G_GNUC_UNUSED gpointer data)
{
    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    /* create menu with available video sources */
    GtkWidget *menu = gtk_menu_new();

    /* list available devices and check off the active device */
    auto device_list = Video::DeviceModel::instance()->devices();

    for( auto device: device_list) {
        GtkWidget *item = gtk_check_menu_item_new_with_mnemonic(device->name().toLocal8Bit().constData());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), device->isActive());
        g_signal_connect(item, "activate", G_CALLBACK(switch_video_input), device);
    }

    /* add screen area as an input */
    GtkWidget *item = gtk_check_menu_item_new_with_mnemonic("Share screen area");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(item, "activate", G_CALLBACK(switch_video_input_screen), NULL);

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return TRUE; /* event has been fully handled */
}

static FrameInfo *
prepare_framedata(Video::Renderer *renderer, ClutterActor* image_actor)
{
    const QByteArray& data = renderer->currentFrame();
    QSize res = renderer->size();

    /* copy frame data */
    gpointer frame_data = g_memdup((gconstpointer)data.constData(), data.size());

    FrameInfo *frame = g_new0(FrameInfo, 1);

    frame->image_actor = image_actor;
    frame->data = (guchar *)frame_data;
    frame->data_size = data.size();
    frame->width = res.width();
    frame->height = res.height();

    return frame;
}

static void
free_framedata(gpointer data)
{
    if (data == NULL) return;
    FrameInfo *frame = (FrameInfo *)data;
    g_free(frame->data);
    g_free(frame);
}

static void
clutter_render_image(FrameInfo *frame)
{
    if (frame == NULL) return;
    g_return_if_fail(CLUTTER_IS_ACTOR(frame->image_actor));

    ClutterContent * image_new = clutter_image_new();

    const gint BPP = 4;
    const gint ROW_STRIDE = BPP * frame->width;

    GError *error = NULL;
    clutter_image_set_data(
            CLUTTER_IMAGE(image_new),
            frame->data,
            COGL_PIXEL_FORMAT_BGRA_8888,
            frame->width,
            frame->height,
            ROW_STRIDE,
            &error);
    if (error) {
        g_warning("error rendering image to clutter: %s", error->message);
        g_error_free(error);
    }

    clutter_actor_set_content(frame->image_actor, image_new);
    g_object_unref (image_new);
    /* note: we must set the content gravity be "resize aspect" after setting the image data to make sure
     * that the aspect ratio is correct
     */
    clutter_actor_set_content_gravity(frame->image_actor, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
}

static gboolean
check_frame_queue(VideoWidget *self)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* get the latest frame in the queue */
    gpointer local_data_last = NULL;
    gpointer local_data_next = g_async_queue_try_pop(priv->local->frame_queue);
    while(local_data_next != NULL) {
        // if (local_data_last != NULL) g_debug("skipping local frame");
        /* make sure to free the frame we're skipping */
        free_framedata(local_data_last);
        local_data_last = local_data_next;
        local_data_next = g_async_queue_try_pop(priv->local->frame_queue);
    }

    /* display the frame */
    clutter_render_image((FrameInfo *)local_data_last);
    free_framedata(local_data_last);

    /* get the latest frame in the queue */
    gpointer remote_data_last = NULL;
    gpointer remote_data_next = g_async_queue_try_pop(priv->remote->frame_queue);
    while(remote_data_next != NULL) {
        // if (remote_data_last != NULL) g_debug("skipping remote frame");
        /* make sure to free the frame we're skipping */
        free_framedata(remote_data_last);
        remote_data_last = remote_data_next;
        remote_data_next = g_async_queue_try_pop(priv->remote->frame_queue);
    }

    /* display the frame */
    clutter_render_image((FrameInfo *)remote_data_last);
    free_framedata(remote_data_last);

    return TRUE; /* keep going */
}

static void
renderer_stop(VideoWidgetRenderer *renderer)
{
    g_return_if_fail(CLUTTER_IS_ACTOR(renderer->actor));
    QObject::disconnect(renderer->frame_update);
}

static void
renderer_start(VideoWidgetRenderer *renderer)
{
    g_return_if_fail(CLUTTER_IS_ACTOR(renderer->actor));
    QObject::disconnect(renderer->frame_update);
    renderer->frame_update = QObject::connect(
        renderer->renderer,
        &Video::Renderer::frameUpdated,
        [=]() {
            if (!renderer->renderer->isRendering()) {
                g_debug("got frame but not rendering");
                return;
            }

            /* this callback comes from another thread;
             * rendering must be done in the main loop;
             * copy the frame and add it to the frame queue
             */
            FrameInfo *frame = prepare_framedata(renderer->renderer,
                                                 renderer->actor);
            g_async_queue_push(renderer->frame_queue, frame);
        }
    );
}

static void
video_widget_set_renderer(VideoWidgetRenderer *renderer)
{
    if (renderer == NULL) return;

    /* reset the content gravity so that the aspect ratio gets properly set if it chagnes */
    clutter_actor_set_content_gravity(renderer->actor, CLUTTER_CONTENT_GRAVITY_RESIZE_FILL);

    /* update the renderer */
    QObject::disconnect(renderer->frame_update);
    QObject::disconnect(renderer->render_stop);
    QObject::disconnect(renderer->render_start);

    if (renderer->renderer->isRendering())
        renderer_start(renderer);

    renderer->render_stop = QObject::connect(
        renderer->renderer,
        &Video::Renderer::stopped,
        [=]() {
            renderer_stop(renderer);
        }
    );

    renderer->render_start = QObject::connect(
        renderer->renderer,
        &Video::Renderer::started,
        [=]() {
            renderer_start(renderer);
        }
    );
}

/*
 * video_widget_new()
 *
 * The function use to create a new video_widget
 */
GtkWidget*
video_widget_new(void)
{
    GtkWidget *self = (GtkWidget *)g_object_new(VIDEO_WIDGET_TYPE, NULL);
    return self;
}

void
video_widget_add_renderer(VideoWidget *self, const VideoRenderer *new_renderer)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));
    if (new_renderer == NULL || new_renderer->renderer == NULL)
        return;

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* update the renderer */
    switch(new_renderer->type) {
        case VIDEO_RENDERER_REMOTE:
            priv->remote->renderer = new_renderer->renderer;
            video_widget_set_renderer(priv->remote);
            break;
        case VIDEO_RENDERER_LOCAL:
            priv->local->renderer = new_renderer->renderer;
            video_widget_set_renderer(priv->local);
            break;
        case VIDEO_RENDERER_COUNT:
            break;
    }
}
