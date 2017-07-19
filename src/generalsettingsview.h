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

#ifndef _GENERALSETTINGSVIEW_H
#define _GENERALSETTINGSVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GENERAL_SETTINGS_VIEW_TYPE            (general_settings_view_get_type ())
#define GENERAL_SETTINGS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GENERAL_SETTINGS_VIEW_TYPE, GeneralSettingsView))
#define GENERAL_SETTINGS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GENERAL_SETTINGS_VIEW_TYPE, GeneralSettingsViewClass))
#define IS_GENERAL_SETTINGS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GENERAL_SETTINGS_VIEW_TYPE))
#define IS_GENERAL_SETTINGS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GENERAL_SETTINGS_VIEW_TYPE))

typedef struct _GeneralSettingsView      GeneralSettingsView;
typedef struct _GeneralSettingsViewClass GeneralSettingsViewClass;

GType      general_settings_view_get_type      (void) G_GNUC_CONST;
GtkWidget *general_settings_view_new           (void);
void       general_settings_view_show_profile  (GeneralSettingsView *self, gboolean show_profile);

G_END_DECLS

#endif /* _GENERALSETTINGSVIEW_H */
