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
#include "dbuserrorhandler.h"

#include <glib/gi18n.h>
#include <callmodel.h>
#include <globalinstances.h>
#include "../ring_client.h"

namespace Interfaces {

static GtkWidget*
dring_crash_dialog()
{
    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_decorated(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), C_("Name of error window (dialog)","Ring Error"));

    /* get the main window */
    if (auto app = g_application_get_default()) {
        auto win = ring_client_get_main_window(RING_CLIENT(app));
        if (win) {
            gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win));
            gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
        } else {
            gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        }
    } else {
        g_warning("no default GApplication exists");
    }

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_box_set_spacing(GTK_BOX(content_area), 10);
    gtk_widget_set_size_request(content_area, 250, -1);
    gtk_widget_set_margin_top(content_area, 25);

    auto message = gtk_label_new(
        _("Trying to reconnect to the Ring daemon (dring)...")
    );

    gtk_box_pack_start(GTK_BOX(content_area), message, FALSE, TRUE, 0);

    GtkWidget *spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(spinner));

    gtk_box_pack_start(GTK_BOX(content_area), spinner, FALSE, TRUE, 0);

    gtk_widget_show_all(content_area);

    return dialog;
}

static GtkWidget*
ring_quitting_dialog()
{
    /* get the main window */
    GtkWindow *win = NULL;
    if (auto app = g_application_get_default()) {
        win = ring_client_get_main_window(RING_CLIENT(app));
    } else {
        g_warning("no default GApplication exists");
    }

    GtkWidget *dialog = gtk_message_dialog_new(
        win,
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
        _("Could not re-connect to the Ring daemon (dring).\nRing will now quit.")
    );

    if (win) {
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    } else {
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    }
    gtk_window_set_title(GTK_WINDOW(dialog), C_("Name of error window (dialog)","Ring Error"));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    return dialog;
}

static gboolean
check_connection_cb(GtkWidget *warning_dialog)
{
    g_return_val_if_fail(GTK_IS_DIALOG(warning_dialog), G_SOURCE_REMOVE);

    gtk_widget_destroy(warning_dialog);

    if ((!CallModel::instance().isConnected()) || (!CallModel::instance().isValid())) {
        g_warning("could not reconnect to the daemon");

        auto quit_dialog = ring_quitting_dialog();

        /* wait for the user to exit the dialog */
        gtk_dialog_run(GTK_DIALOG(quit_dialog));
        gtk_widget_destroy(quit_dialog);

        /* quit */
        if (auto app = g_application_get_default()) {
            auto quit_action = G_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "quit"));
            g_action_activate(quit_action, NULL);
        } else {
            g_warning("no default GApplication exists");
        }
    } else {
        /* we're done handling the error */
        static_cast<DBusErrorHandler&>(GlobalInstances::dBusErrorHandler()).finishedHandlingError();
    }

    return G_SOURCE_REMOVE;
}

static gboolean
error_cb(G_GNUC_UNUSED gpointer user_data)
{
    g_warning("dring has possibly crashed, or has been killed... will wait 2.5 seconds and try to reconnect");

    auto warning_dialog = dring_crash_dialog();
    gtk_window_present(GTK_WINDOW(warning_dialog));

    /* allow 2.5 seconds for the daemon to restart and then see if we're re-connected */
    g_timeout_add(2500, (GSourceFunc)check_connection_cb, warning_dialog);

    return G_SOURCE_REMOVE;
}

void
DBusErrorHandler::connectionError(const QString& error)
{
    g_warning("%s", error.toUtf8().constData());

    if (!handlingError) {
        handlingError = true;
        /* the error may come from a different thread other than the main loop,
         * we use an idle function to run events on the main loop */
        g_idle_add((GSourceFunc)error_cb, NULL);
    }
}

void
DBusErrorHandler::invalidInterfaceError(const QString& error)
{
    g_warning("%s", error.toUtf8().constData());

    if (!handlingError) {
        handlingError = true;
        /* the error may come from a different thread other than the main loop,
         * we use an idle function to run events on the main loop */
        g_idle_add((GSourceFunc)error_cb, NULL);
    }
}

void
DBusErrorHandler::finishedHandlingError()
{
    handlingError = false;
}

} // namespace Interfaces
