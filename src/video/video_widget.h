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

#ifndef __VIDEO_WIDGET_H__
#define __VIDEO_WIDGET_H__

#include <gtk/gtk.h>
#include <video/renderer.h>

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
void            video_widget_push_new_renderer (VideoWidget *, Video::Renderer *, VideoRendererType);
void            video_widget_pause_rendering   (VideoWidget *self, gboolean pause);

void video_widget_on_drag_data_received(G_GNUC_UNUSED GtkWidget *self,
                      G_GNUC_UNUSED GdkDragContext *context,
                      G_GNUC_UNUSED gint x,
                      G_GNUC_UNUSED gint y,
                      GtkSelectionData *selection_data,
                      G_GNUC_UNUSED guint info,
                      G_GNUC_UNUSED guint32 time,
                      gpointer data);

G_END_DECLS

#endif /* __VIDEO_WIDGET_H__ */
