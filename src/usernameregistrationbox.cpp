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
    GtkBox parent;
};

struct _UsernameRegistrationBoxClass
{
    GtkBoxClass parent_class;
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
    QMetaObject::Connection registered_name_found;
    QString* username_waiting_for_lookup_result;
};

G_DEFINE_TYPE_WITH_PRIVATE(UsernameRegistrationBox, username_registration_box, GTK_TYPE_BOX);

#define USERNAME_REGISTRATION_BOX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), USERNAME_REGISTRATION_BOX_TYPE, UsernameRegistrationBoxPrivate))

static void
username_registration_box_dispose(GObject *object)
{
    G_OBJECT_CLASS(username_registration_box_parent_class)->dispose(object);
}

static void
username_registration_box_init(UsernameRegistrationBox *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
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
}

static gboolean
launch_lookup(UsernameRegistrationBox *view)
{
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    NameDirectory::instance().lookupName(QString(), QString(), *priv->username_waiting_for_lookup_result);
    return G_SOURCE_REMOVE;
}

static void
entry_username_changed(G_GNUC_UNUSED GtkEntry* entry, UsernameRegistrationBox *view)
{
    g_return_if_fail(IS_USERNAME_REGISTRATION_BOX(view));
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    //Show the spinner + disable button
    gtk_widget_hide(priv->icon_username_availability);
    gtk_widget_show(priv->spinner);
    gtk_spinner_start(GTK_SPINNER(priv->spinner));
    gtk_widget_set_sensitive(priv->button_register_username, FALSE);

    priv->username_waiting_for_lookup_result = new QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_username)));

    QObject::disconnect(priv->registered_name_found);
    priv->registered_name_found = QObject::connect(
        &NameDirectory::instance(),
        &NameDirectory::registeredNameFound,
        [=] (const QString& accountId, NameDirectory::LookupStatus status, const QString& address, const QString& name) {
            g_debug("Name lookup ended");
            //If this is the username we are waiting for, we can disconnect.
            if (name.compare(priv->username_waiting_for_lookup_result) == 0)
            {
                QObject::disconnect(priv->registered_name_found);
            }
            else
            {
                //Keep waiting...
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
                    gtk_label_set_text(GTK_LABEL(priv->label_status), g_strdup_printf(_("Username \"%s\" is not available"), name.toStdString().c_str()));
                    gtk_widget_show(priv->icon_username_availability);
                    break;
                }
                case NameDirectory::LookupStatus::NOT_FOUND:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                    gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "emblem-default", GTK_ICON_SIZE_SMALL_TOOLBAR);
                    gtk_widget_set_tooltip_text(priv->icon_username_availability, _("The entered username is available"));
                    gtk_label_set_text(GTK_LABEL(priv->label_status),g_strdup_printf(_("Username \"%s\" is available"), name.toStdString().c_str()));
                    gtk_widget_show(priv->icon_username_availability);
                    break;
                }
                case NameDirectory::LookupStatus::ERROR:
                {
                    gtk_widget_set_sensitive(priv->button_register_username, FALSE);
                    gtk_image_set_from_icon_name(GTK_IMAGE(priv->icon_username_availability), "error", GTK_ICON_SIZE_SMALL_TOOLBAR);
                    gtk_widget_set_tooltip_text(priv->icon_username_availability, _("Failed to perform lookup"));
                    gtk_label_set_text(GTK_LABEL(priv->label_status), g_strdup_printf(_("Could not lookup username \"%s\""), name.toStdString().c_str()));
                    gtk_widget_show(priv->icon_username_availability);
                    break;
                }
            }
        }
    );

    //Start the lookup in a second so that the UI dosen't seem to freeze
    g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)launch_lookup, view, NULL);
}

static void
button_register_username_clicked(G_GNUC_UNUSED GtkButton* button, UsernameRegistrationBox *view)
{
    g_return_if_fail(IS_USERNAME_REGISTRATION_BOX(view));
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    const QString username = QString(gtk_entry_get_text(GTK_ENTRY(priv->entry_username)));

    if (username.isEmpty())
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

            priv->name_registration_ended = QObject::connect(
                priv->account,
                &Account::nameRegistrationEnded,
                [=] (NameDirectory::RegisterNameStatus status, const QString& name) {
                    g_debug("Name registration ended");

                    gtk_spinner_stop(GTK_SPINNER(priv->spinner));
                    gtk_widget_hide(priv->spinner);
                    gtk_widget_set_sensitive(priv->entry_username, TRUE);

                    switch(status)
                    {
                        case NameDirectory::RegisterNameStatus::SUCCESS:
                        {
                            gtk_label_set_text(GTK_LABEL(priv->label_status), NULL);
                            gtk_widget_set_sensitive(priv->button_register_username, TRUE);
                            g_object_set(G_OBJECT(priv->entry_username), "editable", FALSE, NULL);
                            gtk_widget_hide(priv->button_register_username);
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

             if (!priv->account->registerName(password, username))
             {
                 //TODO: handle this
                 g_debug("Could not initiate registerName operation");
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
build_view(UsernameRegistrationBox *view, bool register_button)
{
    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);

    auto registered_name = priv->account->registeredName();
    gtk_entry_set_text(GTK_ENTRY(priv->entry_username), registered_name.toLocal8Bit().constData());

    if (registered_name.isEmpty())
    {
        //Make the entry editable
        g_object_set(G_OBJECT(priv->entry_username), "editable", TRUE, NULL);
        g_signal_connect(priv->entry_username, "changed", G_CALLBACK(entry_username_changed), view);

        //Show the status icon
        gtk_widget_show(priv->icon_username_availability);

        //Show the register button
        if (register_button)
        {
            gtk_widget_show(priv->button_register_username);
            gtk_widget_set_sensitive(priv->button_register_username, FALSE);
            gtk_widget_set_tooltip_text(priv->button_register_username, _("Register this username on the blockchain"));
            g_signal_connect(priv->button_register_username, "clicked", G_CALLBACK(button_register_username_clicked), view);
        }

        //Show the status label
        gtk_widget_show(priv->label_status);
    }

    gtk_widget_hide(priv->button_register_username);
}

GtkWidget *
username_registration_box_new(Account* account, bool register_button)
{
    gpointer view = g_object_new(USERNAME_REGISTRATION_BOX_TYPE, NULL);

    UsernameRegistrationBoxPrivate *priv = USERNAME_REGISTRATION_BOX_GET_PRIVATE(view);
    priv->account = account;

    build_view(USERNAME_REGISTRATION_BOX(view), register_button);

    return (GtkWidget *)view;
}
