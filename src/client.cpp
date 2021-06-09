/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
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

#include "client.h"

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
#include <smartinfohub.h>
#include <globalinstances.h>

// Jami Client
#include "client_options.h"
#include "mainwindow.h"
#include "dialogs.h"
#include "native/pixbufmanipulator.h"
#include "native/dbuserrorhandler.h"
#include "notifier.h"
#include "config.h"
#include "utils/files.h"
#include "revision.h"

#if HAVE_AYATANAAPPINDICATOR
#include <libayatana-appindicator/app-indicator.h>
#elif HAVE_APPINDICATOR
#include <libappindicator/app-indicator.h>
#endif

struct _ClientClass
{
    GtkApplicationClass parent_class;
};

struct _Client
{
    GtkApplication parent;
};

typedef struct _ClientPrivate ClientPrivate;

struct _ClientPrivate {
    /* args */
    int    argc;
    char **argv;

    GSettings *settings;

    /* main window */
    GtkWidget        *win;
    /* for libRingClient */
    QCoreApplication *qtapp;
    /* UAM */
    QMetaObject::Connection uam_updated;

    std::unique_ptr<QTranslator> translator_lang;
    std::unique_ptr<QTranslator> translator_full;

    gboolean restore_window_state;

    gpointer systray_icon;
    GtkWidget *icon_menu;
};

/* this union is used to pass ints as pointers and vice versa for GAction parameters*/
typedef union _int_ptr_t
{
    int value;
    gint64 value64;
    gpointer ptr;
} int_ptr_t;

G_DEFINE_TYPE_WITH_PRIVATE(Client, client, GTK_TYPE_APPLICATION);

#define CLIENT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CLIENT_TYPE, ClientPrivate))


static void
exception_dialog(const char* msg)
{
    g_critical("%s", msg);
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                            _("Unable to initialize.\nMake sure the Jami daemon (jamid) is running.\nError: %s"),
                            msg);

    gtk_window_set_title(GTK_WINDOW(dialog), _("Jami Error"));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
accelerators(Client *client)
{
#if GTK_CHECK_VERSION(3,12,0)
    const gchar *quit_accels[2] = {"<Ctrl>Q", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.quit", quit_accels);
    const gchar *fullscreen_accels[2] = {"F11", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.toggle_fullscreen", fullscreen_accels);

    const gchar *accounts_accels[2] = {"<Ctrl>J", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.display_account_list", accounts_accels);

    const gchar *search_accels[2] = {"<Ctrl>F", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.search", search_accels);

    const gchar *conversations_list_accels[2] = {"<Ctrl>L", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.conversations_list", conversations_list_accels);
    const gchar *requests_list_accels[2] = {"<Ctrl>R", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.requests_list", requests_list_accels);

    const gchar *audio_call_accels[2] = {"<Ctrl><Shift>C", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.audio_call", audio_call_accels);
    const gchar *clear_history_accels[2] = {"<Ctrl><Shift>L", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.clear_history", clear_history_accels);
    const gchar *remove_conversation_accels[2] = {"<Ctrl><Shift>Delete", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.remove_conversation", remove_conversation_accels);
    const gchar *block_contact_accels[2] = {"<Ctrl><Shift>B", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.block_contact", block_contact_accels);
    const gchar *unblock_contact_accels[2] = {"<Ctrl><Shift>U", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.unblock_contact", unblock_contact_accels);
    const gchar *copy_contact_accels[2] = {"<Ctrl><Shift>J", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.copy_contact", copy_contact_accels);
    const gchar *add_contact_accels[2] = {"<Ctrl><Shift>A", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.add_contact", add_contact_accels);

    const gchar *accept_call_accels[2] = {"<Ctrl>Y", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.accept_call", accept_call_accels);
    const gchar *decline_call_accels[2] = {"<Ctrl>D", NULL};
    gtk_application_set_accels_for_action(GTK_APPLICATION(client), "app.decline_call", decline_call_accels);

#else
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>Q", "app.quit", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "F11", "app.toggle_fullscreen", NULL);


    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>J", "app.display_account_list", NULL);

    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>F", "app.search", NULL);

    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>L", "app.conversations_list", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>R", "app.requests_list", NULL);

    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>C", "app.audio_call", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>L", "app.clear_history", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>Delete", "app.remove_conversation", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>B", "app.block_contact", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>U", "app.unblock_contact", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>J", "app.copy_contact", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control><Shift>A", "app.add_contact", NULL);

    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>Y", "app.accept_call", NULL);
    gtk_application_add_accelerator(GTK_APPLICATION(client), "<Control>D", "app.decline_call", NULL);
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
    ClientPrivate *priv = CLIENT_GET_PRIVATE(user_data);
    gtk_widget_destroy(priv->win);
#endif
}

static void
action_about(G_GNUC_UNUSED GSimpleAction *simple,
             G_GNUC_UNUSED GVariant      *parameter,
             gpointer user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));
    ClientPrivate *priv = CLIENT_GET_PRIVATE(user_data);

    about_dialog(priv->win);
}

static void
exec_action(GSimpleAction *simple,
            G_GNUC_UNUSED GVariant      *parameter,
            gpointer user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));
    ClientPrivate *priv = CLIENT_GET_PRIVATE(user_data);

    GValue value = G_VALUE_INIT;
    g_value_init(&value, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(simple), "name", &value);
    if (!g_value_get_string(&value)) return;
    std::string name = g_value_get_string(&value);

    if (name == "display_account_list")
        main_window_display_account_list(MAIN_WINDOW(priv->win));
    else if (name == "search")
        main_window_search(MAIN_WINDOW(priv->win));
    else if (name == "conversations_list")
        main_window_conversations_list(MAIN_WINDOW(priv->win));
    else if (name == "requests_list")
        main_window_requests_list(MAIN_WINDOW(priv->win));
    else if (name == "audio_call")
        main_window_audio_call(MAIN_WINDOW(priv->win));
    else if (name == "clear_history")
        main_window_clear_history(MAIN_WINDOW(priv->win));
    else if (name == "remove_conversation")
        main_window_remove_conversation(MAIN_WINDOW(priv->win));
    else if (name == "block_contact")
        main_window_block_contact(MAIN_WINDOW(priv->win));
    else if (name == "unblock_contact")
        main_window_unblock_contact(MAIN_WINDOW(priv->win));
    else if (name == "copy_contact")
        main_window_copy_contact(MAIN_WINDOW(priv->win));
    else if (name == "add_contact")
        main_window_add_contact(MAIN_WINDOW(priv->win));
    else if (name == "accept_call")
        main_window_accept_call(MAIN_WINDOW(priv->win));
    else if (name == "decline_call")
        main_window_decline_call(MAIN_WINDOW(priv->win));
    else if (name == "toggle_fullscreen")
        main_window_toggle_fullscreen(MAIN_WINDOW(priv->win));
    else
        g_warning("Missing implementation for this action: %s", name.c_str());
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

static void
action_show_shortcuts(G_GNUC_UNUSED GSimpleAction *action, G_GNUC_UNUSED GVariant *parameter, gpointer user_data)
{
    g_return_if_fail(G_IS_APPLICATION(user_data));
    ClientPrivate *priv = CLIENT_GET_PRIVATE(user_data);

    GtkBuilder *builder = gtk_builder_new_from_resource("/net/jami/JamiGnome/help-overlay.ui");
    GtkWidget *overlay = GTK_WIDGET(gtk_builder_get_object (builder, "help_overlay"));

    gtk_window_set_transient_for(GTK_WINDOW(overlay), GTK_WINDOW(priv->win));
    gtk_widget_show(overlay);

    g_object_unref(builder);
}

static const GActionEntry actions[] = {
    {"accept", NULL, NULL, NULL, NULL, {0}},
    {"hangup", NULL, NULL, NULL, NULL, {0}},
    {"hold", NULL, NULL, "false", NULL, {0}},
    {"quit", action_quit, NULL, NULL, NULL, {0}},
    {"about", action_about, NULL, NULL, NULL, {0}},
    {"mute_audio", NULL, NULL, "false", NULL, {0}},
    {"mute_video", NULL, NULL, "false", NULL, {0}},
    {"record", NULL, NULL, "false", NULL, {0}},
    {"display-smartinfo", NULL, NULL, "false", toggle_smartinfo, {0}},
    {"display_account_list", exec_action, NULL, NULL, NULL, {0}},
    {"search", exec_action, NULL, NULL, NULL, {0}},
    {"conversations_list", exec_action, NULL, NULL, NULL, {0}},
    {"requests_list", exec_action, NULL, NULL, NULL, {0}},
    {"audio_call", exec_action, NULL, NULL, NULL, {0}},
    {"clear_history", exec_action, NULL, NULL, NULL, {0}},
    {"remove_conversation", exec_action, NULL, NULL, NULL, {0}},
    {"block_contact", exec_action, NULL, NULL, NULL, {0}},
    {"unblock_contact", exec_action, NULL, NULL, NULL, {0}},
    {"copy_contact", exec_action, NULL, NULL, NULL, {0}},
    {"add_contact", exec_action, NULL, NULL, NULL, {0}},
    {"accept_call", exec_action, NULL, NULL, NULL, {0}},
    {"decline_call", exec_action, NULL, NULL, NULL, {0}},
    {"show_shortcuts", action_show_shortcuts, NULL, NULL, NULL, {0}},
    {"toggle_fullscreen", exec_action, NULL, NULL, NULL, {0}},
};

static void
autostart_toggled(GSettings *settings, G_GNUC_UNUSED gchar *key, G_GNUC_UNUSED gpointer user_data)
{
    autostart_symlink(g_settings_get_boolean(settings, "start-on-login"));
}

static void
show_main_window_toggled(Client *client)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(priv->settings, "show-main-window")) {
        gtk_window_present(GTK_WINDOW(priv->win));
    } else {
        gtk_widget_hide(priv->win);
    }
}

static void
window_show(Client *client)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);
    g_settings_set_boolean(priv->settings, "show-main-window", TRUE);
}

static void
window_hide(Client *client)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);
    g_settings_set_boolean(priv->settings, "show-main-window", FALSE);
}

static gboolean
on_close_window(GtkWidget *window, G_GNUC_UNUSED GdkEvent *event, Client *client)
{
    g_return_val_if_fail(GTK_IS_WINDOW(window) && IS_CLIENT(client), FALSE);
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(priv->settings, "show-status-icon")) {
        /* we want to simply hide the window and keep the client running */
        auto closeWindow = main_window_can_close(MAIN_WINDOW(window));
        if (closeWindow) {
            window_hide(client);
            main_window_reset(MAIN_WINDOW(window));
        }
        return TRUE; /* do not propagate event */
    } else {
        /* we want to quit the application, so just propagate the event */
        return FALSE;
    }
}

#if !HAVE_APPINDICATOR

static void
popup_menu(GtkStatusIcon *self,
           guint          button,
           guint          when,
           Client    *client)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS // GtkStatusIcon is deprecated since 3.14, but we fallback on it
    gtk_menu_popup(GTK_MENU(priv->icon_menu), NULL, NULL, gtk_status_icon_position_menu, self, button, when);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

#endif

static void
init_systray(Client *client)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    // init menu
    if (!priv->icon_menu) {

        /* for some reason AppIndicator doesn't like the menu being built from a GMenuModel and/or
         * the GMenuModel being built from an xml resource. So we build the menu in code.
         */
        priv->icon_menu = gtk_menu_new();
        g_object_ref_sink(priv->icon_menu);

        auto item = gtk_check_menu_item_new_with_label(C_("In the status icon menu, toggle action to show or hide the Jami main window", "Show Jami"));
        gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "app.show-main-window");
        gtk_menu_shell_append(GTK_MENU_SHELL(priv->icon_menu), item);

        item = gtk_menu_item_new_with_label(_("Quit"));
        gtk_actionable_set_action_name(GTK_ACTIONABLE(item), "app.quit");
        gtk_menu_shell_append(GTK_MENU_SHELL(priv->icon_menu), item);

        gtk_widget_insert_action_group(priv->icon_menu, "app", G_ACTION_GROUP(client));
        gtk_widget_show_all(priv->icon_menu);
    }

#if HAVE_APPINDICATOR
    auto indicator = app_indicator_new("jami-gnome", "jami-gnome", APP_INDICATOR_CATEGORY_COMMUNICATIONS);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_title(indicator, JAMI_CLIENT_NAME);
    /* app indicator requires a menu */
    app_indicator_set_menu(indicator, GTK_MENU(priv->icon_menu));
    priv->systray_icon = indicator;
#else
    GError *error = NULL;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/net/jami/JamiGnome/jami-symbol-blue", &error);
    if (icon == nullptr) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else {
        G_GNUC_BEGIN_IGNORE_DEPRECATIONS // GtkStatusIcon is deprecated since 3.14, but we fallback on it
        auto status_icon = gtk_status_icon_new_from_pixbuf(icon);
        gtk_status_icon_set_title(status_icon, "jami-gnome");
        G_GNUC_END_IGNORE_DEPRECATIONS
        g_signal_connect_swapped(status_icon, "activate", G_CALLBACK(window_show), client);
        g_signal_connect(status_icon, "popup-menu", G_CALLBACK(popup_menu), client);
        priv->systray_icon = status_icon;
    }
#endif
}

static void
systray_toggled(GSettings *settings, const gchar *key, Client *client)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    if (g_settings_get_boolean(settings, key)) {
        if (!priv->systray_icon)
            init_systray(client);
    } else {
        if (priv->systray_icon)
            g_clear_object(&priv->systray_icon);
    }
}

static void
client_activate(GApplication *app)
{
    Client *client = CLIENT(app);
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    if (priv->win == NULL) {
        // activate being called for the first time
        priv->win = main_window_new(GTK_APPLICATION(app));

        /* make sure win is set to NULL when the window is destroyed */
        g_object_add_weak_pointer(G_OBJECT(priv->win), (gpointer *)&priv->win);

        /* check if the window should be destoryed or not on close */
        g_signal_connect(priv->win, "delete-event", G_CALLBACK(on_close_window), client);

        /* if we didn't launch with the '-r' (--restore-last-window-state) option then force the
         * show-main-window to true */
        if (!priv->restore_window_state)
            window_show(client);
        show_main_window_toggled(client);
        g_signal_connect_swapped(priv->settings, "changed::show-main-window", G_CALLBACK(show_main_window_toggled), client);

        // track sys icon state
        g_signal_connect(priv->settings, "changed::show-status-icon", G_CALLBACK(systray_toggled), client);
        systray_toggled(priv->settings, "show-status-icon", client);
    } else {
        // activate not being called for the first time, force showing of main window
        window_show(client);
    }
}

// TODO add some args!
static void
client_open(GApplication *app, GFile ** /*file*/, gint /*arg3*/, const gchar* /*arg4*/)
{
    client_activate(app);

    // TODO migrate place call at begining
}

static void
client_startup(GApplication *app)
{
    Client *client = CLIENT(app);
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    g_message("Jami GNOME client version: %s", VERSION);
    g_message("git ref: %s", CLIENT_REVISION);

    /* make sure that the system corresponds to the autostart setting */
    autostart_symlink(g_settings_get_boolean(priv->settings, "start-on-login"));
    g_signal_connect(priv->settings, "changed::start-on-login", G_CALLBACK(autostart_toggled), NULL);

    /* init clutter */
    int clutter_error;
    if ((clutter_error = gtk_clutter_init(&priv->argc, &priv->argv)) != CLUTTER_INIT_SUCCESS) {
        g_error("Could not init clutter : %d\n", clutter_error);
        exit(1); /* the g_error above should normally cause the application to exit */
    }

    /* init libRingClient and make sure its connected to the dbus */
    try {
        priv->qtapp = new QCoreApplication(priv->argc, priv->argv);
        /* the call model will try to connect to jamid via dbus */
    } catch(const char * msg) {
        exception_dialog(msg);
        exit(1);
    } catch(QString& msg) {
        exception_dialog(msg.toLocal8Bit().constData());
        exit(1);
    }

    /* load translations from LRC */
    const auto locale_name = QLocale::system().name();
    const auto locale_lang = locale_name.split('_')[0];

    if (locale_name != locale_lang) {
        /* Install language first to have lowest priority */
        priv->translator_lang.reset(new QTranslator);
        if (priv->translator_lang->load(JAMI_CLIENT_INSTALL "/share/libringclient/translations/lrc_" + locale_lang)) {
            g_debug("installed translations for %s", locale_lang.toUtf8().constData());
            priv->qtapp->installTranslator(priv->translator_lang.get());
        }
    }

    priv->translator_full.reset(new QTranslator);
    if (priv->translator_full->load(JAMI_CLIENT_INSTALL "/share/libringclient/translations/lrc_" + locale_name)) {
        g_debug("installed translations for %s", locale_name.toUtf8().constData());
    }

    if (not priv->translator_lang and not priv->translator_full) {
        g_debug("could not load LRC translations for %s, %s",
                QLocale::languageToString(QLocale::system().language()).toUtf8().constData(),
                QLocale::countryToString(QLocale::system().country()).toUtf8().constData()
        );
    }

    /* init delegates */
    GlobalInstances::setPixmapManipulator(std::unique_ptr<Interfaces::PixbufManipulator>(new Interfaces::PixbufManipulator()));
    GlobalInstances::setDBusErrorHandler(std::unique_ptr<Interfaces::DBusErrorHandler>(new Interfaces::DBusErrorHandler()));

    /* Override theme since we don't have appropriate icons for a dark them (yet) */
    GtkSettings *gtk_settings = gtk_settings_get_default();
    g_object_set(G_OBJECT(gtk_settings), "gtk-application-prefer-dark-theme",
                 FALSE, NULL);
    /* enable button icons */
    g_object_set(G_OBJECT(gtk_settings), "gtk-button-images",
                 TRUE, NULL);

    /* enable sound (for notification) */
    g_object_set(G_OBJECT(gtk_settings), "gtk-enable-event-sounds",
                 TRUE, NULL);

    /* add GActions */
    g_action_map_add_action_entries(
        G_ACTION_MAP(app), actions, G_N_ELEMENTS(actions), app);

    /* GActions for settings */
    auto action_window_visible = g_settings_create_action(priv->settings, "show-main-window");
    g_action_map_add_action(G_ACTION_MAP(app), action_window_visible);

    /* add accelerators */
    accelerators(CLIENT(app));

    G_APPLICATION_CLASS(client_parent_class)->startup(app);
}

static void
client_shutdown(GApplication *app)
{
    Client *self = CLIENT(app);
    ClientPrivate *priv = CLIENT_GET_PRIVATE(self);

    gtk_widget_destroy(priv->win);

    QObject::disconnect(priv->uam_updated);

    /* free the QCoreApplication, which will destroy all libRingClient models
     * and thus send the Unregister signal over dbus to jamid */
    if (priv->qtapp) {
        delete priv->qtapp;
        priv->qtapp = nullptr;
    }

    /* free the copied cmd line args */
    g_strfreev(priv->argv);

    g_clear_object(&priv->settings);

    /* Chain up to the parent class */
    G_APPLICATION_CLASS(client_parent_class)->shutdown(app);
}

static void
client_init(Client *self)
{
    ClientPrivate *priv = CLIENT_GET_PRIVATE(self);

    priv->win = NULL;
    priv->qtapp = NULL;
    priv->settings = g_settings_new_full(get_settings_schema(), NULL, NULL);

    /* add custom cmd line options */
    client_add_options(G_APPLICATION(self));
}

static void
client_class_init(ClientClass *klass)
{
    G_APPLICATION_CLASS(klass)->startup = client_startup;
    G_APPLICATION_CLASS(klass)->activate = client_activate;
    G_APPLICATION_CLASS(klass)->open = client_open;
    G_APPLICATION_CLASS(klass)->shutdown = client_shutdown;
}

Client*
client_new(int argc, char *argv[])
{
    Client *client = (Client *)g_object_new(client_get_type(),
                                                    "application-id", JAMI_CLIENT_APP_ID,
                                                    "flags", G_APPLICATION_HANDLES_OPEN ,
                                                    NULL);

    /* copy the cmd line args before they get processed by the GApplication*/
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);
    priv->argc = argc;
    priv->argv = g_strdupv((gchar **)argv);

    return client;
}

GtkWindow*
client_get_main_window(Client *client)
{
    g_return_val_if_fail(IS_CLIENT(client), NULL);
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    return (GtkWindow *)priv->win;
}

void
client_set_restore_main_window_state(Client *client, gboolean restore)
{
    g_return_if_fail(IS_CLIENT(client));
    ClientPrivate *priv = CLIENT_GET_PRIVATE(client);

    priv->restore_window_state = restore;
}
