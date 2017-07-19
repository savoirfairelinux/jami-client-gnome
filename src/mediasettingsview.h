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

#ifndef _MEDIASETTINGSVIEW_H
#define _MEDIASETTINGSVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MEDIA_SETTINGS_VIEW_TYPE            (media_settings_view_get_type ())
#define MEDIA_SETTINGS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MEDIA_SETTINGS_VIEW_TYPE, MediaSettingsView))
#define MEDIA_SETTINGS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), MEDIA_SETTINGS_VIEW_TYPE, MediaSettingsViewClass))
#define IS_MEDIA_SETTINGS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), MEDIA_SETTINGS_VIEW_TYPE))
#define IS_MEDIA_SETTINGS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MEDIA_SETTINGS_VIEW_TYPE))

typedef struct _MediaSettingsView      MediaSettingsView;
typedef struct _MediaSettingsViewClass MediaSettingsViewClass;

GType      media_settings_view_get_type      (void) G_GNUC_CONST;
GtkWidget *media_settings_view_new           (void);
void       media_settings_view_show_preview  (MediaSettingsView *self, gboolean show_preview);

G_END_DECLS

#endif /* _MEDIASETTINGSVIEW_H */
