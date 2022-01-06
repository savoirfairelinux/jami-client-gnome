/*
 *  Copyright (C) 2015-2022 Savoir-faire Linux Inc.
 *  Author: Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#ifndef _PLUGINSETTINGSVIEW_H
#define _PLUGINSETTINGSVIEW_H

#include <gtk/gtk.h>

namespace lrc
{
namespace api
{
    class PluginModel;
}
}

G_BEGIN_DECLS

#define PLUGIN_SETTINGS_VIEW_TYPE            (plugin_settings_view_get_type ())
#define PLUGIN_SETTINGS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PLUGIN_SETTINGS_VIEW_TYPE, PluginSettingsView))
#define PLUGIN_SETTINGS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PLUGIN_SETTINGS_VIEW_TYPE, PluginSettingsViewClass))
#define IS_PLUGIN_SETTINGS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PLUGIN_SETTINGS_VIEW_TYPE))
#define IS_PLUGIN_SETTINGS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PLUGIN_SETTINGS_VIEW_TYPE))

typedef struct _PluginSettingsView      PluginSettingsView;
typedef struct _PluginSettingsViewClass PluginSettingsViewClass;

GType      plugin_settings_view_get_type      (void) G_GNUC_CONST;
GtkWidget *plugin_settings_view_new           (lrc::api::PluginModel& pluginModel);
void       plugin_settings_view_show          (PluginSettingsView *view, gboolean show_preview);

G_END_DECLS

#endif /* _PLUGINSETTINGSVIEW_H */
