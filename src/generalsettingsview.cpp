/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "generalsettingsview.h"

#include <gtk/gtk.h>
#include <categorizedhistorymodel.h>
#include "utils/files.h"

struct _GeneralSettingsView
{
    GtkBox parent;
};

struct _GeneralSettingsViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _GeneralSettingsViewPrivate GeneralSettingsViewPrivate;

struct _GeneralSettingsViewPrivate
{
    GSettings *settings;

    /* Rint settings */
    GtkWidget *checkbutton_autostart;
    GtkWidget *checkbutton_hideonclose;
    GtkWidget *checkbutton_bringtofront;

    /* history settings */
    GtkWidget *adjustment_history_duration;
    GtkWidget *button_clear_history;
};

G_DEFINE_TYPE_WITH_PRIVATE(GeneralSettingsView, general_settings_view, GTK_TYPE_BOX);

#define GENERAL_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GENERAL_SETTINGS_VIEW_TYPE, GeneralSettingsViewPrivate))

static void
general_settings_view_dispose(GObject *object)
{
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(object);

    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(general_settings_view_parent_class)->dispose(object);
}

static void
history_limit_changed(GtkAdjustment *adjustment, G_GNUC_UNUSED gpointer user_data)
{
    int limit = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    CategorizedHistoryModel::instance()->setHistoryLimit(limit);
}

static gboolean
clear_history_dialog(GeneralSettingsView *self)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            "Are you sure you want to clear all your history?\n" \
                            "This operation will also reset the Frequen Contacts list");

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
        CategorizedHistoryModel::instance()->clearAllCollections();
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
    g_settings_bind(priv->settings, "hide-on-close",
                    priv->checkbutton_hideonclose, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "bring-window-to-front",
                    priv->checkbutton_bringtofront, "active",
                    G_SETTINGS_BIND_DEFAULT);

    /* history limit */
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_history_duration),
                             CategorizedHistoryModel::instance()->historyLimit());
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_hideonclose);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, checkbutton_bringtofront);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, adjustment_history_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, button_clear_history);
}

GtkWidget *
general_settings_view_new()
{
    gpointer view = g_object_new(GENERAL_SETTINGS_VIEW_TYPE, NULL);

    return (GtkWidget *)view;
}
