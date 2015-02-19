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

#include "ring_client.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <callmodel.h>

#include "ring_client_options.h"
#include "ringmainwindow.h"

struct _RingClientClass
{
  GtkApplicationClass parent_class;
};

struct _RingClient
{
    GtkApplication parent;
};

typedef struct _RingClientPrivate RingClientPrivate;

struct _RingClientPrivate {
    /* main window */
    GtkWidget        *win;
    /* for libRingclient */
    QCoreApplication *qtapp;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingClient, ring_client, GTK_TYPE_APPLICATION);

#define RING_CLIENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_CLIENT_TYPE, RingClientPrivate))

static void
init_exception_dialog(const char* msg)
{
    g_warning("%s", msg);
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                            _("Unable to initialize.\nMake sure the Ring daemon (dring) is running.\nError: %s"),
                            msg);

    gtk_window_set_title(GTK_WINDOW(dialog), _("Ring Error"));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static int
ring_client_command_line(GApplication *app, GApplicationCommandLine *cmdline)
{
    RingClient *client = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    gint argc;
    gchar **argv = g_application_command_line_get_arguments(cmdline, &argc);
    GOptionContext *context = ring_client_options_get_context();
    GError *error = NULL;
    if (g_option_context_parse(context, &argc, &argv, &error) == FALSE) {
        g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
                error->message, argv[0]);
        g_error_free(error);
        g_option_context_free(context);
        return 1;
    }
    g_option_context_free(context);

    /* Override theme since we don't have appropriate icons for a dark them (yet) */
    GtkSettings *gtk_settings = gtk_settings_get_default();
    g_object_set(G_OBJECT(gtk_settings), "gtk-application-prefer-dark-theme",
                 FALSE, NULL);
    /* enable button icons */
    g_object_set(G_OBJECT(gtk_settings), "gtk-button-images",
                 TRUE, NULL);

    /* init libRingClient and make sure its connected to the dbus */
    try {
        /* TODO: do we care about passing the cmd line arguments here? */
        priv->qtapp = new QCoreApplication(argc, argv);
        /* the call model will try to connect to dring via dbus */
        CallModel::instance();
    } catch (const char * msg) {
        init_exception_dialog(msg);
        return 1;
    } catch(QString& msg) {
        QByteArray ba = msg.toLocal8Bit();
        const char *c_str = ba.data();
        init_exception_dialog(c_str);
        return 1;
    }

    /* create an empty window */
    if (priv->win == NULL) {
        priv->win = ring_main_window_new(GTK_APPLICATION(app));
    }

    gtk_window_present(GTK_WINDOW(priv->win));

    return 0;
}

static void
ring_client_shutdown(GApplication *app)
{
    RingClient *self = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(self);

    /* free the QCoreApplication, which will destroy all libRingClient models
     * and thus send the Unregister signal over dbus to dring */
    delete priv->qtapp;

    /* Chain up to the parent class */
    G_APPLICATION_CLASS(ring_client_parent_class)->shutdown(app);
}

static void
ring_client_init(RingClient *self)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(self);

    /* init widget */
    priv->win = NULL;
    priv->qtapp = NULL;
}

static void
ring_client_class_init(RingClientClass *klass)
{
    G_APPLICATION_CLASS(klass)->command_line = ring_client_command_line;
    G_APPLICATION_CLASS(klass)->shutdown = ring_client_shutdown;
}

RingClient *
ring_client_new()
{
    return (RingClient *)g_object_new(ring_client_get_type(),
                                      "application-id", "cx.ring.RingGnome",
                                      "flags", G_APPLICATION_HANDLES_COMMAND_LINE, NULL);
}
