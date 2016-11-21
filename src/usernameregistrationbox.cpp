/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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
#include <account.h>

// Ring Client
#include "usernameregistrationbox.h"
#include "utils/models.h"

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
    Account   *account;

    //Widgets
    GtkWidget *entry_username;
    GtkWidget *icon_username_availability;
    GtkWidget *spinner;
    GtkWidget *button_register_username;
    GtkWidget *label_status;

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
username_registration_box_init(UsernameRegistrationBox *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    auto priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    priv->registered_name_found = QObject::connect(
        &NameDirectory::instance(),
        &NameDirectory::registeredNameFound,
        [=] (const Account*, NameDirectory::LookupStatus status, const QString&, const QString& name) {
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

            switch(status)
            {
                case NameDirectory::LookupStatus::SUCCESS:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "error", GTK_ICON_SIZE_SMALL_TOOLBAR);
                    gtk_widget_set_tooltip_text(priv->icon_username_availability, _("The entered username is not available"));
                    gtk_label_set_text(GTK_LABEL(priv->label_status), _("Username is not available"));
                    gtk_widget_show(priv->icon_username_availability);
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, FALSE);
                    break;
                }
                case NameDirectory::LookupStatus::INVALID_NAME:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "error", GTK_ICON_SIZE_SMALL_TOOLBAR);
                    gtk_widget_set_tooltip_text(priv->icon_username_availability, _("The entered username is not valid"));
                    gtk_label_set_text(GTK_LABEL(priv->label_status), _("Username is not valid"));
                    gtk_widget_show(priv->icon_username_availability);
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, FALSE);
                    break;
                }
                case NameDirectory::LookupStatus::NOT_FOUND:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                    gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "emblem-default", GTK_ICON_SIZE_SMALL_TOOLBAR);
                    gtk_widget_set_tooltip_text(priv->icon_username_availability, _("The entered username is available"));
                    gtk_label_set_text(GTK_LABEL(priv->label_status), _("Username is available"));
                    gtk_widget_show(priv->icon_username_availability);
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, TRUE);
                    break;
                }
                case NameDirectory::LookupStatus::ERROR:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "error", GTK_ICON_SIZE_SMALL_TOOLBAR);
                    gtk_widget_set_tooltip_text(priv->icon_username_availability, _("Failed to perform lookup"));
                    gtk_label_set_text(GTK_LABEL(priv->label_status), _("Could not lookup username"));
                    gtk_widget_show(priv->icon_username_availability);
                    g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_AVAILABILITY_CHANGED], 0, FALSE);
                    break;
                }
            }
        }
    );
}

static void
username_registration_box_class_init(UsernameRegistrationBoxClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = username_registration_box_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/usernameregistrationbox.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, entry_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, icon_username_availability);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, spinner);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, button_register_username);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), UsernameRegistrationBox, label_status);

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

    NameDirectory::instance().lookupName(nullptr, QString(), username);


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

        if (strlen(username) == 0) {
            // don't lookup empty username
            gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "error", GTK_ICON_SIZE_SMALL_TOOLBAR);
            gtk_widget_show(priv->icon_username_availability);

            gtk_widget_hide(priv->spinner);
            gtk_spinner_stop(GTK_SPINNER(priv->spinner));
            gtk_label_set_text(GTK_LABEL(priv->label_status), "");
        } else {
            gtk_widget_hide(priv->icon_username_availability);
            gtk_widget_show(priv->spinner);
            gtk_spinner_start(GTK_SPINNER(priv->spinner));
            gtk_label_set_text(GTK_LABEL(priv->label_status), _("Looking up username availability..."));

            // queue lookup with a 500ms delay
            priv->lookup_timeout = g_timeout_add(500, (GSourceFunc)lookup_username, view);
        }
    } else {
        // not using blockchain, so don't care about username validity
        gtk_image_clear(GTK_IMAGE(priv->icon_username_availability));
        gtk_widget_show(priv->icon_username_availability);
        gtk_widget_set_size_request(priv->icon_username_availability, 16, 16); // ensure min size of empty icon
        gtk_widget_hide(priv->spinner);
        gtk_spinner_stop(GTK_SPINNER(priv->spinner));
        gtk_label_set_text(GTK_LABEL(priv->label_status), "");
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

    GtkWidget* password_dialog = gtk_message_dialog_new(
        GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(view))),
        (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_OK_CANCEL,
        "Enter the password of your Ring account"
    );

    GtkWidget* entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);
    gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(password_dialog))), entry_password, FALSE, FALSE, 0);
    gtk_widget_show(entry_password);

    gint result = gtk_dialog_run(GTK_DIALOG(password_dialog));
    const QString password = QString(gtk_entry_get_text(GTK_ENTRY(entry_password)));
    gtk_widget_destroy(password_dialog);

    switch(result)
    {
        case GTK_RESPONSE_OK:
        {
            // Show the spinner
            gtk_widget_hide(priv->icon_username_availability);
            gtk_widget_show(priv->spinner);
            gtk_spinner_start(GTK_SPINNER(priv->spinner));

            //Disable the entry
            gtk_widget_set_sensitive(priv->entry_username, FALSE);
            gtk_widget_set_sensitive(priv->button_register_username, FALSE);

            gtk_label_set_text(GTK_LABEL(priv->label_status), _("Registering username..."));

            if (!priv->account->registerName(password, username))
            {
                gtk_spinner_stop(GTK_SPINNER(priv->spinner));
                gtk_widget_hide(priv->spinner);
                gtk_widget_set_sensitive(priv->entry_username, TRUE);

                gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                gtk_label_set_text(GTK_LABEL(priv->label_status), _("Count not initiate name registration, try again."));
                gtk_widget_show(priv->icon_username_availability);
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
build_view(UsernameRegistrationBox *view, gboolean register_button)
{
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    QString registered_name;
    if(priv->account)
    {
        registered_name = priv->account->registeredName();
    }
    else
    {
        registered_name = QString();
    }
    gtk_entry_set_text(GTK_ENTRY(priv->entry_username), registered_name.toLocal8Bit().constData());

    if (registered_name.isEmpty())
    {
        //Make the entry editable
        g_object_set(G_OBJECT(priv->entry_username), "editable", TRUE, NULL);
        priv->entry_changed = g_signal_connect_swapped(priv->entry_username, "changed", G_CALLBACK(entry_username_changed), view);

        //Show the status icon
        gtk_widget_show(priv->icon_username_availability);

        //Show the register button
        if (register_button && priv->account)
        {
            gtk_widget_show(priv->button_register_username);
            gtk_widget_set_sensitive(priv->button_register_username, FALSE);
            gtk_widget_set_tooltip_text(priv->button_register_username, _("Register this username on the blockchain"));
            g_signal_connect(priv->button_register_username, "clicked", G_CALLBACK(button_register_username_clicked), view);
        }

        //Show the status label
        gtk_widget_show(priv->label_status);

        if (priv->account) {
            priv->name_registration_ended = QObject::connect(
                priv->account,
                &Account::nameRegistrationEnded,
                [=] (NameDirectory::RegisterNameStatus status, G_GNUC_UNUSED const QString& name) {
                    gtk_spinner_stop(GTK_SPINNER(priv->spinner));
                    gtk_widget_hide(priv->spinner);
                    gtk_widget_set_sensitive(priv->entry_username, TRUE);

                    switch(status)
                    {
                        case NameDirectory::RegisterNameStatus::SUCCESS:
                        {
                            // update the entry to what was registered, just to be sure
                            // don't do more lookups
                            if (priv->entry_changed != 0) {
                                g_signal_handler_disconnect(priv->entry_username, priv->entry_changed);
                                priv->entry_changed = 0;
                            }
                            gtk_entry_set_text(GTK_ENTRY(priv->entry_username), name.toUtf8().constData());
                            gtk_label_set_text(GTK_LABEL(priv->label_status), NULL);
                            g_object_set(G_OBJECT(priv->entry_username), "editable", FALSE, NULL);
                            gtk_widget_hide(priv->button_register_username);
                            g_signal_emit(G_OBJECT(view), username_registration_box_signals[USERNAME_REGISTRATION_COMPLETED], 0);
                            break;
                        }
                        case NameDirectory::RegisterNameStatus::INVALID_NAME:
                        {
                            gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                            gtk_label_set_text(GTK_LABEL(priv->label_status), _("Invalid username"));
                            gtk_widget_show(priv->icon_username_availability);
                            break;
                        }
                        case NameDirectory::RegisterNameStatus::WRONG_PASSWORD:
                        {
                            gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                            gtk_label_set_text(GTK_LABEL(priv->label_status), _("Bad password"));
                            gtk_widget_show(priv->icon_username_availability);
                            break;
                        }
                        case NameDirectory::RegisterNameStatus::ALREADY_TAKEN:
                        {
                            gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                            gtk_label_set_text(GTK_LABEL(priv->label_status), _("Username already taken"));
                            gtk_widget_show(priv->icon_username_availability);
                            break;
                        }
                        case NameDirectory::RegisterNameStatus::NETWORK_ERROR:
                        {
                            gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                            gtk_label_set_text(GTK_LABEL(priv->label_status), _("Network error"));
                            gtk_widget_show(priv->icon_username_availability);
                            break;
                        }
                    }
                }
            );
        }
    }
}

GtkWidget *
username_registration_box_new(Account* account, gboolean register_button)
{
    gpointer view = g_object_new(USERNAME_REGISTRATION_BOX_TYPE, NULL);

    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    priv->account = account;
    priv->show_register_button = register_button;
    priv->use_blockchain = TRUE; // default to true

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

    } else {
        gtk_widget_hide(priv->button_register_username);
    }
}
