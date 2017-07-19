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

#ifndef _RINGMAINWINDOW_H
#define _RINGMAINWINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RING_MAIN_WINDOW_TYPE (ring_main_window_get_type ())
#define RING_MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindow))
#define RING_MAIN_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), RING_MAIN_WINDOW_TYPE, RingMainWindowClass))
#define IS_RING_MAIN_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_MAIN_WINDOW_TYPE))
#define IS_RING_MAIN_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RING_MAIN_WINDOW_TYPE))


typedef struct _RingMainWindow      RingMainWindow;
typedef struct _RingMainWindowClass RingMainWindowClass;


GType      ring_main_window_get_type (void) G_GNUC_CONST;
GtkWidget *ring_main_window_new      (GtkApplication *app);

G_END_DECLS

#endif /* _RINGMAINWINDOW_H */
