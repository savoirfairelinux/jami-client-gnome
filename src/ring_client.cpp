/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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
 *  terms of the OpenSSL or SSLeay licenses, Savoir-faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "ring_client.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <QtCore/QTranslator>
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
#include <localhistorycollection.h>
#include <media/text.h>
#include <numbercategorymodel.h>
#include <globalinstances.h>
#include <memory>

#include "ring_client_options.h"
#include "ringmainwindow.h"
#include "dialogs.h"
#include "backends/edscontactbackend.h"
#include "native/pixbufmanipulator.h"
#include "ringnotify.h"
#include "config.h"
#include "utils/files.h"
#include "revision.h"
#include "utils/accounts.h"

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

    GSettings *settings;

    /* main window */
    GtkWidget        *win;
    /* for libRingclient */
    QCoreApplication *qtapp;
    /* UAM */
    QMetaObject::Connection uam_updated;

    std::unique_ptr<QTranslator> translator;

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
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>Q", "app.quit", NULL);
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

static const GActionEntry ring_actions[] =
{
    { "accept", NULL,         NULL, NULL,    NULL, {0} },
    { "hangup", NULL,         NULL, NULL,    NULL, {0} },
    { "hold",   NULL,         NULL, "false", NULL, {0} },
    { "quit",   action_quit,  NULL, NULL,    NULL, {0} },
    { "about",  action_about, NULL, NULL,    NULL, {0} },
    { "mute_audio", NULL,     NULL, "false", NULL, {0} },
    { "mute_video", NULL,     NULL, "false", NULL, {0} },
    { "record",     NULL,     NULL, "false", NULL, {0} },
    /* TODO implement the other actions */
    // { "transfer",   NULL,        NULL, "flase", NULL, {0} },
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
autostart_toggled(GSettings *settings, G_GNUC_UNUSED gchar *key, G_GNUC_UNUSED gpointer user_data)
{
    autostart_symlink(g_settings_get_boolean(settings, "start-on-login"));
}

static gboolean
on_close_window(GtkWidget *window, G_GNUC_UNUSED GdkEvent *event, RingClient *client)
{
    g_return_val_if_fail(GTK_IS_WINDOW(window) && IS_RING_CLIENT(client), FALSE);

    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(priv->settings, "hide-on-close")) {
        /* we want to simply hide the window and keep the client running */
        g_debug("hiding main window");
        gtk_widget_hide(window);
        return TRUE; /* do not propogate event */
    } else {
        /* we want to quit the application, so just propogate the event */
        return FALSE;
    }
}

static void
ring_client_activate(GApplication *app)
{
    RingClient *client = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (priv->win == NULL) {
        priv->win = ring_main_window_new(GTK_APPLICATION(app));

        /* make sure win is set to NULL when the window is destroyed */
        g_object_add_weak_pointer(G_OBJECT(priv->win), (gpointer *)&priv->win);

        /* check if the window should be destoryed or not on close */
        g_signal_connect(priv->win, "delete-event", G_CALLBACK(on_close_window), client);
    }

    g_debug("show window");
    gtk_window_present(GTK_WINDOW(priv->win));
}

static void
ring_client_startup(GApplication *app)
{
    RingClient *client = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    g_message("Ring GNOME client version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    g_message("git ref: %s", RING_CLIENT_REVISION);

    /* make sure that the system corresponds to the autostart setting */
    autostart_symlink(g_settings_get_boolean(priv->settings, "start-on-login"));
    g_signal_connect(priv->settings, "changed::start-on-login", G_CALLBACK(autostart_toggled), NULL);

    /* init clutter */
    int clutter_error;
    if ((clutter_error = gtk_clutter_init(&priv->argc, &priv->argv)) != CLUTTER_INIT_SUCCESS) {
        g_error("Could not init clutter : %d\n", clutter_error);
        exit(1); /* the g_error above should normally cause the applicaiton to exit */
    }

    /* init libRingClient and make sure its connected to the dbus */
    try {
        priv->qtapp = new QCoreApplication(priv->argc, priv->argv);
        /* the call model will try to connect to dring via dbus */
        CallModel::instance();
    } catch (const char * msg) {
        init_exception_dialog(msg);
        g_error("%s", msg);
        exit(1); /* the g_error above should normally cause the applicaiton to exit */
    } catch(QString& msg) {
        QByteArray ba = msg.toLocal8Bit();
        const char *c_str = ba.data();
        init_exception_dialog(c_str);
        g_error("%s", c_str);
        exit(1); /* the g_error above should normally cause the applicaiton to exit */
    }

    /* load translations from LRC */
    priv->translator.reset(new QTranslator);
    if (priv->translator->load(QLocale::system(), "lrc", "_", RING_CLIENT_INSTALL "/share/libringclient/translations")) {
        priv->qtapp->installTranslator(priv->translator.get());
    } else {
        g_debug("could not load LRC translations for %s, %s",
            QLocale::languageToString(QLocale::system().language()).toUtf8().constData(),
            QLocale::countryToString(QLocale::system().country()).toUtf8().constData()
        );
    }

    /* init delegates */
    GlobalInstances::setPixmapManipulator(std::unique_ptr<Interfaces::PixbufManipulator>(new Interfaces::PixbufManipulator()));

    /* make sure all RING accounts have a display name... this basically makes sure
     * that all accounts created before the display name patch have a display name
     * set... a bit of a hack as this should maybe be done in LRC */
    force_ring_display_name();

    /* make sure basic number categories exist, in case user has no contacts
     * from which these would be automatically created
     */
    NumberCategoryModel::instance()->addCategory("work", QVariant());
    NumberCategoryModel::instance()->addCategory("home", QVariant());

    /* add backends */
    CategorizedHistoryModel::instance()->addCollection<LocalHistoryCollection>(LoadOptions::FORCE_ENABLED);

    /* fallback backend for vcards */
    PersonModel::instance()->addCollection<FallbackPersonCollection>(LoadOptions::FORCE_ENABLED);

    /* EDS backend(s) */
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
    actionHash[ (int)UserActionModel::Action::MUTE_AUDIO      ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "mute_audio"));
    actionHash[ (int)UserActionModel::Action::MUTE_VIDEO      ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "mute_video"));
    /* TODO: add commented actions when ready */
    // actionHash[ (int)UserActionModel::Action::SERVER_TRANSFER ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "transfer"));
    actionHash[ (int)UserActionModel::Action::RECORD          ] = G_SIMPLE_ACTION(g_action_map_lookup_action(G_ACTION_MAP(app), "record"));
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

    /* show window on incoming calls (if the option is set)*/
    QObject::connect(CallModel::instance(), &CallModel::incomingCall,
        [app] (G_GNUC_UNUSED Call *call) {
            RingClient *client = RING_CLIENT(app);
            RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
            if (g_settings_get_boolean(priv->settings, "bring-window-to-front"))
                ring_client_activate(app);
        }
    );

    /* send call notifications */
    ring_notify_init();
    QObject::connect(CallModel::instance(), &CallModel::incomingCall,
        [] (Call *call) { ring_notify_incoming_call(call);}
    );

    /* chat notifications for incoming messages on all calls which are not the
     * currently selected call */
     ring_notify_monitor_chat_notifications(client);

#if GLIB_CHECK_VERSION(2,40,0)
    G_APPLICATION_CLASS(ring_client_parent_class)->startup(app);
#else
    /* don't need to chain up to the parent callback as this function will
     * be called manually by the command_line callback in this case */
#endif
}

#if !GLIB_CHECK_VERSION(2,40,0)
static int
ring_client_command_line(GApplication *app, GApplicationCommandLine *cmdline)
{
    gint argc;
    gchar **argv = g_application_command_line_get_arguments(cmdline, &argc);
    GOptionContext *context = ring_client_options_get_context();
    GError *error = NULL;
    if (g_option_context_parse(context, &argc, &argv, &error) == FALSE) {
        g_print(_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
                error->message, argv[0]);
        g_clear_error(&error);
        g_option_context_free(context);
        g_strfreev(argv);
        return 1;
    }
    g_option_context_free(context);
    g_strfreev(argv);

    if (!g_application_get_is_remote(app)) {
        /* if this is the primary instance, we must peform the startup */
        ring_client_startup(app);
    }

    g_application_activate(app);

    return 0;
}
#endif

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

    g_clear_object(&priv->settings);

    ring_notify_uninit();

    /* Chain up to the parent class */
    G_APPLICATION_CLASS(ring_client_parent_class)->shutdown(app);
}

static void
ring_client_init(RingClient *self)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(self);

    priv->win = NULL;
    priv->qtapp = NULL;
    priv->cancellable = g_cancellable_new();
    priv->settings = g_settings_new_full(get_ring_schema(), NULL, NULL);

#if GLIB_CHECK_VERSION(2,40,0)
    /* add custom cmd line options */
    ring_client_add_options(G_APPLICATION(self));
#endif
}

static void
ring_client_class_init(RingClientClass *klass)
{
#if GLIB_CHECK_VERSION(2,40,0)
    G_APPLICATION_CLASS(klass)->startup = ring_client_startup;
#else
    G_APPLICATION_CLASS(klass)->command_line = ring_client_command_line;
#endif
    G_APPLICATION_CLASS(klass)->activate = ring_client_activate;
    G_APPLICATION_CLASS(klass)->shutdown = ring_client_shutdown;
}

RingClient *
ring_client_new(int argc, char *argv[])
{
    /* because the g_application_add_main_option_entries was only added in
     * glib 2.40, for lower versions we must handle the command line options
     * ourselves
     */
    RingClient *client = (RingClient *)g_object_new(ring_client_get_type(),
                                                    "application-id", RING_CLIENT_APP_ID,
#if GLIB_CHECK_VERSION(2,40,0)
                                                    NULL);
#else
                                                    "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
                                                    NULL);
#endif

    /* copy the cmd line args before they get processed by the GApplication*/
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
    priv->argc = argc;
    priv->argv = g_strdupv((gchar **)argv);

    return client;
}

GtkWindow  *
ring_client_get_main_windw(RingClient *client)
{
    g_return_val_if_fail(IS_RING_CLIENT(client), NULL);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    return (GtkWindow *)priv->win;
}
