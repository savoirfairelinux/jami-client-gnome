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

#include "menus.h"

#include <contactmethod.h>
#include "../choosecontactdialog.h"

/**
 * checks if the given contact method is already associated with a contact
 */
gboolean
contact_method_has_contact(ContactMethod *cm)
{
    g_return_val_if_fail(cm, FALSE);

    return cm->contact() != NULL;
}

static void
choose_contact(GtkWidget *item, ContactMethod *contactmethod)
{
    // we get the parent widget which should be stored in the item object
    GtkWidget *parent = GTK_WIDGET(g_object_get_data(G_OBJECT(item), "parent-widget"));
    auto dialog = choose_contact_dialog_new(contactmethod, parent);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

/**
 * creates a menu item allowing the adition of a contact method to a contact
 */
GtkWidget * menu_item_add_to_contact(ContactMethod *cm, GtkWidget *parent)
{
    g_return_val_if_fail(cm, NULL);

    auto add_to = gtk_menu_item_new_with_mnemonic("_Add to contact");

    /* save the parent widget in the item object, so we can retrieve
     * it in the callback */
    g_object_set_data(G_OBJECT(add_to), "parent-widget", parent);

    g_signal_connect(add_to, "activate", G_CALLBACK(choose_contact), cm);

    return add_to;
}
