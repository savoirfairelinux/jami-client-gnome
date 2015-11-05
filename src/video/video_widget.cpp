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

#include <glib/gi18n.h>
#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>
#include <video/renderer.h>
#include <video/sourcemodel.h>
#include <video/devicemodel.h>
#include <QtCore/QUrl>
#include "../defines.h"
#include <stdlib.h>
#include <atomic>
#include <mutex>
#include "xrectsel.h"

static constexpr int VIDEO_LOCAL_SIZE            = 150;
static constexpr int VIDEO_LOCAL_OPACITY_DEFAULT = 255; /* out of 255 */

/* check video frame queues at this rate;
 * use 30 ms (about 30 fps) since we don't expect to
 * receive video frames faster than that */
static constexpr int FRAME_RATE_PERIOD           = 30;

struct _VideoWidgetClass {
    GtkClutterEmbedClass parent_class;
};

struct _VideoWidget {
    GtkClutterEmbed parent;
};

typedef struct _VideoWidgetPrivate VideoWidgetPrivate;

typedef struct _VideoWidgetRenderer VideoWidgetRenderer;

struct _VideoWidgetPrivate {
    ClutterActor            *video_container;

    /* remote peer data */
    VideoWidgetRenderer     *remote;

    /* local peer data */
    VideoWidgetRenderer     *local;

    guint                    frame_timeout_source;

    /* new renderers should be put into the queue for processing by a g_timeout
     * function whose id should be saved into renderer_timeout_source;
     * this way when the VideoWidget object is destroyed, we do not try
     * to process any new renderers by stoping the g_timeout function.
     */
    guint                    renderer_timeout_source;
    GAsyncQueue             *new_renderer_queue;
};

struct _VideoWidgetRenderer {
    VideoRendererType        type;
    ClutterActor            *actor;
    ClutterAction           *drag_action;
    Video::Renderer         *renderer;
    std::mutex               run_mutex;
    bool                     running;
    std::atomic_bool         dirty;

    /* show_black_frame is used to request the actor to render a black image;
     * this will take over 'running' and 'dirty', ie: a black frame will be
     * rendered even if the Video::Renderer is not running, or has a frame available.
     * this will be set back to false once the black frame is rendered
     */
    std::atomic_bool         show_black_frame;
    std::atomic_bool         pause_rendering;
    QMetaObject::Connection  frame_update;
    QMetaObject::Connection  render_stop;
    QMetaObject::Connection  render_start;
};

G_DEFINE_TYPE_WITH_PRIVATE(VideoWidget, video_widget, GTK_CLUTTER_TYPE_EMBED);

#define VIDEO_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VIDEO_WIDGET_TYPE, VideoWidgetPrivate))

/* static prototypes */
static gboolean check_frame_queue              (VideoWidget *);
static void     renderer_stop                  (VideoWidgetRenderer *);
static void     renderer_start                 (VideoWidgetRenderer *);
static void     on_drag_data_received          (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint32, gpointer);
static gboolean on_button_press_in_screen_event(GtkWidget *, GdkEventButton *, gpointer);
static gboolean check_renderer_queue           (VideoWidget *);
static void     free_video_widget_renderer     (VideoWidgetRenderer *);
static void     video_widget_add_renderer      (VideoWidget *, VideoWidgetRenderer *);

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

    /* dispose may be called multiple times, make sure
     * not to call g_source_remove more than once */
    if (priv->frame_timeout_source) {
        g_source_remove(priv->frame_timeout_source);
        priv->frame_timeout_source = 0;
    }

    if (priv->renderer_timeout_source) {
        g_source_remove(priv->renderer_timeout_source);
        priv->renderer_timeout_source = 0;
    }

    if (priv->new_renderer_queue) {
        g_async_queue_unref(priv->new_renderer_queue);
        priv->new_renderer_queue = NULL;
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

    free_video_widget_renderer(priv->local);
    free_video_widget_renderer(priv->remote);

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

static void
on_allocation_changed(ClutterActor *video_area, G_GNUC_UNUSED GParamSpec *pspec, VideoWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    auto actor = priv->local->actor;
    auto drag_action = priv->local->drag_action;

    ClutterActorBox actor_box;
    clutter_actor_get_allocation_box(actor, &actor_box);
    gfloat actor_w = clutter_actor_box_get_width(&actor_box);
    gfloat actor_h = clutter_actor_box_get_height(&actor_box);

    ClutterActorBox area_box;
    clutter_actor_get_allocation_box(video_area, &area_box);
    gfloat area_w = clutter_actor_box_get_width(&area_box);
    gfloat area_h = clutter_actor_box_get_height(&area_box);

    /* make sure drag area stays within the bounds of the stage */
    ClutterRect *rect = clutter_rect_init (
        clutter_rect_alloc(),
        0, 0,
        area_w - actor_w,
        area_h - actor_h);
    clutter_drag_action_set_drag_area(CLUTTER_DRAG_ACTION(drag_action), rect);
    clutter_rect_free(rect);
}

static void
on_drag_begin(G_GNUC_UNUSED ClutterDragAction   *action,
                            ClutterActor        *actor,
              G_GNUC_UNUSED gfloat               event_x,
              G_GNUC_UNUSED gfloat               event_y,
              G_GNUC_UNUSED ClutterModifierType  modifiers,
              G_GNUC_UNUSED gpointer             user_data)
{
    /* clear the align constraint when starting to move the preview, otherwise
     * it won't move; save and set its position, to what it was before the
     * constraint was cleared, or else it might jump around */
    gfloat actor_x, actor_y;
    clutter_actor_get_position(actor, &actor_x, &actor_y);
    clutter_actor_clear_constraints(actor);
    clutter_actor_set_position(actor, actor_x, actor_y);
}

static void
on_drag_end(G_GNUC_UNUSED ClutterDragAction   *action,
                          ClutterActor        *actor,
            G_GNUC_UNUSED gfloat               event_x,
            G_GNUC_UNUSED gfloat               event_y,
            G_GNUC_UNUSED ClutterModifierType  modifiers,
                          ClutterActor        *video_area)
{
    ClutterActorBox area_box;
    clutter_actor_get_allocation_box(video_area, &area_box);
    gfloat area_w = clutter_actor_box_get_width(&area_box);
    gfloat area_h = clutter_actor_box_get_height(&area_box);

    gfloat actor_x, actor_y;
    clutter_actor_get_position(actor, &actor_x, &actor_y);
    gfloat actor_w, actor_h;
    clutter_actor_get_size(actor, &actor_w, &actor_h);

    area_w -= actor_w;
    area_h -= actor_h;

    /* add new constraints to make sure the preview stays in about the same location
     * relative to the rest of the video when resizing */
    ClutterConstraint *constraint_x = clutter_align_constraint_new(video_area,
        CLUTTER_ALIGN_X_AXIS, actor_x/area_w);
    clutter_actor_add_constraint(actor, constraint_x);

    ClutterConstraint *constraint_y = clutter_align_constraint_new(video_area,
        CLUTTER_ALIGN_Y_AXIS, actor_y/area_h);
    clutter_actor_add_constraint(actor, constraint_y);
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

    auto stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(self));

    /* layout manager is used to arrange children in space, here we ask clutter
     * to align children to fill the space when resizing the window */
    clutter_actor_set_layout_manager(stage,
        clutter_bin_layout_new(CLUTTER_BIN_ALIGNMENT_FILL, CLUTTER_BIN_ALIGNMENT_FILL));

    /* add a scene container where we can add and remove our actors */
    priv->video_container = clutter_actor_new();
    clutter_actor_set_background_color(priv->video_container, CLUTTER_COLOR_Black);
    clutter_actor_add_child(stage, priv->video_container);

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
    /* set position constraint to right cornder;
     * this constraint will be removed once the user tries to move the position
     * of the action */
    constraint = clutter_align_constraint_new(priv->video_container,
                                              CLUTTER_ALIGN_BOTH, 0.99);
    clutter_actor_add_constraint(priv->local->actor, constraint);
    clutter_actor_set_opacity(priv->local->actor,
                              VIDEO_LOCAL_OPACITY_DEFAULT);

    /* add ability for actor to be moved */
    clutter_actor_set_reactive(priv->local->actor, TRUE);
    priv->local->drag_action = clutter_drag_action_new();
    clutter_actor_add_action(priv->local->actor, priv->local->drag_action);

    g_signal_connect(priv->local->drag_action, "drag-begin", G_CALLBACK(on_drag_begin), NULL);
    g_signal_connect_after(priv->local->drag_action, "drag-end", G_CALLBACK(on_drag_end), stage);

    /* make sure the actor stays within the bounds of the stage */
    g_signal_connect(stage, "notify::allocation", G_CALLBACK(on_allocation_changed), self);

    /* Init the timeout source which will check the for new frames.
     * The priority must be lower than GTK drawing events
     * (G_PRIORITY_HIGH_IDLE + 20) so that this timeout source doesn't choke
     * the main loop on slower machines.
     */
    priv->frame_timeout_source = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                    FRAME_RATE_PERIOD,
                                                    (GSourceFunc)check_frame_queue,
                                                    self,
                                                    NULL);

    /* init new renderer queue */
    priv->new_renderer_queue = g_async_queue_new_full((GDestroyNotify)free_video_widget_renderer);
    /* check new render every 30 ms (30ms is "fast enough");
     * we don't use an idle function so it doesn't consume cpu needlessly */
    priv->renderer_timeout_source= g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
                                                      30,
                                                      (GSourceFunc)check_renderer_queue,
                                                      self,
                                                      NULL);

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
        Video::SourceModel::instance().setFile(QUrl(*uris));

    g_strfreev(uris);
}

static void
switch_video_input(G_GNUC_UNUSED GtkWidget *widget, Video::Device *device)
{
    Video::SourceModel::instance().switchTo(device);
}

static void
switch_video_input_screen(G_GNUC_UNUSED GtkWidget *item, G_GNUC_UNUSED gpointer user_data)
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

    Video::SourceModel::instance().setDisplay(display, QRect(x,y,width,height));
}

static void
switch_video_input_file(G_GNUC_UNUSED GtkWidget *item, GtkWidget *parent)
{
    if (parent && GTK_IS_WIDGET(parent)) {
        /* get parent window */
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
    }

    gchar *uri = NULL;
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Choose File",
            GTK_WINDOW(parent),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Open", GTK_RESPONSE_ACCEPT,
            NULL);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        Video::SourceModel::instance().setFile(QUrl(uri));
    }

    gtk_widget_destroy(dialog);

    g_free(uri);
}

/*
 * on_button_press_in_screen_event()
 *
 * Handle button event in the video screen.
 */
static gboolean
on_button_press_in_screen_event(GtkWidget *parent,
                                GdkEventButton *event,
                                G_GNUC_UNUSED gpointer data)
{
    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    /* create menu with available video sources */
    GtkWidget *menu = gtk_menu_new();

    auto active = Video::SourceModel::instance().activeIndex();

    /* list available devices and check off the active device */
    auto device_list = Video::DeviceModel::instance().devices();

    for( auto device: device_list) {
        GtkWidget *item = gtk_check_menu_item_new_with_mnemonic(device->name().toLocal8Bit().constData());
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        auto device_idx =  Video::SourceModel::instance().getDeviceIndex(device);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), device_idx == active);
        g_signal_connect(item, "activate", G_CALLBACK(switch_video_input), device);
    }

    /* add separator */
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    /* add screen area as an input */
    GtkWidget *item = gtk_check_menu_item_new_with_mnemonic(_("Share screen area"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), Video::SourceModel::ExtendedDeviceList::SCREEN == active);
    g_signal_connect(item, "activate", G_CALLBACK(switch_video_input_screen), NULL);

    /* add file as an input */
    item = gtk_check_menu_item_new_with_mnemonic(_("Share file"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), Video::SourceModel::ExtendedDeviceList::FILE == active);
    g_signal_connect(item, "activate", G_CALLBACK(switch_video_input_file), parent);

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return TRUE; /* event has been fully handled */
}

static void
clutter_render_image(VideoWidgetRenderer* wg_renderer)
{
    auto actor = wg_renderer->actor;
    g_return_if_fail(CLUTTER_IS_ACTOR(actor));

    if (wg_renderer->pause_rendering)
        return;

    if (wg_renderer->show_black_frame) {
        /* render a black frame set the bool back to false, this is likely done
         * when the renderer is stopped so we ignore whether or not it is running
         */
        if (auto image_old = clutter_actor_get_content(actor)) {
            gfloat width;
            gfloat height;
            if (clutter_content_get_preferred_size(image_old, &width, &height)) {
                /* NOTE: this is a workaround for #72531, a crash which occurs
                 * in cogl < 1.18. We allocate a black frame of the same size
                 * as the previous image, instead of simply setting an empty or
                 * a NULL ClutterImage.
                 */
                auto image_empty = clutter_image_new();
                if (auto empty_data = (guint8 *)g_try_malloc0((gsize)width * height * 4)) {
                    GError* error = NULL;
                    clutter_image_set_data(
                            CLUTTER_IMAGE(image_empty),
                            empty_data,
                            COGL_PIXEL_FORMAT_BGRA_8888,
                            (guint)width,
                            (guint)height,
                            (guint)width*4,
                            &error);
                    if (error) {
                        g_warning("error rendering empty image to clutter: %s", error->message);
                        g_clear_error(&error);
                        g_object_unref(image_empty);
                        return;
                    }
                    clutter_actor_set_content(actor, image_empty);
                    g_object_unref(image_empty);
                    g_free(empty_data);
                } else {
                    clutter_actor_set_content(actor, NULL);
                }
            } else {
                clutter_actor_set_content(actor, NULL);
            }
        }
        wg_renderer->show_black_frame = false;
        return;
    }

    ClutterContent *image_new = nullptr;

    {
        /* the following must be done under lock in case a 'stopped' signal is
         * received during rendering; otherwise the mem could become invalid */
        std::lock_guard<std::mutex> lock(wg_renderer->run_mutex);

        if (!wg_renderer->running)
            return;

        auto renderer = wg_renderer->renderer;
        if (renderer == nullptr)
            return;

        auto frame_ptr = renderer->currentFrame();
        auto frame_data = frame_ptr.ptr;
        if (!frame_data or !wg_renderer->dirty)
            return;

        wg_renderer->dirty = false;

        image_new = clutter_image_new();
        g_return_if_fail(image_new);

        const auto& res = renderer->size();
        const gint BPP = 4;
        const gint ROW_STRIDE = BPP * res.width();

        GError *error = nullptr;
        clutter_image_set_data(
            CLUTTER_IMAGE(image_new),
            frame_data,
            COGL_PIXEL_FORMAT_BGRA_8888,
            res.width(),
            res.height(),
            ROW_STRIDE,
            &error);
        if (error) {
            g_warning("error rendering image to clutter: %s", error->message);
            g_clear_error(&error);
            g_object_unref (image_new);
            return;
        }
    }

    clutter_actor_set_content(actor, image_new);
    g_object_unref (image_new);

    /* note: we must set the content gravity be "resize aspect" after setting the image data to make sure
     * that the aspect ratio is correct
     */
    clutter_actor_set_content_gravity(actor, CLUTTER_CONTENT_GRAVITY_RESIZE_ASPECT);
}

static gboolean
check_frame_queue(VideoWidget *self)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* display renderer's frames */
    clutter_render_image(priv->local);
    clutter_render_image(priv->remote);

    return TRUE; /* keep going */
}

static void
renderer_stop(VideoWidgetRenderer *renderer)
{
    QObject::disconnect(renderer->frame_update);
    {
        /* must do this under lock, in case the rendering is taking place when
         * this signal is received */
        std::lock_guard<std::mutex> lock(renderer->run_mutex);
        renderer->running = false;
    }
    /* ask to show a black frame */
    renderer->show_black_frame = true;
}

static void
renderer_start(VideoWidgetRenderer *renderer)
{
    QObject::disconnect(renderer->frame_update);
    {
        std::lock_guard<std::mutex> lock(renderer->run_mutex);
        renderer->running = true;
    }
    renderer->frame_update = QObject::connect(
        renderer->renderer,
        &Video::Renderer::frameUpdated,
        [renderer]() {
            // WARNING: this lambda is called in LRC renderer thread,
            // but check_frame_queue() is in mainloop!

            /* make sure show_black_frame is false since it will take priority
             * over a new frame from the Video::Renderer */
            renderer->show_black_frame = false;
            renderer->dirty = true;
        }
        );
}

static void
free_video_widget_renderer(VideoWidgetRenderer *renderer)
{
    QObject::disconnect(renderer->frame_update);
    QObject::disconnect(renderer->render_stop);
    QObject::disconnect(renderer->render_start);
    g_free(renderer);
}

static void
video_widget_add_renderer(VideoWidget *self, VideoWidgetRenderer *new_video_renderer)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));
    g_return_if_fail(new_video_renderer);
    g_return_if_fail(new_video_renderer->renderer);

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* update the renderer */
    switch(new_video_renderer->type) {
        case VIDEO_RENDERER_REMOTE:
            /* swap the remote renderer */
            new_video_renderer->actor = priv->remote->actor;
            free_video_widget_renderer(priv->remote);
            priv->remote = new_video_renderer;
            /* reset the content gravity so that the aspect ratio gets properly
             * reset if it chagnes */
            clutter_actor_set_content_gravity(priv->remote->actor,
                                              CLUTTER_CONTENT_GRAVITY_RESIZE_FILL);
            break;
        case VIDEO_RENDERER_LOCAL:
            /* swap the remote renderer */
            new_video_renderer->actor = priv->local->actor;
            new_video_renderer->drag_action = priv->local->drag_action;
            free_video_widget_renderer(priv->local);
            priv->local = new_video_renderer;
            /* reset the content gravity so that the aspect ratio gets properly
             * reset if it chagnes */
            clutter_actor_set_content_gravity(priv->local->actor,
                                              CLUTTER_CONTENT_GRAVITY_RESIZE_FILL);
            break;
        case VIDEO_RENDERER_COUNT:
            break;
    }
}

static gboolean
check_renderer_queue(VideoWidget *self)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), G_SOURCE_REMOVE);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* get all the renderers in the queue */
    VideoWidgetRenderer *new_video_renderer = (VideoWidgetRenderer *)g_async_queue_try_pop(priv->new_renderer_queue);
    while (new_video_renderer) {
        video_widget_add_renderer(self, new_video_renderer);
        new_video_renderer = (VideoWidgetRenderer *)g_async_queue_try_pop(priv->new_renderer_queue);
    }

    return G_SOURCE_CONTINUE;
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

/**
 * video_widget_push_new_renderer()
 *
 * This function is used add a new Video::Renderer to the VideoWidget in a
 * thread-safe manner.
 */
void
video_widget_push_new_renderer(VideoWidget *self, Video::Renderer *renderer, VideoRendererType type)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* if the renderer is nullptr, there is nothing to be done */
    if (!renderer) return;

    VideoWidgetRenderer *new_video_renderer = g_new0(VideoWidgetRenderer, 1);
    new_video_renderer->renderer = renderer;
    new_video_renderer->type = type;

    if (new_video_renderer->renderer->isRendering())
        renderer_start(new_video_renderer);

    new_video_renderer->render_stop = QObject::connect(
        new_video_renderer->renderer,
        &Video::Renderer::stopped,
        [=]() {
            renderer_stop(new_video_renderer);
        }
    );

    new_video_renderer->render_start = QObject::connect(
        new_video_renderer->renderer,
        &Video::Renderer::started,
        [=]() {
            renderer_start(new_video_renderer);
        }
    );

    g_async_queue_push(priv->new_renderer_queue, new_video_renderer);
}

void
video_widget_pause_rendering(VideoWidget *self, gboolean pause)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    priv->local->pause_rendering = pause;
    priv->remote->pause_rendering = pause;
}
