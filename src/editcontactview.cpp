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

#include "editcontactview.h"

#include <glib/gi18n.h>
#include <contactmethod.h>
#include <personmodel.h>
#include <numbercategorymodel.h>
#include "utils/models.h"

enum
{
    PERSON_SAVED,

    LAST_SIGNAL
};

struct _EditContactView
{
    GtkGrid parent;
};

struct _EditContactViewClass
{
    GtkGridClass parent_class;
};

typedef struct _EditContactViewPrivate EditContactViewPrivate;

struct _EditContactViewPrivate
{
    GtkWidget *button_save;
    GtkWidget *combobox_addressbook;
    GtkWidget *entry_name;
    GtkWidget *combobox_detail;
    GtkWidget *label_bestId;

    ContactMethod *cm;
    Person        *person;
};

static guint edit_contact_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE(EditContactView, edit_contact_view, GTK_TYPE_GRID);

#define EDIT_CONTACT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EDIT_CONTACT_VIEW_TYPE, EditContactViewPrivate))

static void
save_cb(EditContactView *self)
{
    g_return_if_fail(IS_EDIT_CONTACT_VIEW(self));
    EditContactViewPrivate *priv = EDIT_CONTACT_VIEW_GET_PRIVATE(self);

    auto name = gtk_entry_get_text(GTK_ENTRY(priv->entry_name));
    if (!priv->person) {
        /* make sure that the entry is not empty */
        if (!name or strlen(name) == 0) {
            g_warning("new contact must have a name");
            gtk_widget_grab_focus(priv->entry_name);
            return;
        }
    }

    /* get the selected number category */
    const auto& idx = gtk_combo_box_get_index(GTK_COMBO_BOX(priv->combobox_detail));
    if (idx.isValid()) {
        auto category = NumberCategoryModel::instance().getCategory(idx.data().toString());
        priv->cm->setCategory(category);
    }

    if (!priv->person) {
        /* get the selected collection */
        GtkTreeIter iter;
        gtk_combo_box_get_active_iter(GTK_COMBO_BOX(priv->combobox_addressbook), &iter);
        auto addressbook_model = gtk_combo_box_get_model(GTK_COMBO_BOX(priv->combobox_addressbook));
        GValue value = G_VALUE_INIT;
        gtk_tree_model_get_value(addressbook_model, &iter, 1, &value);
        auto collection = (CollectionInterface *)g_value_get_pointer(&value);

        /* create a new person */
        priv->person = new Person(collection);
        priv->person->setFormattedName(name);

        /* associate the new person with the contact method */
        Person::ContactMethods numbers;
        numbers << priv->cm;
        priv->person->setContactMethods(numbers);

        PersonModel::instance().addNewPerson(priv->person, collection);
    } else {
        auto numbers = priv->person->phoneNumbers();
        numbers << priv->cm;
        priv->person->setContactMethods(numbers);
        priv->person->save();
    }

    g_signal_emit(self, edit_contact_signals[PERSON_SAVED], 0);
}

static void
name_changed(GtkEntry *entry_name, EditContactView *self)
{
    g_return_if_fail(IS_EDIT_CONTACT_VIEW(self));
    EditContactViewPrivate *priv = EDIT_CONTACT_VIEW_GET_PRIVATE(self);

    auto name = gtk_entry_get_text(entry_name);
    /* make sure that the entry is not empty */
    if (!name or strlen(name) == 0) {
        gtk_widget_set_sensitive(priv->button_save, FALSE);
    } else {
        gtk_widget_set_sensitive(priv->button_save, TRUE);
    }
}

static void
edit_contact_view_init(EditContactView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    EditContactViewPrivate *priv = EDIT_CONTACT_VIEW_GET_PRIVATE(self);

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
    const auto& collections = PersonModel::instance().enabledCollections();
    for (int i = 0; i < collections.size(); ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(addressbook_model, &iter);
        gtk_list_store_set(addressbook_model, &iter,
                           0, collections.at(i)->name().toUtf8().constData(),
                           1, collections.at(i),
                           -1);
    }

    /* set the first addressbook which is not the "vCard" to be used, unless its
     * the only one */
    GtkTreeIter iter_to_select;
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(addressbook_model), &iter_to_select)) {
        GValue value = G_VALUE_INIT;
        gtk_tree_model_get_value(GTK_TREE_MODEL(addressbook_model), &iter_to_select, 1, &value);
        auto collection = (CollectionInterface *)g_value_get_pointer(&value);
        GtkTreeIter iter = iter_to_select;
        while ( (collection->name() == "vCard") && gtk_tree_model_iter_next(GTK_TREE_MODEL(addressbook_model), &iter)) {
            iter_to_select = iter;
            value = G_VALUE_INIT;
            gtk_tree_model_get_value(GTK_TREE_MODEL(addressbook_model), &iter, 1, &value);
            collection = (CollectionInterface *)g_value_get_pointer(&value);
        }
    }
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(priv->combobox_addressbook), &iter_to_select);

    /* model for the available details to choose from */
    gtk_combo_box_set_qmodel(GTK_COMBO_BOX(priv->combobox_detail),
                             (QAbstractItemModel *)&NumberCategoryModel::instance(), NULL);

    /* set "home" as the default number category */
    const auto& idx = NumberCategoryModel::instance().nameToIndex(C_("Phone number category", "home"));
    if (idx.isValid())
        gtk_combo_box_set_active_index(GTK_COMBO_BOX(priv->combobox_detail), idx);

    /* disable save button until name is entered */
    gtk_widget_set_sensitive(priv->button_save, FALSE);

    /* connect to the button signals */
    g_signal_connect_swapped(priv->button_save, "clicked", G_CALLBACK(save_cb), self);
    g_signal_connect(priv->entry_name, "changed", G_CALLBACK(name_changed), self);
}

static void
edit_contact_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(edit_contact_view_parent_class)->dispose(object);
}

static void
edit_contact_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(edit_contact_view_parent_class)->finalize(object);
}

static void
edit_contact_view_class_init(EditContactViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = edit_contact_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = edit_contact_view_dispose;

    edit_contact_signals[PERSON_SAVED] =
        g_signal_new("person-saved",
            G_OBJECT_CLASS_TYPE(G_OBJECT_CLASS(klass)),
            (GSignalFlags)(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
            0, /* class offset */
            NULL, /* accumulater */
            NULL, /* accu data */
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass),
                                                "/cx/ring/RingGnome/editcontactview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), EditContactView, button_save);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), EditContactView, combobox_addressbook);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), EditContactView, entry_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), EditContactView, combobox_detail);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS(klass), EditContactView, label_bestId);
}

/**
 * If a Person is specified, a view to edit the given Person will be created;
 * if no Person is given (NULL), then a new Person object will be created when
 * 'save' is clicked
 */
GtkWidget *
edit_contact_view_new(ContactMethod *cm, Person *p)
{
    g_return_val_if_fail(cm, NULL);

    gpointer self = g_object_new(EDIT_CONTACT_VIEW_TYPE, NULL);

    EditContactViewPrivate *priv = EDIT_CONTACT_VIEW_GET_PRIVATE(self);

    priv->cm = cm;
    auto uri_escaped = g_markup_printf_escaped("<b>%s</b>", cm->uri().toUtf8().constData());
    gtk_label_set_markup(GTK_LABEL(priv->label_bestId), uri_escaped);
    g_free(uri_escaped);

    /* use the primaryName as the suggested name (usually the display name), unless it is the same
     * as the uri */
    if (cm->primaryName() != cm->uri())
        gtk_entry_set_text(GTK_ENTRY(priv->entry_name), cm->primaryName().toUtf8().constData());

    if (p) {
        priv->person = p;

        /* select the proper addressbook and prevent it from being modified */
        if (p->collection() != NULL) {
            auto model = gtk_combo_box_get_model(GTK_COMBO_BOX(priv->combobox_addressbook));
            GtkTreeIter iter;
            if (gtk_tree_model_get_iter_first(model, &iter)) {
                GValue value = G_VALUE_INIT;
                gtk_tree_model_get_value(model, &iter, 1, &value);
                auto collection = (CollectionInterface *)g_value_get_pointer(&value);
                while (collection != p->collection() && gtk_tree_model_iter_next(model, &iter)) {
                    value = G_VALUE_INIT;
                    gtk_tree_model_get_value(model, &iter, 1, &value);
                    collection = (CollectionInterface *)g_value_get_pointer(&value);
                }
            }

            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(priv->combobox_addressbook), &iter);
        }
        gtk_widget_set_sensitive(priv->combobox_addressbook, FALSE);

        /* set the name of the person, and prevent it from being edited */
        gtk_entry_set_text(GTK_ENTRY(priv->entry_name), p->formattedName().toUtf8().constData());
        g_object_set(G_OBJECT(priv->entry_name), "editable", FALSE, NULL);
    }

    return (GtkWidget *)self;
}
