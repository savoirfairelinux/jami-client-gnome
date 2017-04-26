/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

#include "ring_client.h"

// system
#include <memory>
#include <regex>

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <clutter-gtk/clutter-gtk.h>

// Qt
#include <QtCore/QTranslator>
#include <QtCore/QCoreApplication>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QStandardPaths>

// LRC
#include <callmodel.h>
#include <useractionmodel.h>
#include <categorizedhistorymodel.h>
#include <personmodel.h>
#include <fallbackpersoncollection.h>
#include <localhistorycollection.h>
#include <media/text.h>
#include <numbercategorymodel.h>
#include <globalinstances.h>
#include <profilemodel.h>
#include <profile.h>
#include <peerprofilecollection.h>
#include <localprofilecollection.h>
#include <accountmodel.h>
#include <smartinfohub.h>
#include <media/textrecording.h>
#include <media/recordingmodel.h>
#include <availableaccountmodel.h>
#include <banscollection.h>

// Ring client
#include "ring_client_options.h"
#include "ringmainwindow.h"
#include "dialogs.h"
#include "backends/edscontactbackend.h"
#include "native/pixbufmanipulator.h"
#include "native/dbuserrorhandler.h"
#include "ringnotify.h"
#include "config.h"
#include "utils/files.h"
#include "revision.h"
#include "utils/accounts.h"
#include "utils/calling.h"

#if HAVE_APPINDICATOR
#include <libappindicator/app-indicator.h>
#endif

#if USE_LIBNM
#include <libnm-glib/nm-client.h>
#endif

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

    gboolean restore_window_state;

    gpointer systray_icon;
    GtkWidget *icon_menu;

#if USE_LIBNM
    /* NetworkManager */
    NMClient *nm_client;
    NMActiveConnection *primary_connection;
#endif

    /* notifications */
    QMetaObject::Connection call_notification;
    QMetaObject::Connection chat_notification;
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
exception_dialog(const char* msg)
{
    g_critical("%s", msg);
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

static void
toggle_smartinfo(GSimpleAction *action, GVariant *parameter, gpointer)
{
    g_simple_action_set_state(action, parameter);
    if (g_variant_get_boolean(parameter)) {
        SmartInfoHub::instance().start();
    } else {
        SmartInfoHub::instance().stop();
    }
}

static const GActionEntry ring_actions[] =
{
    { "accept",             NULL,         NULL, NULL,    NULL, {0} },
    { "hangup",             NULL,         NULL, NULL,    NULL, {0} },
    { "hold",               NULL,         NULL, "false", NULL, {0} },
    { "quit",               action_quit,  NULL, NULL,    NULL, {0} },
    { "about",              action_about, NULL, NULL,    NULL, {0} },
    { "mute_audio",         NULL,         NULL, "false", NULL, {0} },
    { "mute_video",         NULL,         NULL, "false", NULL, {0} },
    { "record",             NULL,         NULL, "false", NULL, {0} },
    { "display-smartinfo",  NULL,         NULL, "false", toggle_smartinfo, {0} },
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
    UserActionModel* uam = CallModel::instance().userActionModel();

    uam << a;
}

static void
autostart_toggled(GSettings *settings, G_GNUC_UNUSED gchar *key, G_GNUC_UNUSED gpointer user_data)
{
    autostart_symlink(g_settings_get_boolean(settings, "start-on-login"));
}


static void
show_main_window_toggled(RingClient *client)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(priv->settings, "show-main-window")) {
        gtk_window_present(GTK_WINDOW(priv->win));
    } else {
        gtk_widget_hide(priv->win);
    }
}

static void
ring_window_show(RingClient *client)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
    g_settings_set_boolean(priv->settings, "show-main-window", TRUE);
}

static void
ring_window_hide(RingClient *client)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
    g_settings_set_boolean(priv->settings, "show-main-window", FALSE);
}

static gboolean
on_close_window(GtkWidget *window, G_GNUC_UNUSED GdkEvent *event, RingClient *client)
{
    g_return_val_if_fail(GTK_IS_WINDOW(window) && IS_RING_CLIENT(client), FALSE);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(priv->settings, "show-status-icon")) {
        /* we want to simply hide the window and keep the client running */
        ring_window_hide(client);
        return TRUE; /* do not propogate event */
    } else {
        /* we want to quit the application, so just propogate the event */
        return FALSE;
    }
}

static void
popup_menu(GtkStatusIcon *self,
           guint          button,
           guint          when,
           RingClient    *client)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS // GtkStatusIcon is deprecated since 3.14, but we fallback on it
    gtk_menu_popup(GTK_MENU(priv->icon_menu), NULL, NULL, gtk_status_icon_position_menu, self, button, when);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

static void
init_systray(RingClient *client)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    // init menu
    if (!priv->icon_menu) {

        /* for some reason AppIndicator doesn't like the menu being built from a GMenuModel and/or
         * the GMenuModel being built from an xml resource. So we build the menu in code.
         */
        priv->icon_menu = gtk_menu_new();
        g_object_ref_sink(priv->icon_menu);

        auto item = gtk_check_menu_item_new_with_label(C_("In the status icon menu, toggle action to show or hide the Ring main window", "Show Ring"));
        gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "app.show-main-window");
        gtk_menu_shell_append(GTK_MENU_SHELL(priv->icon_menu), item);

        item = gtk_menu_item_new_with_label(_("Quit"));
        gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "app.quit");
        gtk_menu_shell_append(GTK_MENU_SHELL(priv->icon_menu), item);

        gtk_widget_insert_action_group(priv->icon_menu, "app", G_ACTION_GROUP(client));
        gtk_widget_show_all(priv->icon_menu);
    }

    gboolean use_appinidcator = FALSE;

#if HAVE_APPINDICATOR
    /* only use AppIndicator in Unity (Tuleap: #1440) */
    const auto desktop = g_getenv("XDG_CURRENT_DESKTOP");
    if (g_strcmp0("Unity", desktop) == 0 || g_strcmp0("KDE", desktop) == 0) {
        use_appinidcator = TRUE;

        auto indicator = app_indicator_new("ring", "ring", APP_INDICATOR_CATEGORY_COMMUNICATIONS);
        app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
        app_indicator_set_title(indicator, "ring");
        /* app indicator requires a menu */
        app_indicator_set_menu(indicator, GTK_MENU(priv->icon_menu));
        priv->systray_icon = indicator;
    }
#endif

    if (!use_appinidcator) {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS // GtkStatusIcon is deprecated since 3.14, but we fallback on it
        auto status_icon = gtk_status_icon_new_from_icon_name("ring");
        gtk_status_icon_set_title(status_icon, "ring");
G_GNUC_END_IGNORE_DEPRECATIONS
        g_signal_connect_swapped(status_icon, "activate", G_CALLBACK(ring_window_show), client);
        g_signal_connect(status_icon, "popup-menu", G_CALLBACK(popup_menu), client);

        priv->systray_icon = status_icon;
    }
}

static void
systray_toggled(GSettings *settings, const gchar *key, RingClient *client)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(settings, key)) {
        if (!priv->systray_icon)
            init_systray(client);
    } else {
        if (priv->systray_icon)
            g_clear_object(&priv->systray_icon);
    }
}

static void
ring_client_activate(GApplication *app)
{
    RingClient *client = RING_CLIENT(app);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    if (priv->win == NULL) {
        // activate being called for the first time
        priv->win = ring_main_window_new(GTK_APPLICATION(app));

        /* make sure win is set to NULL when the window is destroyed */
        g_object_add_weak_pointer(G_OBJECT(priv->win), (gpointer *)&priv->win);

        /* check if the window should be destoryed or not on close */
        g_signal_connect(priv->win, "delete-event", G_CALLBACK(on_close_window), client);

        /* if we didn't launch with the '-r' (--restore-last-window-state) option then force the
         * show-main-window to true */
        if (!priv->restore_window_state)
            ring_window_show(client);
        show_main_window_toggled(client);
        g_signal_connect_swapped(priv->settings, "changed::show-main-window", G_CALLBACK(show_main_window_toggled), client);

        // track sys icon state
        g_signal_connect(priv->settings, "changed::show-status-icon", G_CALLBACK(systray_toggled), client);
        systray_toggled(priv->settings, "show-status-icon", client);
    } else {
        // activate not being called for the first time, force showing of main window
        ring_window_show(client);
    }
}

static void
ring_client_open(GApplication *app, GFile **file, gint /*arg3*/, const gchar* /*arg4*/)
{
    ring_client_activate(app);

    if (strcmp(g_file_get_uri_scheme(*file), "ring") == 0) {
        const char * call_id = g_file_get_basename(*file);
        std::regex format {"^[[:xdigit:]]{40}$"};

        if (std::regex_match(call_id, format)) {
            auto cm = std::unique_ptr<TemporaryContactMethod>(new TemporaryContactMethod);
            cm->setUri(URI(QString::fromStdString(call_id)));

            place_new_call(cm.get());
            cm.release();
        }
    }
}

#if USE_LIBNM

static void
log_connection_info(NMActiveConnection *connection)
{
    if (connection) {
        g_debug("primary network connection: %s, default: %s",
                nm_active_connection_get_uuid(connection),
                nm_active_connection_get_default(connection) ? "yes" : "no");
    } else {
        g_warning("no primary network connection detected, check network settings");
    }
}

static void
primary_connection_changed(NMClient *nm,  GParamSpec*, RingClient *self)
{
    auto priv = RING_CLIENT_GET_PRIVATE(self);
    auto connection = nm_client_get_primary_connection(nm);

    if (priv->primary_connection != connection) {
        /* make sure the connection really changed
         * on client start it seems to always emit the notify::primary-connection signal though it
         * hasn't changed */
        log_connection_info(connection);
        priv->primary_connection = connection;
        AccountModel::instance().slotConnectivityChanged();
    }
}

static void
nm_client_cb(G_GNUC_UNUSED GObject *source_object, GAsyncResult *result, RingClient *self)
{
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(self);

    GError* error = nullptr;
    if (auto nm_client = nm_client_new_finish(result, &error)) {
        priv->nm_client = nm_client;
        g_debug("NetworkManager client initialized, version: %s\ndaemon running: %s\nnnetworking enabled: %s",
                nm_client_get_version(nm_client),
                nm_client_get_manager_running(nm_client) ? "yes" : "no",
                nm_client_networking_get_enabled(nm_client) ? "yes" : "no");

        auto connection = nm_client_get_primary_connection(nm_client);
        log_connection_info(connection);
        priv->primary_connection = connection;

        /* We monitor the primary connection and notify the daemon to re-load its connections
         * (accounts, UPnP, ...) when it changes. For example, on most systems, if we have an
         * ethernet connection and then also connect to wifi, the primary connection will not change;
         * however it will change in the opposite case because an ethernet connection is preferred.
         */
        g_signal_connect(nm_client, "notify::primary-connection", G_CALLBACK(primary_connection_changed), self);

    } else {
        g_warning("error initializing NetworkManager client: %s", error->message);
        g_clear_error(&error);
    }
}

#endif /* USE_LIBNM */

static void
call_notifications_toggled(RingClient *self)
{
    auto priv = RING_CLIENT_GET_PRIVATE(self);

    if (g_settings_get_boolean(priv->settings, "enable-call-notifications")) {
        priv->call_notification = QObject::connect(
            &CallModel::instance(),
            &CallModel::incomingCall,
            [] (Call *call) { ring_notify_incoming_call(call); }
        );
    } else {
        QObject::disconnect(priv->call_notification);
    }
}

static void
chat_notifications_toggled(RingClient *self)
{
    auto priv = RING_CLIENT_GET_PRIVATE(self);

    if (g_settings_get_boolean(priv->settings, "enable-chat-notifications")) {
        priv->chat_notification = QObject::connect(
            &Media::RecordingModel::instance(),
            &Media::RecordingModel::newTextMessage,
            [self] (Media::TextRecording* t, ContactMethod* cm)
            { ring_notify_message(cm, t, self); }
        );
    } else {
        QObject::disconnect(priv->chat_notification);
    }
}

static void
selected_account_changed(RingClient *self)
{
    auto priv = RING_CLIENT_GET_PRIVATE(self);

    QByteArray account_id;

    const auto idx = AvailableAccountModel::instance().selectionModel()->currentIndex();
    if (idx.isValid()) {
        account_id = idx.data(static_cast<int>(Account::Role::Id)).toByteArray();
        if (account_id.isEmpty())
            g_warning("selected account id is empty; possibly newly created account");
    }

    g_settings_set_string(priv->settings, "selected-account", account_id.constData());
}

static void
restore_selected_account(RingClient *self)
{
    auto priv = RING_CLIENT_GET_PRIVATE(self);

    gchar *saved_account_id = g_settings_get_string(priv->settings, "selected-account");

    QModelIndex saved_idx;

    // try to find this account
    if (strlen(saved_account_id) > 0) {
        if (auto account = AccountModel::instance().getById(saved_account_id)) {
            saved_idx = AvailableAccountModel::instance().mapFromSource(account->index());
            if (!saved_idx.isValid())
                g_warning("could not select saved selected-account; it is possibly not enabled");
        } else {
            g_warning("could not find saved selected-account; it has possibly been deleted");
        }
    }

    g_free(saved_account_id);

    /* if no account selected; lets pick in the order of priority:
     * 1. the first available Ring account
     * 2. the first available SIP account
     * 5. none (can't pick not enabled accounts)
     */
    if (!saved_idx.isValid()) {
        if (auto account = AvailableAccountModel::instance().currentDefaultAccount(URI::SchemeType::RING)) {
            saved_idx = AvailableAccountModel::instance().mapFromSource(account->index());
        }
    }
    if (!saved_idx.isValid()) {
        if (auto account = AvailableAccountModel::instance().currentDefaultAccount(URI::SchemeType::SIP)) {
            saved_idx = AvailableAccountModel::instance().mapFromSource(account->index());
        }
    }

    AvailableAccountModel::instance().selectionModel()->setCurrentIndex(saved_idx, QItemSelectionModel::ClearAndSelect);
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
    } catch(const char * msg) {
        exception_dialog(msg);
        exit(1);
    } catch(QString& msg) {
        exception_dialog(msg.toLocal8Bit().constData());
        exit(1);
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
    GlobalInstances::setDBusErrorHandler(std::unique_ptr<Interfaces::DBusErrorHandler>(new Interfaces::DBusErrorHandler()));

    /* make sure all RING accounts have a display name... this basically makes sure
     * that all accounts created before the display name patch have a display name
     * set... a bit of a hack as this should maybe be done in LRC */
    force_ring_display_name();

    /* make sure basic number categories exist, in case user has no contacts
     * from which these would be automatically created
     */
    NumberCategoryModel::instance().addCategory("work", QVariant());
    NumberCategoryModel::instance().addCategory("home", QVariant());

    /* add backends */
    CategorizedHistoryModel::instance().addCollection<LocalHistoryCollection>(LoadOptions::FORCE_ENABLED);
    PersonModel::instance().addCollection<PeerProfileCollection>(LoadOptions::FORCE_ENABLED);
    ProfileModel::instance().addCollection<LocalProfileCollection>(LoadOptions::FORCE_ENABLED);

    /* fallback backend for vcards */
    PersonModel::instance().addCollection<FallbackPersonCollection>(LoadOptions::FORCE_ENABLED);

    /* bans collection */
    PersonModel::instance().addCollection<BansCollection>(LoadOptions::FORCE_ENABLED);

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

    /* GActions for settings */
    auto action_window_visible = g_settings_create_action(priv->settings, "show-main-window");
    g_action_map_add_action(G_ACTION_MAP(app), action_window_visible);

    /* add accelerators */
    ring_accelerators(RING_CLIENT(app));

    /* Bind GActions to the UserActionModel */
    UserActionModel* uam = CallModel::instance().userActionModel();
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
    QObject::connect(&CallModel::instance(), &CallModel::incomingCall,
        [app] (G_GNUC_UNUSED Call *call) {
            RingClient *client = RING_CLIENT(app);
            RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
            if (g_settings_get_boolean(priv->settings, "bring-window-to-front"))
                ring_window_show(client);
        }
    );

    /* enable notifications based on settings */
    ring_notify_init();
    call_notifications_toggled(client);
    chat_notifications_toggled(client);
    g_signal_connect_swapped(priv->settings, "changed::enable-call-notifications", G_CALLBACK(call_notifications_toggled), client);
    g_signal_connect_swapped(priv->settings, "changed::enable-chat-notifications", G_CALLBACK(chat_notifications_toggled), client);

#if USE_LIBNM
     /* monitor the network using libnm to notify the daemon about connectivity chagnes */
     nm_client_new_async(priv->cancellable, (GAsyncReadyCallback)nm_client_cb, client);
#endif

    /* keep track of the selected account */
    QObject::connect(AvailableAccountModel::instance().selectionModel(),
        &QItemSelectionModel::currentChanged,
        [app] () { selected_account_changed(RING_CLIENT(app)); }
    );
    restore_selected_account(RING_CLIENT(app));

    G_APPLICATION_CLASS(ring_client_parent_class)->startup(app);
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
    QObject::disconnect(priv->call_notification);
    QObject::disconnect(priv->chat_notification);

    /* free the QCoreApplication, which will destroy all libRingClient models
     * and thus send the Unregister signal over dbus to dring */
    delete priv->qtapp;

    /* free the copied cmd line args */
    g_strfreev(priv->argv);

    g_clear_object(&priv->settings);

    ring_notify_uninit();

#if USE_LIBNM
    /* clear NetworkManager client if it was used */
    g_clear_object(&priv->nm_client);
#endif

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

    /* add custom cmd line options */
    ring_client_add_options(G_APPLICATION(self));
}

static void
ring_client_class_init(RingClientClass *klass)
{
    G_APPLICATION_CLASS(klass)->startup = ring_client_startup;
    G_APPLICATION_CLASS(klass)->activate = ring_client_activate;
    G_APPLICATION_CLASS(klass)->open = ring_client_open;
    G_APPLICATION_CLASS(klass)->shutdown = ring_client_shutdown;
}

RingClient *
ring_client_new(int argc, char *argv[])
{
    RingClient *client = (RingClient *)g_object_new(ring_client_get_type(),
                                                    "application-id", RING_CLIENT_APP_ID,
                                                    "flags", G_APPLICATION_HANDLES_OPEN ,
                                                    NULL);

    /* copy the cmd line args before they get processed by the GApplication*/
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);
    priv->argc = argc;
    priv->argv = g_strdupv((gchar **)argv);

    return client;
}

GtkWindow  *
ring_client_get_main_window(RingClient *client)
{
    g_return_val_if_fail(IS_RING_CLIENT(client), NULL);
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    return (GtkWindow *)priv->win;
}

void
ring_client_set_restore_main_window_state(RingClient *client, gboolean restore)
{
    g_return_if_fail(IS_RING_CLIENT(client));
    RingClientPrivate *priv = RING_CLIENT_GET_PRIVATE(client);

    priv->restore_window_state = restore;
}
