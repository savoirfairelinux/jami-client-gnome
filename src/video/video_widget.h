/*
 *  Copyright (C) 2015-2022 Savoir-faire Linux Inc.
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

#ifndef __VIDEO_WIDGET_H__
#define __VIDEO_WIDGET_H__

#include <gtk/gtk.h>
#include <video/renderer.h>
#include <api/newvideo.h>
#include <QJsonObject>

#include "../accountinfopointer.h"

namespace lrc
{
namespace api
{
    class AVModel;
}
}

G_BEGIN_DECLS

#define VIDEO_WIDGET_TYPE              (video_widget_get_type())
#define VIDEO_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_WIDGET_TYPE, VideoWidget))
#define VIDEO_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_WIDGET_TYPE, VideoWidgetClass))
#define IS_VIDEO_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_WIDGET_TYPE))
#define IS_VIDEO_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_WIDGET_TYPE))

typedef struct _VideoWidgetClass VideoWidgetClass;
typedef struct _VideoWidget VideoWidget;

typedef enum {
    VIDEO_RENDERER_REMOTE,
    VIDEO_RENDERER_LOCAL,
    VIDEO_RENDERER_COUNT
} VideoRendererType;

/* Public interface */
GType           video_widget_get_type          (void) G_GNUC_CONST;
GtkWidget*      video_widget_new               (void);
void            video_widget_add_new_renderer (VideoWidget*, lrc::api::AVModel* avModel, const lrc::api::video::Renderer*, VideoRendererType);
const lrc::api::video::Renderer* video_widget_get_renderer (VideoWidget*, VideoRendererType);
void            video_widget_on_drag_data_received (GtkWidget *self,
                                                    GdkDragContext *context,
                                                    gint x,
                                                    gint y,
                                                    GtkSelectionData *selection_data,
                                                    guint info,
                                                    guint32 time,
                                                    gpointer data);
gboolean        video_widget_on_button_press_in_screen_event (VideoWidget *self,
                                                              GdkEventButton *event,
                                                              G_GNUC_UNUSED gpointer);
void            video_widget_take_snapshot (VideoWidget *self);
GdkPixbuf*      video_widget_get_snapshot  (VideoWidget *self);
void            video_widget_set_preview_visible (VideoWidget *self, bool show);
void            video_widget_add_participant_hover(VideoWidget *self, const QJsonObject& participant);
void            video_widget_set_call_info(VideoWidget *self, AccountInfoPointer const & accountInfo, const QString& callId);
void            video_widget_remove_hovers(VideoWidget *self);
void            video_widget_on_event(VideoWidget *self, GdkEvent* event);

G_END_DECLS

#endif /* __VIDEO_WIDGET_H__ */
