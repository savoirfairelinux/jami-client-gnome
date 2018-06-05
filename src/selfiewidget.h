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

#ifndef _SELFIEWIDGET_H
#define _SELFIEWIDGET_H

// GTK
#include <gtk/gtk.h>

// std
#include <string>

G_BEGIN_DECLS

#define SELFIE_WIDGET_TYPE            (selfie_widget_get_type ())
#define SELFIE_WIDGET(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SELFIE_WIDGET_TYPE, SelfieWidget))
#define SELFIE_WIDGET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SELFIE_WIDGET_TYPE, SelfieWidget))
#define IS_SELFIE_WIDGET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SELFIE_WIDGET_TYPE))
#define IS_SELFIE_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SELFIE_WIDGET_TYPE))

typedef struct _SelfieWidget      SelfieWidget;
typedef struct _SelfieWidgetClass SelfieWidgetClass;

GType       selfie_widget_get_type      (void) G_GNUC_CONST;
GtkWidget   *selfie_widget_new          (void);
const char  *selfie_widget_get_filename (SelfieWidget *self);
void        selfie_widget_set_peer_name (SelfieWidget *self, std::string name);

G_END_DECLS

#endif /* _SELFIEWIDGET_H */
