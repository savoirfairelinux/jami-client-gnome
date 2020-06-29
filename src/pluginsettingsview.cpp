/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

#include "pluginsettingsview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string>

// LRC
#include <api/pluginmodel.h>

namespace { namespace details
{
class CppImpl;
}}

enum
{
  PROP_MAIN_WIN_PNT = 1,
};

struct _PluginSettingsView
{
    GtkScrolledWindow parent;
};

struct _PluginSettingsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _PluginSettingsViewPrivate PluginSettingsViewPrivate;

struct _PluginSettingsViewPrivate
{
    GtkWidget *vbox_main;

    /* plugins enabled settings */
    GtkWidget *plugins_enabled_button;
    GtkWidget *button_choose_jpl_file;
    // GtkWidget *plugin_loaded_button;

    /* main window pointer */
    GtkWidget* main_window_pnt;

    details::CppImpl* cpp; ///< Non-UI and C++ only code
};

G_DEFINE_TYPE_WITH_PRIVATE(PluginSettingsView, plugin_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define PLUGIN_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), PLUGIN_SETTINGS_VIEW_TYPE, PluginSettingsViewPrivate))

namespace { namespace details
{

class CppImpl
{
public:
    explicit CppImpl(PluginSettingsView& widget, lrc::api::PluginModel& pluginModel);

    lrc::api::PluginModel* pluginModel_ = nullptr;
    PluginSettingsView* self = nullptr; // The GTK widget itself
    PluginSettingsViewPrivate* widgets = nullptr;
};

CppImpl::CppImpl(PluginSettingsView& widget, lrc::api::PluginModel& pluginModel)
: self {&widget}
, widgets {PLUGIN_SETTINGS_VIEW_GET_PRIVATE(&widget)}
, pluginModel_(&pluginModel)
{
    gtk_switch_set_active(
        GTK_SWITCH(widgets->plugins_enabled_button),
        pluginModel_->getPluginsEnabled());
    
}

}} // namespace details

enum {
    PLUGIN_INSTALL_FILE,
    LAST_SIGNAL
};

static guint general_settings_view_signals[LAST_SIGNAL] = { 0 };

static void
update_plugins_enabled(GtkSwitch *button, GParamSpec* /*spec*/, PluginSettingsView *self)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(self));
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    gboolean newState = gtk_switch_get_active(button);
    if (priv && priv->cpp && priv->cpp->pluginModel_)
        priv->cpp->pluginModel_->setPluginsEnabled(newState);

}

static void
choose_jpl_file(PluginSettingsView *self)
{
    g_return_if_fail(PLUGIN_SETTINGS_VIEW(self));

    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    // gboolean newState = gtk_switch_get_active(priv->plugins_enabled_button);
    // if (true)
    {
        gint res;
        gchar* filename = nullptr;

        if (!priv->main_window_pnt) {
            g_debug("Internal error: NULL main window pointer in PluginSettingsView.");
            return;
        }

        GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Choose Plugin JPL file"),
                                        GTK_WINDOW(priv->main_window_pnt),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        _("_Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("_Save"),
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);

        res = gtk_dialog_run (GTK_DIALOG(dialog));

        if (res == GTK_RESPONSE_ACCEPT) {
            auto chooser = GTK_FILE_CHOOSER(dialog);
            filename = gtk_file_chooser_get_filename(chooser);
        }
        gtk_widget_destroy (dialog);

        if (!filename) return;

        // install plugin
        if (priv && priv->cpp && priv->cpp->pluginModel_)
            priv->cpp->pluginModel_->installPlugin(filename, true);
        
        g_signal_emit(G_OBJECT(self), general_settings_view_signals[PLUGIN_INSTALL_FILE], 0, filename);
    }
}

static void
plugin_settings_view_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    PluginSettingsView *self = PLUGIN_SETTINGS_VIEW (object);
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_MAIN_WIN_PNT) {
        GtkWidget *main_window_pnt = (GtkWidget*) g_value_get_pointer(value);

        if (!main_window_pnt) {
            g_debug("Internal error: NULL main window pointer passed to set_property");
            return;
        }

        priv->main_window_pnt = main_window_pnt;
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
plugin_settings_view_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    PluginSettingsView *self = PLUGIN_SETTINGS_VIEW (object);
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_MAIN_WIN_PNT) {
        g_value_set_pointer(value, priv->main_window_pnt);
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
plugin_settings_view_dispose(GObject *object)
{
    PluginSettingsView *view = PLUGIN_SETTINGS_VIEW(object);
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(view);

    G_OBJECT_CLASS(plugin_settings_view_parent_class)->dispose(object);
}

static void
plugin_settings_view_init(PluginSettingsView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
plugin_settings_view_class_init(PluginSettingsViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = plugin_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/pluginsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, vbox_main);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, plugins_enabled_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, button_choose_jpl_file);
    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, plugin_loaded_button);

    /* Define class properties: e.g. pointer to main window, etc.*/
    object_class->set_property = plugin_settings_view_set_property;
    object_class->get_property = plugin_settings_view_get_property;

    GParamFlags flags = (GParamFlags) (G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_MAIN_WIN_PNT,
        g_param_spec_pointer ("main_window_pnt",
                              "MainWindow pointer",
                              "Pointer to the Main Window. This property is used by modal dialogs.",
                              flags));
}


GtkWidget *
plugin_settings_view_new(GtkWidget* main_window_pnt, lrc::api::PluginModel& pluginModel)
{
    auto self = g_object_new(PLUGIN_SETTINGS_VIEW_TYPE, NULL);
    auto* priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    priv->cpp = new details::CppImpl(
        *(reinterpret_cast<PluginSettingsView*>(self)), pluginModel
    );

    // set_up ring main window pointer (needed by modal dialogs)
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_POINTER);
    g_value_set_pointer (&val, main_window_pnt);
    g_object_set_property (G_OBJECT (self), "main_window_pnt", &val);
    g_value_unset (&val);

    // CppImpl ctor
    g_signal_connect(priv->plugins_enabled_button, "notify::active",
        G_CALLBACK(update_plugins_enabled), self);

    g_signal_connect_swapped(priv->button_choose_jpl_file, "clicked", G_CALLBACK(choose_jpl_file), self);
    
    return (GtkWidget *)self;
}

void
plugin_settings_view_show(PluginSettingsView *self, gboolean show_preview)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(self));
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    
    if(show_preview)
    {
        gtk_widget_show_all(priv->plugins_enabled_button);
        // gtk_widget_show_all(priv->plugin_loaded_button);
    }
}
