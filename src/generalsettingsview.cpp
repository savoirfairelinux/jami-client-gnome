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

#include "generalsettingsview.h"

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// LRC
#include <person.h>
#include <profile.h>
#include <profilemodel.h>
#include <categorizedhistorymodel.h>
#include <media/recordingmodel.h>

// Ring client
#include "utils/files.h"
#include "avatarmanipulation.h"

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
    GtkWidget *checkbutton_chatdisplaylinks;
    GtkWidget *checkbutton_chatnotifications;
    GtkWidget *checkbutton_searchentryplacescall;
    GtkWidget *radiobutton_chatright;
    GtkWidget *radiobutton_chatbottom;
    GtkWidget *box_profil_settings;
    GtkWidget *avatarmanipulation;
    GtkWidget *profile_name;

    /* history settings */
    GtkWidget *adjustment_history_duration;
    GtkWidget *button_clear_history;
};

G_DEFINE_TYPE_WITH_PRIVATE(GeneralSettingsView, general_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define GENERAL_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GENERAL_SETTINGS_VIEW_TYPE, GeneralSettingsViewPrivate))

static void
general_settings_view_dispose(GObject *object)
{
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(object);

    g_clear_object(&priv->settings);

    //make sure the VideoWidget is destroyed
    general_settings_view_show_profile(GENERAL_SETTINGS_VIEW(object), FALSE);

    G_OBJECT_CLASS(general_settings_view_parent_class)->dispose(object);
}

static void
history_limit_changed(GtkAdjustment *adjustment, G_GNUC_UNUSED gpointer user_data)
{
    int limit = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    CategorizedHistoryModel::instance().setHistoryLimit(limit);
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

    if (clear_history_dialog(self) ) {
        CategorizedHistoryModel::instance().clear();
        Media::RecordingModel::instance().clear();
    }
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

    /* history limit */
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_history_duration),
                             CategorizedHistoryModel::instance().historyLimit());
    g_signal_connect(priv->adjustment_history_duration,
                     "value-changed", G_CALLBACK(history_limit_changed), NULL);

    /* clear history */
    g_signal_connect(priv->button_clear_history, "clicked", G_CALLBACK(clear_history), self);
}

static void
general_settings_view_class_init(GeneralSettingsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = general_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/generalsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_autostart);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_showstatusicon);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_bringtofront);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_callnotifications);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_chatdisplaylinks);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_chatnotifications);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_searchentryplacescall);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, radiobutton_chatright);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, radiobutton_chatbottom);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, adjustment_history_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, button_clear_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, box_profil_settings);
}

GtkWidget *
general_settings_view_new()
{
    gpointer view = g_object_new(GENERAL_SETTINGS_VIEW_TYPE, NULL);

    return (GtkWidget *)view;
}

static void
change_profile_name(GtkEntry *entry)
{
    auto profile = ProfileModel::instance().selectedProfile();
    profile->person()->setFormattedName(gtk_entry_get_text(entry));
    profile->save();
}

void
general_settings_view_show_profile(GeneralSettingsView *self, gboolean show_profile)
{
    g_return_if_fail(GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    /* We will construct and destroy the profile (AvatarManipulation widget) each time the profile
     * should be visible and hidden, respectively. It is not the "prettiest" way of doing things,
     * but this way we ensure 1. that the profile is updated correctly when it is shown and 2. that
     * the VideoWidget inside is destroyed when it is not being shown.
     */
    if (show_profile) {
        /* avatar manipulation widget */
        priv->avatarmanipulation = avatar_manipulation_new();
        gtk_widget_set_halign(priv->box_profil_settings, GtkAlign::GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(priv->box_profil_settings), priv->avatarmanipulation, true, true, 0);
        gtk_widget_set_visible(priv->avatarmanipulation, true);

        /* print the profile name. as long as we have only one profil, profil name = person name (a.k.a formatedName) */
        priv->profile_name = gtk_entry_new();
        gtk_entry_set_text (GTK_ENTRY(priv->profile_name),
                            ProfileModel::instance().selectedProfile()->person()->formattedName().toUtf8().constData());
        gtk_widget_set_visible(priv->profile_name, true);
        gtk_entry_set_alignment(GTK_ENTRY(priv->profile_name), 0.5);
        gtk_box_pack_start(GTK_BOX(priv->box_profil_settings), priv->profile_name, true, true, 0);
        g_signal_connect(priv->profile_name, "changed", G_CALLBACK(change_profile_name), NULL);
    } else {
        if (priv->avatarmanipulation) {
            gtk_container_remove(GTK_CONTAINER(priv->box_profil_settings), priv->avatarmanipulation);
            priv->avatarmanipulation = nullptr;
        }
        if (priv->profile_name) {
            gtk_container_remove(GTK_CONTAINER(priv->box_profil_settings), priv->profile_name);
            priv->profile_name = nullptr;
        }
    }
}
