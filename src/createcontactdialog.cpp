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

#include "createcontactdialog.h"

#include <contactmethod.h>
#include <personmodel.h>
#include <numbercategorymodel.h>
#include "utils/models.h"

struct _CreateContactDialog
{
    GtkDialog parent;
};

struct _CreateContactDialogClass
{
    GtkDialogClass parent_class;
};

typedef struct _CreateContactDialogPrivate CreateContactDialogPrivate;

struct _CreateContactDialogPrivate
{
    GtkWidget *button_save;
    GtkWidget *button_cancel;
    GtkWidget *combobox_addressbook;
    GtkWidget *entry_name;
    GtkWidget *combobox_detail;
    GtkWidget *entry_contactmethod;

    ContactMethod *cm;
};

G_DEFINE_TYPE_WITH_PRIVATE(CreateContactDialog, create_contact_dialog, GTK_TYPE_DIALOG);

#define CREATE_CONTACT_DIALOG_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CREATE_CONTACT_DIALOG_TYPE, CreateContactDialogPrivate))

static void
save_cb(G_GNUC_UNUSED GtkButton *button, CreateContactDialog *self)
{
    g_return_if_fail(IS_CREATE_CONTACT_DIALOG(self));
    CreateContactDialogPrivate *priv = CREATE_CONTACT_DIALOG_GET_PRIVATE(self);

    /* make sure that the entry is not empty */
    auto name = gtk_entry_get_text(GTK_ENTRY(priv->entry_name));
    if (!name or strlen(name) == 0) {
        g_warning("new contact must have a name");
        gtk_widget_grab_focus(priv->entry_name);
        return;
    }

    /* get the selected collection */
    GtkTreeIter iter;
    gtk_combo_box_get_active_iter(GTK_COMBO_BOX(priv->combobox_addressbook), &iter);
    auto addressbook_model = gtk_combo_box_get_model(GTK_COMBO_BOX(priv->combobox_addressbook));
    GValue value = G_VALUE_INIT;
    gtk_tree_model_get_value(addressbook_model, &iter, 1, &value);
    auto collection = (CollectionInterface *)g_value_get_pointer(&value);

    /* create a new person */
    Person *p = new Person(collection);
    p->setFormattedName(name);

    /* associate the new person with the contact method */
    Person::ContactMethods numbers;
    numbers << priv->cm;
    p->setContactMethods(numbers);

    PersonModel::instance()->addNewPerson(p, collection);

    gtk_dialog_response(GTK_DIALOG(self), GTK_RESPONSE_OK);
}

static void
cancel_cb(G_GNUC_UNUSED GtkButton *button, CreateContactDialog *self)
{
    gtk_dialog_response(GTK_DIALOG(self), GTK_RESPONSE_CANCEL);
}

static void
create_contact_dialog_init(CreateContactDialog *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    CreateContactDialogPrivate *priv = CREATE_CONTACT_DIALOG_GET_PRIVATE(self);

    /* model for the combobox for the choice of addressbooks */
    auto addressbook_model = gtk_list_store_new(
        2, G_TYPE_STRING, G_TYPE_POINTER
    );

    gtk_combo_box_set_model(GTK_COMBO_BOX(priv->combobox_addressbook),
                            GTK_TREE_MODEL(addressbook_model));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_addressbook),
                               renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->combobox_addressbook),
                                   renderer, "text", 0, NULL);

    /* get all the available contact backends (addressbooks) */
    const auto& collections = PersonModel::instance()->enabledCollections();
    for (int i = 0; i < collections.size(); ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(addressbook_model, &iter);
        gtk_list_store_set(addressbook_model, &iter,
                           0, collections.at(i)->name().toUtf8().constData(),
                           1, collections.at(i),
                           -1);
    }

    /* select the frist addressbook in the model by default */
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->combobox_addressbook), 0);

    /* model for the available details to choose from */
    gtk_combo_box_set_qmodel(GTK_COMBO_BOX(priv->combobox_detail),
                             (QAbstractItemModel *)NumberCategoryModel::instance(), NULL);

    /* set "home" as the default number category */
    const auto& idx = NumberCategoryModel::instance()->nameToIndex("home");
    if (idx.isValid())
        gtk_combo_box_set_active_index(GTK_COMBO_BOX(priv->combobox_detail), idx);

    /* set a default name in the name entry, and grab focus to select the text */
    gtk_entry_set_text(GTK_ENTRY(priv->entry_name), "New Contact");
    gtk_widget_grab_focus(GTK_WIDGET(priv->entry_name));

    /* connect to the button signals */
    g_signal_connect(priv->button_save, "clicked", G_CALLBACK(save_cb), self);
    g_signal_connect(priv->button_cancel, "clicked", G_CALLBACK(cancel_cb), self);
}

static void
create_contact_dialog_dispose(GObject *object)
{
    G_OBJECT_CLASS(create_contact_dialog_parent_class)->dispose(object);
}

static void
create_contact_dialog_finalize(GObject *object)
{
    G_OBJECT_CLASS(create_contact_dialog_parent_class)->finalize(object);
}

static void
create_contact_dialog_class_init(CreateContactDialogClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = create_contact_dialog_finalize;
    G_OBJECT_CLASS(klass)->dispose = create_contact_dialog_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/cx/ring/RingGnome/createcontactdialog.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), CreateContactDialog, button_save);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), CreateContactDialog, button_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), CreateContactDialog, combobox_addressbook);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), CreateContactDialog, entry_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), CreateContactDialog, combobox_detail);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), CreateContactDialog, entry_contactmethod);
}

GtkWidget *
create_contact_dialog_new(ContactMethod *cm, GtkWidget *parent)
{
    g_return_val_if_fail(cm, NULL);

    if (parent && GTK_IS_WIDGET(parent)) {
        /* get parent window so we can center on it */
        parent = gtk_widget_get_toplevel(GTK_WIDGET(parent));
        if (!gtk_widget_is_toplevel(parent)) {
            parent = NULL;
            g_debug("could not get top level parent");
        }
    }

    gpointer self = g_object_new(CREATE_CONTACT_DIALOG_TYPE, "transient-for", parent, NULL);

    CreateContactDialogPrivate *priv = CREATE_CONTACT_DIALOG_GET_PRIVATE(self);

    priv->cm = cm;
    gtk_entry_set_text(GTK_ENTRY(priv->entry_contactmethod), cm->uri().toUtf8().constData());
    gtk_entry_set_width_chars(GTK_ENTRY(priv->entry_contactmethod), cm->uri().toUtf8().size());

    return (GtkWidget *)self;
}
