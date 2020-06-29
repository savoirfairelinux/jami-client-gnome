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
    GtkWidget *vbox_plugins_list;

    /* plugins enabled settings */
    GtkWidget *plugins_enabled_button;

    /*plugins List and installation */
    GtkWidget *button_choose_jpl_file;

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
    auto active =  pluginModel_->getPluginsEnabled();
    gtk_switch_set_active(
        GTK_SWITCH(widgets->plugins_enabled_button), active);
    
    gtk_file_chooser_set_action(
        GTK_FILE_CHOOSER(widgets->button_choose_jpl_file),
        GTK_FILE_CHOOSER_ACTION_OPEN);
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

    if(newState)
    {
        gtk_widget_show_all(priv->vbox_plugins_list);
    }
    else
    {
        gtk_widget_hide(priv->vbox_plugins_list);
    }
}

static void
choose_jpl_file(PluginSettingsView *self)
{
    g_return_if_fail(PLUGIN_SETTINGS_VIEW(self));

    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    if (priv->cpp->pluginModel_->getPluginsEnabled())
    {
        gint res;
        gchar* filename = nullptr;

        GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Choose Plugin install file"),
                                        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self))),
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, vbox_plugins_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, plugins_enabled_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, button_choose_jpl_file);
}


GtkWidget *
plugin_settings_view_new(lrc::api::PluginModel& pluginModel)
{
    auto self = g_object_new(PLUGIN_SETTINGS_VIEW_TYPE, NULL);
    auto* priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    priv->cpp = new details::CppImpl(
        *(reinterpret_cast<PluginSettingsView*>(self)), pluginModel
    );

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
        gtk_widget_show_all(priv->button_choose_jpl_file);
    }
    if(priv->cpp->pluginModel_->getPluginsEnabled())
    {
        gtk_widget_show_all(priv->vbox_plugins_list);
    }
}
