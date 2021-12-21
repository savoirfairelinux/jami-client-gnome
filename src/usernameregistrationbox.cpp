/*
 *  Copyright (C) 2016-2021 Savoir-faire Linux Inc.
 *  Author: Alexandre Viau <alexandre.viau@savoirfairelinux.com>
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

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>

// LRC
#include <api/newaccountmodel.h>
#include <namedirectory.h>

// Jami  Client
#include "usernameregistrationbox.h"

struct _UsernameRegistrationBox
{
    GtkGrid parent;
};

struct _UsernameRegistrationBoxClass
{
    GtkGridClass parent_class;
};

typedef struct _UsernameRegistrationBoxPrivate UsernameRegistrationBoxPrivate;

struct _UsernameRegistrationBoxPrivate
{
    AccountInfoPointer const *accountInfo_ = nullptr;
    bool withAccount;

    //Widgets
    GtkWidget *label_username;
    GtkWidget *frame_username;
    GtkWidget *entry_username;
    GtkWidget *spinner;
    GtkWidget *button_register_username;

    QMetaObject::Connection name_registration_ended;

    //Lookup variables
    QMetaObject::Connection registered_name_found;
    QString* username_waiting_for_lookup_result;
    gint lookup_timeout;
    gulong entry_changed;

    gboolean use_blockchain;
    gboolean show_register_button;
};

G_DEFINE_TYPE_WITH_PRIVATE(UsernameRegistrationBox, username_registration_box, GTK_TYPE_GRID);

#define USERNAME_REGISTRATION_BOX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), USERNAME_REGISTRATION_BOX_TYPE, UsernameRegistrationBoxPrivate))

/* signals */
enum {
    USERNAME_AVAILABILITY_CHANGED,
    USERNAME_REGISTRATION_COMPLETED,
    LAST_SIGNAL
};

static guint username_registration_box_signals[LAST_SIGNAL] = { 0 };

static void
username_registration_box_dispose(GObject *object)
{
    auto priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(object);

    QObject::disconnect(priv->registered_name_found);
    QObject::disconnect(priv->name_registration_ended);

    if (priv->lookup_timeout) {
        g_source_remove(priv->lookup_timeout);
        priv->lookup_timeout = 0;
    }

    G_OBJECT_CLASS(username_registration_box_parent_class)->dispose(object);
}

static void
show_error(UsernameRegistrationBox* view, bool show_error, gchar* tooltip)
{
    auto priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    g_return_if_fail(priv);

    GtkStyleContext* context_box;
    if (show_error) {
        context_box = gtk_widget_get_style_context(GTK_WIDGET(priv->entry_username));
        gtk_style_context_add_class(context_box, "box_error");
        context_box = gtk_widget_get_style_context(GTK_WIDGET(priv->spinner));
        gtk_style_context_add_class(context_box, "box_error");
        context_box = gtk_widget_get_style_context(GTK_WIDGET(priv->button_register_username));
        gtk_style_context_add_class(context_box, "box_error");
    } else {
        context_box = gtk_widget_get_style_context(GTK_WIDGET(priv->entry_username));
        gtk_style_context_remove_class(context_box, "box_error");
        context_box = gtk_widget_get_style_context(GTK_WIDGET(priv->spinner));
        gtk_style_context_remove_class(context_box, "box_error");
        context_box = gtk_widget_get_style_context(GTK_WIDGET(priv->button_register_username));
        gtk_style_context_remove_class(context_box, "box_error");
    }

    auto* image = show_error ?
        gtk_image_new_from_icon_name("dialog-error-symbolic", GTK_ICON_SIZE_BUTTON)
        : gtk_image_new_from_icon_name("emblem-ok-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(priv->button_register_username), image);
    gtk_widget_set_tooltip_text(priv->button_register_username, tooltip);
}

static void
username_registration_box_init(UsernameRegistrationBox *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    auto priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    priv->registered_name_found = QObject::connect(
        &NameDirectory::instance(),
        &NameDirectory::registeredNameFound,
        [=] (NameDirectory::LookupStatus status, const QString&, const QString& name) {
            // g_debug("Name lookup ended");

            if (!priv->use_blockchain)
                return;

            // compare to the current username entry
            const auto username_lookup = gtk_entry_get_text(GTK_ENTRY(priv->entry_username));
            if (name.compare(username_lookup) != 0) {
                // assume result is for older lookup
                return;
            }

            //We may now stop the spinner
            gtk_spinner_stop(GTK_SPINNER(priv->spinner));
            gtk_widget_hide(priv->spinner);

            if (priv->show_register_button)
                gtk_widget_show(priv->button_register_username);
            else
                gtk_widget_hide(priv->button_register_username);


            // We don't want to display any icon/label in case of empty lookup
            if (!username_lookup || !*username_lookup) {
                gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                show_error(view, false, _("Register this username on the name server"));
                return;
            }

            switch(status)
            {
                case NameDirectory::LookupStatus::SUCCESS:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    show_error(view, true, _("Username already taken"));
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, FALSE);
                    break;
                }
                case NameDirectory::LookupStatus::INVALID_NAME:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    show_error(view, true, _("Invalid username"));
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, FALSE);
                    break;
                }
                case NameDirectory::LookupStatus::NOT_FOUND:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                    show_error(view, false, _("Register this username on the name server"));
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, TRUE);
                    break;
                }
                case NameDirectory::LookupStatus::ERROR:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    show_error(view, true, _("Lookup unavailable, check your network connection"));
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, FALSE);
                    break;
                }
            }
        });
}

static void
username_registration_box_class_init(UsernameRegistrationBoxClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = username_registration_box_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/usernameregistrationbox.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, label_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, frame_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, entry_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, spinner);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, button_register_username);

    /* add signals */
    username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED] = g_signal_new("username-availability-changed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__BOOLEAN,
                 G_TYPE_NONE,
                 1, G_TYPE_BOOLEAN);
    username_registration_box_signals[USERNAME_REGISTRATION_COMPLETED] = g_signal_new("username-registration-completed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static gboolean
lookup_username(UsernameRegistrationBox *view)
{
    g_return_val_if_fail(IS_USERNAME_REGISTRATION_BOX(view), G_SOURCE_REMOVE);

    auto priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    const auto username = gtk_entry_get_text(GTK_ENTRY(priv->entry_username));

    if (priv->accountInfo_) {
        auto prop = (*priv->accountInfo_)->accountModel->getAccountConfig((*priv->accountInfo_)->id);
        NameDirectory::instance().lookupName(prop.RingNS.uri, username);
    } else {
        NameDirectory::instance().lookupName(QString(), username);
    }


    priv->lookup_timeout = 0;
    return G_SOURCE_REMOVE;
}

static void
entry_username_changed(UsernameRegistrationBox *view)
{
    g_return_if_fail(IS_USERNAME_REGISTRATION_BOX(view));
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    auto username = gtk_entry_get_text(GTK_ENTRY(priv->entry_username));

    // cancel any queued lookup
    if (priv->lookup_timeout) {
        g_source_remove(priv->lookup_timeout);
        priv->lookup_timeout = 0;
    }
    gtk_widget_set_sensitive(priv->button_register_username, FALSE);

    if (priv->use_blockchain) {

        show_error(view, false, _("Performing lookup…"));
        if (strlen(username) == 0) {
            gtk_widget_hide(priv->spinner);
            gtk_spinner_stop(GTK_SPINNER(priv->spinner));
        } else {
            gtk_widget_show(priv->spinner);
            gtk_spinner_start(GTK_SPINNER(priv->spinner));

            // queue lookup with a 500ms delay
            priv->lookup_timeout = g_timeout_add(500, (GSourceFunc)lookup_username, view);
        }
    } else {
        // not using blockchain, so don't care about username validity
        gtk_widget_hide(priv->spinner);
        gtk_spinner_stop(GTK_SPINNER(priv->spinner));
    }
}

static void
button_register_username_clicked(G_GNUC_UNUSED GtkButton* button, UsernameRegistrationBox *view)
{
    g_return_if_fail(IS_USERNAME_REGISTRATION_BOX(view));
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    if (!priv->use_blockchain)
        return;

    const auto username = gtk_entry_get_text(GTK_ENTRY(priv->entry_username));
    if (strlen(username) == 0)
    {
        //Error message should be displayed already.
        return;
    }

    auto show_password = true;
    if (*priv->accountInfo_) {
        auto accountId = (*priv->accountInfo_)->id;
        lrc::api::account::ConfProperties_t props;
        props = (*priv->accountInfo_)->accountModel->getAccountConfig(accountId);
        show_password = props.archiveHasPassword;
    }

    std::string password;
    gint result = GTK_RESPONSE_OK;

    if (show_password) {
        GtkWidget* password_dialog = gtk_message_dialog_new(
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))),
            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
            GTK_MESSAGE_QUESTION,
            GTK_BUTTONS_OK_CANCEL,
            _("Enter the password of your Jami account")
        );

        GtkWidget* entry_password = gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
        gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(password_dialog))), entry_password, FALSE, FALSE, 0);
        gtk_widget_show(entry_password);

        result = gtk_dialog_run(GTK_DIALOG(password_dialog));
        password = gtk_entry_get_text(GTK_ENTRY(entry_password));
        gtk_widget_destroy(password_dialog);
    }

    switch(result)
    {
        case GTK_RESPONSE_OK:
        {
            // Show the spinner
            gtk_widget_show(priv->spinner);
            gtk_spinner_start(GTK_SPINNER(priv->spinner));

            //Disable the entry
            gtk_widget_set_sensitive(priv->entry_username, FALSE);
            gtk_widget_set_sensitive(priv->button_register_username, FALSE);

            if (!(*priv->accountInfo_)->accountModel->registerName((*priv->accountInfo_)->id, password.c_str(), username))
            {
                gtk_spinner_stop(GTK_SPINNER(priv->spinner));
                gtk_widget_hide(priv->spinner);
                gtk_widget_set_sensitive(priv->entry_username, TRUE);

                gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                show_error(view, false, _("Registration in progress…"));
            }
            break;
        }
        case GTK_RESPONSE_CANCEL:
        {
            break;
        }
    }
}

static void
username_registration_dialog_error(const char* msg)
{
    g_warning("%s", msg);
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL),
                            GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                            "%s", msg);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Name registration Error"));
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

static void
build_view(UsernameRegistrationBox *view, gboolean register_button)
{
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    QString registered_name = {};
    if(priv->withAccount) {
        registered_name = (*priv->accountInfo_)->registeredName;
    }

    if (registered_name.isEmpty())
    {
        //Make the entry editable
        g_object_set(G_OBJECT(priv->entry_username), "editable", TRUE, NULL);
        priv->entry_changed = g_signal_connect_swapped(priv->entry_username, "changed", G_CALLBACK(entry_username_changed), view);

        //Show the register button
        if (register_button && priv->withAccount)
        {
            gtk_widget_show(priv->button_register_username);
            gtk_widget_set_sensitive(priv->button_register_username, FALSE);
            g_signal_connect(priv->button_register_username, "clicked", G_CALLBACK(button_register_username_clicked), view);
        }

        if (priv->withAccount) {
            priv->name_registration_ended = QObject::connect(
                (*priv->accountInfo_)->accountModel,
                &lrc::api::NewAccountModel::nameRegistrationEnded,
                [=] (const QString& accountId, lrc::api::account::RegisterNameStatus status, const QString& name) {
                    if (accountId != (*priv->accountInfo_)->id) return;
                    if (name == "") return;
                    gtk_spinner_stop(GTK_SPINNER(priv->spinner));
                    gtk_widget_hide(priv->spinner);
                    gtk_widget_set_sensitive(priv->entry_username, TRUE);

                    switch(status)
                    {
                    case lrc::api::account::RegisterNameStatus::SUCCESS:
                        {
                            // update the entry to what was registered, just to be sure
                            // don't do more lookups
                            if (priv->entry_changed != 0) {
                                g_signal_handler_disconnect(priv->entry_username, priv->entry_changed);
                                priv->entry_changed = 0;
                            }
                            gtk_label_set_text(GTK_LABEL(priv->label_username), qUtf8Printable(name));
                            gtk_widget_hide(priv->frame_username);
                            gtk_widget_show(priv->label_username);
                            gtk_widget_set_can_focus(priv->label_username, true);
                            g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_REGISTRATION_COMPLETED], 0);
                            break;
                        }
                    case lrc::api::account::RegisterNameStatus::INVALID_NAME:
                        gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                        username_registration_dialog_error(_("Unable to register name (Invalid name). Your username should contains between 3 and 32 alphanumerics characters (or underscore)."));
                        break;
                    case lrc::api::account::RegisterNameStatus::WRONG_PASSWORD:
                        gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                        username_registration_dialog_error(_("Unable to register name (Wrong password)."));
                        break;
                    case lrc::api::account::RegisterNameStatus::ALREADY_TAKEN:
                        gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                        username_registration_dialog_error(_("Unable to register name (Username already taken)."));
                        break;
                    case lrc::api::account::RegisterNameStatus::NETWORK_ERROR:
                        gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                        username_registration_dialog_error(_("Unable to register name (Network error) - check your connection."));
                        break;
                    }
                }
            );
        }
        gtk_widget_show(priv->frame_username);
        gtk_widget_hide(priv->label_username);
    } else {
        gtk_widget_hide(priv->frame_username);
        gtk_label_set_text(GTK_LABEL(priv->label_username), qUtf8Printable(registered_name));
        gtk_widget_show(priv->label_username);
    }

    if (priv->show_register_button)
        gtk_widget_show(priv->button_register_username);
    else
        gtk_widget_hide(priv->button_register_username);

    // CSS styles
    auto provider = gtk_css_provider_new();
    std::string css = ".box_error { background: #de8484; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
username_registration_box_new_empty(bool register_button)
{
    gpointer view = g_object_new(USERNAME_REGISTRATION_BOX_TYPE, NULL);

    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    priv->accountInfo_ = nullptr;
    priv->withAccount = false;
    priv->use_blockchain = true;
    priv->show_register_button = register_button;

    build_view(USERNAME_REGISTRATION_BOX(view), register_button);

    return (GtkWidget *)view;
}

GtkWidget *
username_registration_box_new(AccountInfoPointer const & accountInfo, bool register_button)
{
    gpointer view = g_object_new(USERNAME_REGISTRATION_BOX_TYPE, NULL);

    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    priv->accountInfo_ = &accountInfo;
    priv->withAccount = true;
    priv->use_blockchain = true;
    priv->show_register_button = register_button;

    build_view(USERNAME_REGISTRATION_BOX(view), register_button);

    return (GtkWidget *)view;
}

GtkEntry*
username_registration_box_get_entry(UsernameRegistrationBox* view)
{
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    return GTK_ENTRY(priv->entry_username);
}


void
username_registration_box_set_use_blockchain(UsernameRegistrationBox* view, gboolean use_blockchain)
{
    auto priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    if (priv->use_blockchain == use_blockchain)
        return;

    priv->use_blockchain = use_blockchain;

    entry_username_changed(view);

    if (use_blockchain) {
        if (priv->show_register_button)
            gtk_widget_show(priv->button_register_username);
        else
            gtk_widget_hide(priv->button_register_username);
    } else {
        gtk_widget_hide(priv->button_register_username);
    }
}
