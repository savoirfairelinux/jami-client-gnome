/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
 *   Author : Aline Gondim Santos <aline.gondimsantos@savoirfairelinux.com>
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

#define PLUGIN_ICON_SIZE 25

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
    // GtkWidget *vbox_main;
    GtkWidget *vbox_plugins_list;

    /* plugins enabled settings */
    GtkWidget *plugins_enabled_button;

    /*plugins List and installation */
    GtkWidget *button_choose_jpl_file;
    GtkWidget *list_installed_plugins;

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
}

}} // namespace details

static void
update_plugins_enabled(GtkSwitch *button, GParamSpec* /*spec*/, PluginSettingsView *self)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(self));
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);

    gboolean newState = gtk_switch_get_active(button);
    if (priv && priv->cpp && priv->cpp->pluginModel_)
        priv->cpp->pluginModel_->setPluginsEnabled(newState);

    if(newState) {
        gtk_widget_show_all(priv->vbox_plugins_list);
    }
    else {
        gtk_widget_hide(priv->vbox_plugins_list);
    }
}

static void
refreshPluginsList(PluginSettingsView *self);

static void
choose_jpl_file(PluginSettingsView *self)
{
    g_return_if_fail(PLUGIN_SETTINGS_VIEW(self));

    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);
    if (priv->cpp->pluginModel_->getPluginsEnabled())
    {
        gint res;
        gchar* filename = nullptr;

#if GTK_CHECK_VERSION(3,20,0)
        GtkFileChooserNative *native = gtk_file_chooser_native_new(
            _("Choose plugin"),
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self))),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            _("_Open"),
            _("_Cancel"));

        res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
        if (res == GTK_RESPONSE_ACCEPT) {
            filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
        }

        g_object_unref(native);
#else
        GtkWidget *dialog = gtk_file_chooser_dialog_new(
            _("Choose plugin"),
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self))),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            _("_Cancel"), GTK_RESPONSE_CANCEL,
            _("_Open"), GTK_RESPONSE_ACCEPT,
            NULL);

        res = gtk_dialog_run(GTK_DIALOG(dialog));
        if (res == GTK_RESPONSE_ACCEPT) {
            filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        }

        gtk_widget_destroy(dialog);
#endif

        if (!filename) return;

        // install and load plugin
        if (priv && priv->cpp && priv->cpp->pluginModel_)
            priv->cpp->pluginModel_->installPlugin(filename, true);
    }

    refreshPluginsList(self);
}

static GList*
get_row_iterator(GtkWidget* row)
{
    auto box = gtk_bin_get_child(GTK_BIN(row));
    return gtk_container_get_children(GTK_CONTAINER(box));
}

static GtkWidget*
get_uninstall_button_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto current_child = g_list_last(list_iterator);  // button
    return GTK_WIDGET(current_child->data);
}

static GtkWidget*
get_toggle_button_from_row(GtkWidget* row)
{
    auto* list_iterator = get_row_iterator(row);
    auto current_child = g_list_last(list_iterator)->prev;  // button
    return GTK_WIDGET(current_child->data);
}

static void
uninstall_plugin(GtkButton* button, PluginSettingsView *view)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(view));
    auto* priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(view);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_installed_plugins), row))) {
        if (GTK_BUTTON(get_uninstall_button_from_row(children)) == button) {
            auto plugin = gtk_widget_get_name(GTK_WIDGET(button));
            auto* top_window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view)));
            auto* password_dialog = gtk_message_dialog_new(top_window,
                GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                _("Warning! This action will uninstall the plugin!\nNote: this action cannot be undone."));
            gtk_window_set_title(GTK_WINDOW(password_dialog), _("Uninstall Plugin"));
            gtk_dialog_set_default_response(GTK_DIALOG(password_dialog), GTK_RESPONSE_OK);

            auto res = gtk_dialog_run(GTK_DIALOG(password_dialog));
            if (res == GTK_RESPONSE_OK)
                priv->cpp->pluginModel_->uninstallPlugin(plugin);

            gtk_widget_destroy(password_dialog);
        }
        ++row;
    }

    refreshPluginsList(view);
}

static void
toggle_plugin(GtkToggleButton* checkButton, PluginSettingsView *view)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(view));
    auto* priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(view);

    auto row = 0;
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_installed_plugins), row))) {
        if (GTK_TOGGLE_BUTTON(get_toggle_button_from_row(children)) == checkButton) {
            auto plugin = gtk_widget_get_name(GTK_WIDGET(checkButton));
            gboolean newState = gtk_toggle_button_get_active(checkButton);

            if(newState) {
                priv->cpp->pluginModel_->loadPlugin(plugin);
            }
            else {
                priv->cpp->pluginModel_->unloadPlugin(plugin);
            }
        }
        ++row;
    }
}

static void
add_plugin(PluginSettingsView *view, const QString plugin)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(view));
    auto* priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(view);

    auto details = priv->cpp->pluginModel_->getPluginDetails(plugin);

    auto* plugin_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_can_focus(plugin_box, false);
    gtk_box_set_spacing(GTK_BOX(plugin_box), 10);
    GdkPixbuf* pluginIcon = gdk_pixbuf_new_from_file_at_size((details.iconPath).toStdString().c_str(), PLUGIN_ICON_SIZE, PLUGIN_ICON_SIZE, NULL);
    auto* aux = gtk_image_new_from_pixbuf(pluginIcon);
    gtk_box_pack_start(GTK_BOX(plugin_box), GTK_WIDGET(aux), false, false, 0);

    GtkStyleContext* context_box;
    context_box = gtk_widget_get_style_context(GTK_WIDGET(plugin_box));
    gtk_style_context_add_class(context_box, "boxitem");
    // Fill with plugin information
    auto* plugin_info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_can_focus(plugin_info_box, false);
    auto* label_name = gtk_label_new(qUtf8Printable(details.name));
    gtk_box_pack_start(GTK_BOX(plugin_info_box), label_name, true, true, 0);
    gtk_box_pack_start(GTK_BOX(plugin_box), plugin_info_box, true, true, 0);

    // Add uninstall button
    auto image = gtk_image_new_from_icon_name("user-trash-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    auto* uninstall_plugin_button = gtk_button_new();
    gtk_widget_set_can_focus(uninstall_plugin_button, false);
    gtk_button_set_relief(GTK_BUTTON(uninstall_plugin_button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(uninstall_plugin_button, _("Uninstall Plugin"));
    gtk_button_set_image(GTK_BUTTON(uninstall_plugin_button), image);
    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(uninstall_plugin_button));
    gtk_style_context_add_class(context, "transparent-button");
    std::string label_btn = (details.path).toStdString();
    gtk_widget_set_name(uninstall_plugin_button, label_btn.c_str());
    g_signal_connect(uninstall_plugin_button, "clicked", G_CALLBACK(uninstall_plugin), view);
    gtk_box_pack_end(GTK_BOX(plugin_box), GTK_WIDGET(uninstall_plugin_button), false, false, 0);

    // Add load button
    auto* load_plugin_button = gtk_check_button_new();
    gtk_widget_set_can_focus(load_plugin_button, false);
    gtk_widget_set_name(load_plugin_button, label_btn.c_str());
    gtk_widget_set_tooltip_text(load_plugin_button, _("Load/Unload Plugin"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(load_plugin_button), details.loaded);
    g_signal_connect(load_plugin_button, "clicked", G_CALLBACK(toggle_plugin), view);
    gtk_box_pack_end(GTK_BOX(plugin_box), GTK_WIDGET(load_plugin_button), false, false, 0);

    // Insert at the end of the list
    gtk_list_box_insert(GTK_LIST_BOX(priv->list_installed_plugins), plugin_box, -1);
    gtk_widget_set_halign(GTK_WIDGET(plugin_box), GTK_ALIGN_FILL);
}

static void
refreshPluginsList(PluginSettingsView *self)
{
    g_return_if_fail(IS_PLUGIN_SETTINGS_VIEW(self));
    PluginSettingsViewPrivate *priv = PLUGIN_SETTINGS_VIEW_GET_PRIVATE(self);

    // Build devices list
    while (GtkWidget* children = GTK_WIDGET(gtk_list_box_get_row_at_index(GTK_LIST_BOX(priv->list_installed_plugins), 0)))
        gtk_container_remove(GTK_CONTAINER(priv->list_installed_plugins), children);
    auto plugins = priv->cpp->pluginModel_->getInstalledPlugins();
    for (const auto& plugin : plugins)
        add_plugin(self, plugin);
    gtk_widget_set_halign(GTK_WIDGET(priv->list_installed_plugins), GTK_ALIGN_FILL);
    gtk_widget_show_all(priv->list_installed_plugins);
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

    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, vbox_main);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, vbox_plugins_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, plugins_enabled_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, button_choose_jpl_file);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), PluginSettingsView, list_installed_plugins);
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

    if(show_preview) {
        gtk_widget_show_all(priv->plugins_enabled_button);
        gtk_widget_show_all(priv->button_choose_jpl_file);
        refreshPluginsList(self);
        if(priv->cpp->pluginModel_->getPluginsEnabled()) {
            gtk_widget_show_all(priv->vbox_plugins_list);
        }
    }
}
