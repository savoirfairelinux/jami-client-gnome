/*
 *  Copyright (C) 2015-2018 Savoir-faire Linux Inc.
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

#include "generalsettingsview.h"

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib.h>

// Ring client
#include "utils/files.h"
#include "avatarmanipulation.h"

enum
{
  PROP_RING_MAIN_WIN_PNT = 1,
};

struct _GeneralSettingsView
{
    GtkScrolledWindow parent;
};

struct _GeneralSettingsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _GeneralSettingsViewPrivate GeneralSettingsViewPrivate;

struct _GeneralSettingsViewPrivate
{
    GSettings *settings;

    /* Rint settings */
    GtkWidget *checkbutton_autostart;
    GtkWidget *checkbutton_showstatusicon;
    GtkWidget *checkbutton_bringtofront;
    GtkWidget *checkbutton_callnotifications;
    GtkWidget *checkbutton_pendingnotifications;
    GtkWidget *checkbutton_chatnotifications;
    GtkWidget *checkbutton_chatdisplaylinks;
    GtkWidget *checkbutton_searchentryplacescall;
    GtkWidget *radiobutton_chatright;
    GtkWidget *radiobutton_chatbottom;
    GtkWidget *button_choose_downloads_directory;

    /* history settings */
    GtkWidget *adjustment_history_duration;
    GtkWidget *button_clear_history;

    /* ring main window pointer */
    GtkWidget* ring_main_window_pnt;
};

G_DEFINE_TYPE_WITH_PRIVATE(GeneralSettingsView, general_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define GENERAL_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GENERAL_SETTINGS_VIEW_TYPE, GeneralSettingsViewPrivate))

enum {
    CLEAR_ALL_HISTORY,
    UPDATE_DOWNLOAD_FOLDER,
    LAST_SIGNAL
};

static guint general_settings_view_signals[LAST_SIGNAL] = { 0 };

static void
general_settings_view_dispose(GObject *object)
{
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(object);

    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(general_settings_view_parent_class)->dispose(object);
}

static gboolean
clear_history_dialog(GeneralSettingsView *self)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            _("This is a destructive operation. Are you sure you want to delete all of your chat and call history?"));

    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(self));
    if (gtk_widget_is_toplevel(parent)) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_OK:
            response = TRUE;
            break;
        default:
            response = FALSE;
            break;
    }

    gtk_widget_destroy(dialog);

    return response;
}

static void
clear_history(G_GNUC_UNUSED GtkWidget *button, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));

    if (clear_history_dialog(self) )
        g_signal_emit(G_OBJECT(self), general_settings_view_signals[CLEAR_ALL_HISTORY], 0);
}

static void
update_downloads_button_label(GeneralSettingsView *self)
{
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    // Get folder name
    const gchar *folder_dirname = g_path_get_basename(g_variant_get_string(g_settings_get_value(priv->settings, "download-folder"), NULL));
    gtk_button_set_label(GTK_BUTTON(priv->button_choose_downloads_directory), folder_dirname);
}

static void
change_prefered_directory (gchar * directory, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    priv->settings = g_settings_new_full(get_ring_schema(), NULL, NULL);
    g_settings_set_value(priv->settings, "download-folder", g_variant_new("s", directory));
    update_downloads_button_label(self);
}

static void
choose_downloads_directory(GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));

    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    gint res;
    gchar* filename = nullptr;

    if (!priv->ring_main_window_pnt) {
        g_debug("Internal error: NULL main window pointer in GeneralSettingsView.");
        return;
    }

    GtkWidget *dialog = gtk_file_chooser_dialog_new (_("Choose download folder"),
                                      GTK_WINDOW(priv->ring_main_window_pnt),
                                      GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
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

    // set download folder
    change_prefered_directory(filename, self);

    g_signal_emit(G_OBJECT(self), general_settings_view_signals[UPDATE_DOWNLOAD_FOLDER], 0, filename);
}

static void
general_settings_view_init(GeneralSettingsView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    priv->settings = g_settings_new_full(get_ring_schema(), NULL, NULL);

    /* bind client option to gsettings */
    g_settings_bind(priv->settings, "start-on-login",
                    priv->checkbutton_autostart, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "show-status-icon",
                    priv->checkbutton_showstatusicon, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "bring-window-to-front",
                    priv->checkbutton_bringtofront, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-call-notifications",
                    priv->checkbutton_callnotifications, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-display-links",
                    priv->checkbutton_chatdisplaylinks, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-pending-notifications",
                    priv->checkbutton_pendingnotifications, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-chat-notifications",
                    priv->checkbutton_chatnotifications, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "search-entry-places-call",
                    priv->checkbutton_searchentryplacescall, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "chat-pane-horizontal",
                    priv->radiobutton_chatright, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "chat-pane-horizontal",
                    priv->radiobutton_chatbottom, "active",
                    (GSettingsBindFlags) (G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN));
    g_settings_bind(priv->settings, "history-limit",
                    priv->adjustment_history_duration, "value",
                    G_SETTINGS_BIND_DEFAULT);

    /* Set-up download directory settings, default if none specified */
    auto* download_directory_variant = g_settings_get_value(priv->settings, "download-folder");
    char* download_directory_value;
    g_variant_get(download_directory_variant, "&s", &download_directory_value);
    std::string default_download_dir = {};
    if (auto* directory = g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD))
        default_download_dir = directory;
    else
        default_download_dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
    auto current_value = std::string(download_directory_value);
    if (current_value.empty()) {
        g_settings_set_value(priv->settings, "download-folder", g_variant_new("s", default_download_dir.c_str()));
    }
    update_downloads_button_label(self);

    /* clear history */
    g_signal_connect(priv->button_clear_history, "clicked", G_CALLBACK(clear_history), self);
}


static void
general_settings_view_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    GeneralSettingsView *self = GENERAL_SETTINGS_VIEW (object);
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_RING_MAIN_WIN_PNT) {
        GtkWidget *ring_main_window_pnt = (GtkWidget*) g_value_get_pointer(value);

        if (!ring_main_window_pnt) {
            g_debug("Internal error: NULL main window pointer passed to set_property");
            return;
        }

        priv->ring_main_window_pnt = ring_main_window_pnt;
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
general_settings_view_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    GeneralSettingsView *self = GENERAL_SETTINGS_VIEW (object);
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_RING_MAIN_WIN_PNT) {
        g_value_set_pointer(value, priv->ring_main_window_pnt);
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
general_settings_view_class_init(GeneralSettingsViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = general_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/generalsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_autostart);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_showstatusicon);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_bringtofront);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_callnotifications);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_chatdisplaylinks);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_pendingnotifications);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_chatnotifications);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_searchentryplacescall);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, radiobutton_chatright);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, radiobutton_chatbottom);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, adjustment_history_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, button_clear_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, button_choose_downloads_directory);

    general_settings_view_signals[CLEAR_ALL_HISTORY] = g_signal_new (
        "clear-all-history",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    general_settings_view_signals[UPDATE_DOWNLOAD_FOLDER] = g_signal_new("update-download-folder",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,
        1, G_TYPE_STRING);

    /* Define class properties: e.g. pointer to main window, etc.*/
    object_class->set_property = general_settings_view_set_property;
    object_class->get_property = general_settings_view_get_property;

    GParamFlags flags = (GParamFlags) (G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_RING_MAIN_WIN_PNT,
        g_param_spec_pointer ("ring_main_window_pnt",
                              "RingMainWindow pointer",
                              "Pointer to the Ring Main Window. This property is used by modal dialogs.",
                              flags));
}

GtkWidget *
general_settings_view_new(GtkWidget* ring_main_window_pointer)
{
    gpointer view = g_object_new(GENERAL_SETTINGS_VIEW_TYPE, NULL);

    // set_up ring main window pointer (needed by modal dialogs)
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_POINTER);
    g_value_set_pointer (&val, ring_main_window_pointer);
    g_object_set_property (G_OBJECT (view), "ring_main_window_pnt", &val);
    g_value_unset (&val);

    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(GENERAL_SETTINGS_VIEW (view));
    g_signal_connect_swapped(priv->button_choose_downloads_directory, "clicked", G_CALLBACK(choose_downloads_directory), view);

    return (GtkWidget *)view;
}
