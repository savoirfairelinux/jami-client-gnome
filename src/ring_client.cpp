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
#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <callmodel.h>
#include <QtCore/QItemSelectionModel>
#include <useractionmodel.h>
#include <clutter-gtk/clutter-gtk.h>
#include <categorizedhistorymodel.h>
#include <personmodel.h>
#include <fallbackpersoncollection.h>
#include <QtCore/QStandardPaths>

#include "ring_client_options.h"
#include "ringmainwindow.h"
#include "backends/minimalhistorybackend.h"
#include "dialogs.h"
#include "backends/edscontactbackend.h"
#include "delegates/pixbufdelegate.h"

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
    /* args */
    int    argc;
    char **argv;
    /* main window */
    GtkWidget        *win;
    /* for libRingclient */
    QCoreApplication *qtapp;
    /* UAM */
    QMetaObject::Connection uam_updated;

    GCancellable *cancellable;
};

/* this union is used to pass ints as pointers and vice versa for GAction parameters*/
typedef union _int_ptr_t
{
    int value;
    gint64 value64;
    gpointer ptr;
} int_ptr_t;

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

static void
ring_accelerators(RingClient *client)
{
#if GTK_CHECK_VERSION(3,12,0)
    const gchar *quit_accels[2] = { "<Ctrl>Q", NULL };
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.quit", quit_accels);
#else
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>Q", "win.quit", NULL);
#endif
}

static void
action_quit(G_GNUC_UNUSED GSimpleAction *simple,
            G_GNUC_UNUSED GVariant      *parameter,
            gpointer user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));

#if GLIB_CHECK_VERSION(2,32,0)
    g_application_quit(G_APPLICATION(user_data));
#else
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(user_data);
    gtk_widget_destroy(priv->win);
#endif
}

static void
action_about(G_GNUC_UNUSED GSimpleAction *simple,
            G_GNUC_UNUSED GVariant      *parameter,
            gpointer user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(user_data);

    ring_about_dialog(priv->win);
}

static void
action_accept_call(G_GNUC_UNUSED GSimpleAction *simple,
                   GVariant *parameter,
                   gpointer  user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(user_data);

    /* the parameter should store the pointer of the Call* object */
    int_ptr_t call_ptr;
    call_ptr.value64 = g_variant_get_int64(parameter);

    g_return_if_fail(call_ptr.ptr != nullptr);

    /* check if this call exists */
    CallList list = CallModel::instance()->getActiveCalls();

    for (int i = 0; i < list.size(); ++i) {
        if ((gpointer)list.at(i) == call_ptr.ptr)
            ((Call*)call_ptr.ptr)->performAction(Call::Action::ACCEPT);
    }
    g_warning("Could not find call with pointer %p to accept", call_ptr.ptr);
}

static void
action_reject_call(G_GNUC_UNUSED GSimpleAction *simple,
                   GVariant *parameter,
                   gpointer  user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(user_data);

    /* the parameter should store the pointer of the Call* object */
    int_ptr_t call_ptr;
    call_ptr.value64 = g_variant_get_int64(parameter);

    g_return_if_fail(call_ptr.ptr != nullptr);

    /* check if this call exists */
    CallList list = CallModel::instance()->getActiveCalls();

    for (int i = 0; i < list.size(); ++i) {
        if ((gpointer)list.at(i) == call_ptr.ptr)
            ((Call*)call_ptr.ptr)->performAction(Call::Action::REFUSE);
    }
    g_warning("Could not find call with pointer %p to reject", call_ptr.ptr);
}

static const GActionEntry ring_actions[] =
{
    { "accept", NULL,         NULL, NULL,    NULL, {0} },
    { "hangup", NULL,         NULL, NULL,    NULL, {0} },
    { "hold",   NULL,         NULL, "false", NULL, {0} },
    { "quit",   action_quit,  NULL, NULL,    NULL, {0} },
    { "about",  action_about, NULL, NULL,    NULL, {0} },
    /* TODO implement the other actions */
    // { "mute_audio", NULL,        NULL, "false", NULL, {0} },
    // { "mute_video", NULL,        NULL, "false", NULL, {0} },
    // { "transfer",   NULL,        NULL, "flase", NULL, {0} },
    // { "record",     NULL,        NULL, "false", NULL, {0} }
    /* actions for specific calls (for notifications) */
    { "accept_call", action_accept_call, (const gchar*)G_VARIANT_TYPE_INT64, NULL, NULL, {0} },
    { "reject_call", action_reject_call, (const gchar*)G_VARIANT_TYPE_INT64, NULL, NULL, {0} },
};

static void
activate_action(GSimpleAction *action, G_GNUC_UNUSED GVariant *parameter, gpointer user_data)
{
    g_debug("activating action: %s", g_action_get_name(G_ACTION(action)));

    int_ptr_t key;

    key.ptr = user_data;
    UserActionModel::Action a = static_cast<UserActionModel::Action>(key.value);
    UserActionModel* uam = CallModel::instance()->userActionModel();

    uam << a;
}

static void
ring_client_startup(GApplication *app)
{

    g_debug("startup");
    RingClient *client = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    /* init clutter */
    int clutter_error;
    if ((clutter_error = gtk_clutter_init(&priv->argc, &priv->argv)) != CLUTTER_INIT_SUCCESS) {
        g_error("Could not init clutter : %d\n", clutter_error);
        return;
        // return 1;
    }

    /* init libRingClient and make sure its connected to the dbus */
    try {
        priv->qtapp = new QCoreApplication(priv->argc, priv->argv);
        /* the call model will try to connect to dring via dbus */
        CallModel::instance();
    } catch (const char * msg) {
        init_exception_dialog(msg);
        g_error("%s", msg);
        return;
        // return 1;
    } catch(QString& msg) {
        QByteArray ba = msg.toLocal8Bit();
        const char *c_str = ba.data();
        init_exception_dialog(c_str);
        g_error("%s", c_str);
        return;
        // return 1;
    }

    /* init delegates */
    /* FIXME: put in smart pointer? */
    new PixbufDelegate();

    /* add backends */
    CategorizedHistoryModel::instance()->addCollection<MinimalHistoryBackend>(LoadOptions::FORCE_ENABLED);

    PersonModel::instance()->addCollection<FallbackPersonCollection>(LoadOptions::FORCE_ENABLED);

    /* TODO: should a local vcard location be added ?
     * PersonModel::instance()->addCollection<FallbackPersonCollection, QString>(
     *    QStandardPaths::writableLocation(QStandardPaths::DataLocation)+QLatin1Char('/')+"vcard",
     *    LoadOptions::FORCE_ENABLED);
     */

    /* EDS backend */
    load_eds_sources(priv->cancellable);

    /* Override theme since we don't have appropriate icons for a dark them (yet) */
    GtkSettings *gtk_settings = gtk_settings_get_default();
    g_object_set(G_OBJECT(gtk_settings), "gtk-application-prefer-dark-theme",
                 FALSE, NULL);
    /* enable button icons */
    g_object_set(G_OBJECT(gtk_settings), "gtk-button-images",
                 TRUE, NULL);

    /* add GActions */
    g_action_map_add_action_entries(
        G_ACTION_MAP(app), ring_actions, G_N_ELEMENTS(ring_actions), app);

    /* add accelerators */
    ring_accelerators(RING_CLIENT(app));

    /* Bind GActions to the UserActionModel */
    UserActionModel* uam = CallModel::instance()->userActionModel();
    QHash<int, GSimpleAction*> actionHash;
    actionHash[ (int)UserActionModel::Action::ACCEPT          ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "accept"));
    actionHash[ (int)UserActionModel::Action::HOLD            ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "hold"));
    /* TODO: add commented actions when ready */
    // actionHash[ (int)UserActionModel::Action::MUTE_AUDIO      ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "mute_audio"));
    // actionHash[ (int)UserActionModel::Action::SERVER_TRANSFER ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "transfer"));
    // actionHash[ (int)UserActionModel::Action::RECORD          ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "record"));
    actionHash[ (int)UserActionModel::Action::HANGUP          ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "hangup"));

    for (QHash<int,GSimpleAction*>::const_iterator i = actionHash.begin(); i != actionHash.end(); ++i) {
       GSimpleAction* sa = i.value();
       int_ptr_t user_data;
       user_data.value = i.key();
       g_signal_connect(G_OBJECT(sa), "activate", G_CALLBACK(activate_action), user_data.ptr);
    }

    /* change the state of the GActions based on the UserActionModel */
    priv->uam_updated = QObject::connect(uam,&UserActionModel::dataChanged, [actionHash,uam](const QModelIndex& tl, const QModelIndex& br) {
       const int first(tl.row()),last(br.row());
       for(int i = first; i <= last;i++) {
          const QModelIndex& idx = uam->index(i,0);
          GSimpleAction* sa = actionHash[(int)qvariant_cast<UserActionModel::Action>(idx.data(UserActionModel::Role::ACTION))];
          if (sa) {
            /* enable/disable GAction based on UserActionModel */
            g_simple_action_set_enabled(sa, idx.flags() & Qt::ItemIsEnabled);
            /* set the state of the action if its stateful */
            if (g_action_get_state_type(G_ACTION(sa)) != NULL)
                g_simple_action_set_state(sa, g_variant_new_boolean(idx.data(Qt::CheckStateRole) == Qt::Checked));
          }
       }
    });

    /* add call notifications */
    // QObject::connect(
    //     CallModel::instance(),
    //     &CallModel::incomingCall,
    //     [=] (Call* call) {
    //         g_return_if_fail(call);
    //         g_debug("incoming call notification for call pointer %p", (gpointer)call);
    //
    //         if (!g_action_map_lookup_action(G_ACTION_MAP(app), "accept_call"))
    //             g_warning("action doesn't exist");
    //
    //         gchar *title = g_strdup_printf("Incoming call");
    //         gchar *body = g_strdup_printf("%s", call->formattedName().toUtf8().constData());
    //         GNotification *notification = g_notification_new(title);
    //         // g_notification_set_body(notification, body);
    //         /* the doc sites phone calls as an example of urgent notifications */
    //         // g_notification_set_priority(notification, G_NOTIFICATION_PRIORITY_URGENT);
    //         // int_ptr_t call_ptr;
    //         // call_ptr.ptr = (gpointer)call;
    //         // g_notification_add_button_with_target_value(notification,
    //         //                                             "Accept",
    //         //                                             "accept_call",
    //         //                                             g_variant_new_int64(call_ptr.value64));
    //         //
    //         // g_notification_add_button_with_target_value(notification,
    //         //                                             "Reject",
    //         //                                             "reject_call",
    //         //                                              g_variant_new_int64(call_ptr.value64));
    //         g_notification_add_button(notification, "Accept", "app.accept");
    //
    //         GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource("/cx/ring/RingGnome/ring-symbol-blue", nullptr);
    //         GBytes *pixbuf_bytes = NULL;
    //         g_object_get(pixbuf, "pixel-bytes", &pixbuf_bytes, NULL);
    //         if (pixbuf_bytes) {
    //             GIcon *icon = g_bytes_icon_new(pixbuf_bytes);
    //             g_notification_set_icon(notification, icon);
    //         }
    //
    //
    //         g_application_send_notification(app, "test", notification);
    //         // g_object_unref(notification);
    //         // g_free(title);
    //         // g_free(body);
    //     }
    // );

    // return 0;

    G_APPLICATION_CLASS(ring_client_parent_class)->startup(app);
}

// static int
// ring_client_command_line(GApplication *app, GApplicationCommandLine *cmdline)
// {
//     RingClient *client = RING_CLIENT(app);
//     RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
//
//     gint argc;
//     gchar **argv = g_application_command_line_get_arguments(cmdline, &argc);
//     GOptionContext *context = ring_client_options_get_context();
//     GError *error = NULL;
//     if (g_option_context_parse(context, &argc, &argv, &error) == FALSE) {
//         g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
//                 error->message, argv[0]);
//         g_clear_error(&error);
//         g_option_context_free(context);
//         return 1;
//     }
//     g_option_context_free(context);
//
//     /* init libs and create main window only once */
//     if (priv->win == NULL) {
//         if (ring_client_startup(app, argc, argv) != 0)
//             return 1;
//         priv->win = ring_main_window_new(GTK_APPLICATION(app));
//     }
//
//     gtk_window_present(GTK_WINDOW(priv->win));
//
//     return 0;
// }

static void
ring_client_activate(GApplication *app)
{
    g_debug("activate");
    RingClient *client = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (priv->win == NULL) {
        priv->win = ring_main_window_new(GTK_APPLICATION(app));
    }

    gtk_window_present(GTK_WINDOW(priv->win));
}

static void
ring_client_shutdown(GApplication *app)
{
    RingClient *self = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(self);

    g_debug("quitting");

    /* cancel any pending cancellable operations */
    g_cancellable_cancel(priv->cancellable);
    g_object_unref(priv->cancellable);

    QObject::disconnect(priv->uam_updated);

    /* free the QCoreApplication, which will destroy all libRingClient models
     * and thus send the Unregister signal over dbus to dring */
    delete priv->qtapp;

    /* free the copied cmd line args */
    g_strfreev(priv->argv);

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
    priv->cancellable = g_cancellable_new();

    ring_client_add_options(G_APPLICATION(self));
}

static void
ring_client_class_init(RingClientClass *klass)
{
    G_APPLICATION_CLASS(klass)->startup = ring_client_startup;
    G_APPLICATION_CLASS(klass)->activate = ring_client_activate;
    G_APPLICATION_CLASS(klass)->shutdown = ring_client_shutdown;
}

RingClient *
ring_client_new(int argc, char *argv[])
{
    RingClient *client = (RingClient *)g_object_new(ring_client_get_type(),
                                                    "application-id", "cx.ring.gnome-ring", NULL);
                                    //   "flags", G_APPLICATION_HANDLES_COMMAND_LINE, NULL);

    /* copy the cmd line args before they get processed by the GApplication*/
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
    priv->argc = argc;
    priv->argv = g_strdupv((gchar **)argv);

    return client;
}
