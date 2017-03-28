/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#include "contactpopupmenu.h"

// GTK+ related
#include <glib/gi18n.h>

// LRC
#include <contactmethod.h>
#include <person.h>
#include <numbercategory.h>
#include <call.h>

// Ring client
#include "utils/calling.h"
#include "models/gtkqtreemodel.h"
#include "utils/menus.h"

static constexpr const char* COPY_DATA_KEY = "copy_data";

struct _ContactPopupMenu
{
    GtkMenu parent;
};

struct _ContactPopupMenuClass
{
    GtkMenuClass parent_class;
};

typedef struct _ContactPopupMenuPrivate ContactPopupMenuPrivate;

struct _ContactPopupMenuPrivate
{
    GtkTreeView *treeview;
};

G_DEFINE_TYPE_WITH_PRIVATE(ContactPopupMenu, contact_popup_menu, GTK_TYPE_MENU);

#define CONTACT_POPUP_MENU_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACT_POPUP_MENU_TYPE, ContactPopupMenuPrivate))

static void
copy_contact_info(GtkWidget *item, G_GNUC_UNUSED gpointer user_data)
{
    gpointer data = g_object_get_data(G_OBJECT(item), COPY_DATA_KEY);
    g_return_if_fail(data);
    gchar* text = (gchar *)data;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, text, -1);
}

static void
call_contactmethod(G_GNUC_UNUSED GtkWidget *item, ContactMethod *cm)
{
    g_return_if_fail(cm);
    place_new_call(cm);
}

static void
remove_contact(GtkWidget *item, Person *person)
{
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            _("Are you sure you want to delete contact \"%s\"?"
                            " It will be removed from your system's addressbook."),
                            person->formattedName().toUtf8().constData());

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(item));
    if (gtk_widget_is_toplevel(parent)) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        person->remove();
    }

    gtk_widget_destroy(dialog);
}

/**
 * Update the menu when the selected item in the treeview changes.
 */
static void
update(GtkTreeSelection *selection, ContactPopupMenu *self)
{
    ContactPopupMenuPrivate *priv = CONTACT_POPUP_MENU_GET_PRIVATE(self);

    /* clear the current menu */
    gtk_container_forall(GTK_CONTAINER(self), (GtkCallback)gtk_widget_destroy, nullptr);

    /* we always build a menu, however in some cases some or all of the items will be deactivated;
     * we prefer this to having an empty menu because GTK+ behaves weird in the empty menu case
     */
    auto call_item = gtk_menu_item_new_with_mnemonic(_("_Call"));
    gtk_widget_set_sensitive(GTK_WIDGET(call_item), FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(self), call_item);
    auto copy_name_item = gtk_menu_item_new_with_mnemonic(_("_Copy name"));
    gtk_widget_set_sensitive(GTK_WIDGET(copy_name_item), FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(self), copy_name_item);
    auto copy_number_item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
    gtk_widget_set_sensitive(GTK_WIDGET(copy_number_item), FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(self), copy_number_item);
    auto add_to_contact_item = gtk_menu_item_new_with_mnemonic(_("_Add to contact"));
    gtk_widget_set_sensitive(GTK_WIDGET(add_to_contact_item), FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(self), add_to_contact_item);
    auto remove_contact_item = gtk_menu_item_new_with_mnemonic(_("_Remove contact"));
    gtk_widget_set_sensitive(GTK_WIDGET(remove_contact_item), FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(self), remove_contact_item);

    /* show all items */
    gtk_widget_show_all(GTK_WIDGET(self));

    GtkTreeIter iter;
    GtkTreeModel *model;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return;

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);

    auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
    auto object = idx.data(static_cast<int>(Ring::Role::Object));
    if (!type.isValid() || !object.isValid())
        return; // not a valid Ring::Role::Object, so nothing to do

    /* call */
    switch (type.value<Ring::ObjectType>()) {
        case Ring::ObjectType::Person:
        {
            /* possiblity for multiple numbers */
            auto cms = object.value<Person *>()->phoneNumbers();
            if (cms.size() == 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(call_item), TRUE);
                g_signal_connect(call_item,
                                 "activate",
                                 G_CALLBACK(call_contactmethod),
                                 cms.at(0));
            } else if (cms.size() > 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(call_item), TRUE);
                auto call_menu = gtk_menu_new();
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(call_item), call_menu);
                for (int i = 0; i < cms.size(); ++i) {
                    gchar *number = nullptr;
                    if (cms.at(i)->category()) {
                        // try to get the number category, eg: "home"
                        number = g_strdup_printf("(%s) %s", cms.at(i)->category()->name().toUtf8().constData(),
                                                          cms.at(i)->uri().toUtf8().constData());
                    } else {
                        number = g_strdup_printf("%s", cms.at(i)->uri().toUtf8().constData());
                    }
                    auto item = gtk_menu_item_new_with_label(number);
                    g_free(number);
                    gtk_menu_shell_append(GTK_MENU_SHELL(call_menu), item);
                    g_signal_connect(item,
                                     "activate",
                                     G_CALLBACK(call_contactmethod),
                                     cms.at(i));
                }
            }
        }
        break;
        case Ring::ObjectType::ContactMethod:
        {
            gtk_widget_set_sensitive(GTK_WIDGET(call_item), TRUE);
            auto cm = object.value<ContactMethod *>();
            g_signal_connect(call_item,
                             "activate",
                             G_CALLBACK(call_contactmethod),
                             cm);
        }
        break;
        case Ring::ObjectType::Call:
        {
            gtk_widget_set_sensitive(GTK_WIDGET(call_item), TRUE);
            auto call = object.value<Call *>();
            g_signal_connect(call_item,
                             "activate",
                             G_CALLBACK(call_contactmethod),
                             call->peerContactMethod());
        }
        break;
        case Ring::ObjectType::Media:
        case Ring::ObjectType::Certificate:
        case Ring::ObjectType::ContactRequest:
        // nothing to do for now
        case Ring::ObjectType::COUNT__:
        break;
    }

    /* copy name */
    QVariant name_var = idx.data(static_cast<int>(Ring::Role::Name));
    if (name_var.isValid()) {
        gtk_widget_set_sensitive(GTK_WIDGET(copy_name_item), TRUE);
        gchar *name = g_strdup_printf("%s", name_var.value<QString>().toUtf8().constData());
        g_object_set_data_full(G_OBJECT(copy_name_item), COPY_DATA_KEY, name, (GDestroyNotify)g_free);
        g_signal_connect(copy_name_item,
                         "activate",
                         G_CALLBACK(copy_contact_info),
                         NULL);
    }

    /* copy number(s) */
    switch (type.value<Ring::ObjectType>()) {
        case Ring::ObjectType::Person:
        {
            /* possiblity for multiple numbers */
            auto cms = object.value<Person *>()->phoneNumbers();
            if (cms.size() == 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(copy_number_item), TRUE);
                gchar *number = g_strdup_printf("%s",cms.at(0)->uri().toUtf8().constData());
                g_object_set_data_full(G_OBJECT(copy_number_item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                g_signal_connect(copy_number_item,
                                 "activate",
                                 G_CALLBACK(copy_contact_info),
                                 NULL);
            } else if (cms.size() > 1) {
                gtk_widget_set_sensitive(GTK_WIDGET(copy_number_item), TRUE);
                auto copy_menu = gtk_menu_new();
                gtk_menu_item_set_submenu(GTK_MENU_ITEM(copy_number_item), copy_menu);
                for (int i = 0; i < cms.size(); ++i) {
                    auto number = g_strdup_printf("%s",cms.at(i)->uri().toUtf8().constData());
                    gchar *category_number = nullptr;
                    if (cms.at(i)->category()) {
                        // try to get the number category, eg: "home"
                        category_number = g_strdup_printf("(%s) %s",
                                                          cms.at(i)->category()->name().toUtf8().constData(),
                                                          number);
                    } else {
                        category_number = g_strdup_printf("%s", number);
                    }
                    auto item = gtk_menu_item_new_with_label(category_number);
                    g_free(category_number);
                    gtk_menu_shell_append(GTK_MENU_SHELL(copy_menu), item);
                    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                    g_signal_connect(item,
                                     "activate",
                                     G_CALLBACK(copy_contact_info),
                                     NULL);
                }
            }
        }
        break;
        case Ring::ObjectType::ContactMethod:
        case Ring::ObjectType::Call:
        {
            QVariant number_var = idx.data(static_cast<int>(Ring::Role::Number));
            if (number_var.isValid()) {
                gtk_widget_set_sensitive(GTK_WIDGET(copy_number_item), TRUE);
                gchar *number = g_strdup_printf("%s", number_var.value<QString>().toUtf8().constData());
                g_object_set_data_full(G_OBJECT(copy_number_item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                g_signal_connect(copy_number_item,
                                 "activate",
                                 G_CALLBACK(copy_contact_info),
                                 NULL);
            }
        }
        break;
        case Ring::ObjectType::Media:
        case Ring::ObjectType::Certificate:
        case Ring::ObjectType::ContactRequest:
        // nothing to do
        case Ring::ObjectType::COUNT__:
        break;
    }

    /* get rectangle to know where to draw the add to contact popup */
    GdkRectangle rect;
    auto path = gtk_tree_model_get_path(model, &iter);
    auto column = gtk_tree_view_get_column(priv->treeview, 0);
    gtk_tree_view_get_cell_area(priv->treeview, path, column, &rect);
    gtk_tree_view_convert_bin_window_to_widget_coords(priv->treeview, rect.x, rect.y, &rect.x, &rect.y);
    gtk_tree_path_free(path);

    /* add to contact - only offer to add CMs which are not already associated with a Person */
    switch (type.value<Ring::ObjectType>()) {
        case Ring::ObjectType::Person:
        // already a contact
        break;
        case Ring::ObjectType::ContactMethod:
        {
            auto cm = object.value<ContactMethod *>();
            if (!cm->contact()) {
                gtk_widget_set_sensitive(GTK_WIDGET(add_to_contact_item), TRUE);
                menu_item_add_to_contact(GTK_MENU_ITEM(add_to_contact_item), cm, GTK_WIDGET(priv->treeview), &rect);
            }
        }
        break;
        case Ring::ObjectType::Call:
        {
            auto cm = object.value<Call *>()->peerContactMethod();
            if (!cm->contact()) {
                gtk_widget_set_sensitive(GTK_WIDGET(add_to_contact_item), TRUE);
                menu_item_add_to_contact(GTK_MENU_ITEM(add_to_contact_item), cm, GTK_WIDGET(priv->treeview), &rect);
            }
        }
        break;
        case Ring::ObjectType::Media:
        case Ring::ObjectType::Certificate:
        case Ring::ObjectType::ContactRequest:
        // nothing to do
        case Ring::ObjectType::COUNT__:
        break;
    }

     /* remove contact */
     if (type.value<Ring::ObjectType>() == Ring::ObjectType::Person) {
         gtk_widget_set_sensitive(GTK_WIDGET(remove_contact_item), TRUE);
         auto person = object.value<Person *>();
         g_signal_connect(remove_contact_item,
                          "activate",
                          G_CALLBACK(remove_contact),
                          person);
     }

     /* show all items */
     gtk_widget_show_all(GTK_WIDGET(self));
}

static void
contact_popup_menu_init(G_GNUC_UNUSED ContactPopupMenu *self)
{
    // nothing to do
}

static void
contact_popup_menu_dispose(GObject *object)
{
    G_OBJECT_CLASS(contact_popup_menu_parent_class)->dispose(object);
}

static void
contact_popup_menu_finalize(GObject *object)
{
    G_OBJECT_CLASS(contact_popup_menu_parent_class)->finalize(object);
}

static void
contact_popup_menu_class_init(ContactPopupMenuClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = contact_popup_menu_finalize;
    G_OBJECT_CLASS(klass)->dispose = contact_popup_menu_dispose;
}

GtkWidget *
contact_popup_menu_new(GtkTreeView *treeview)
{
    gpointer self = g_object_new(CONTACT_POPUP_MENU_TYPE, NULL);
    ContactPopupMenuPrivate *priv = CONTACT_POPUP_MENU_GET_PRIVATE(self);

    priv->treeview = treeview;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(priv->treeview);
    g_signal_connect(selection, "changed", G_CALLBACK(update), self);

    // build the menu for the first time
    update(selection, CONTACT_POPUP_MENU(self));

    return (GtkWidget *)self;
}

gboolean
contact_popup_menu_show(ContactPopupMenu *self, GdkEventButton *event)
{
    /* check for right click */
    if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_SECONDARY ) {
        /* the menu will automatically get updated when the selection changes */
        gtk_menu_popup(GTK_MENU(self), NULL, NULL, NULL, NULL, event->button, event->time);
    }

    return GDK_EVENT_PROPAGATE; /* so that the item selection changes */
}
