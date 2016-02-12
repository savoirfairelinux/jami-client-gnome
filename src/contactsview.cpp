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

#include "contactsview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "models/gtkqsortfiltertreemodel.h"
#include <categorizedcontactmodel.h>
#include <personmodel.h>
#include "utils/calling.h"
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include <contactmethod.h>
#include "defines.h"
#include "utils/models.h"
#include <QtCore/QItemSelectionModel>
#include "numbercategory.h"

static constexpr const char* COPY_DATA_KEY = "copy_data";

struct _ContactsView
{
    GtkTreeView parent;
};

struct _ContactsViewClass
{
    GtkTreeViewClass parent_class;
};

typedef struct _ContactsViewPrivate ContactsViewPrivate;

struct _ContactsViewPrivate
{
    CategorizedContactModel::SortedProxy *q_sorted_proxy;
};

G_DEFINE_TYPE_WITH_PRIVATE(ContactsView, contacts_view, GTK_TYPE_TREE_VIEW);

#define CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), CONTACTS_VIEW_TYPE, ContactsViewPrivate))

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *tree_model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    /* check if this is a top level item (category),
     * or a bottom level item (contact method)
     * in this case we don't want to show a photo */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);
    if (depth == 2) {
        /* get person */
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(tree_model), iter);
        if (idx.isValid()) {
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            Person *c = var_c.value<Person *>();
            /* get photo */
            QVariant var_p = GlobalInstances::pixmapManipulator().contactPhoto(c, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
render_name_and_contact_method(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                               GtkCellRenderer *cell,
                               GtkTreeModel *tree_model,
                               GtkTreeIter *iter,
                               GtkTreeView *treeview)
{
    // check if this iter is selected
    gboolean is_selected = FALSE;
    if (GTK_IS_TREE_VIEW(treeview)) {
        auto selection = gtk_tree_view_get_selection(treeview);
        is_selected = gtk_tree_selection_iter_is_selected(selection, iter);
    }

    /**
     * If contact (person), show the name and the contact method (number)
     * underneath; if multiple contact methods, then indicate as such
     *
     * Otherwise just display the category or contact method
     */
    GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);

    gchar *text = NULL;

    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {
        QVariant var = idx.data(Qt::DisplayRole);
        if (depth == 1) {
            /* category */
            text = g_strdup_printf("<b>%s</b>", var.value<QString>().toUtf8().constData());
        } else if (depth == 2) {
            /* contact, check for contact methods */
            QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
            if (var_c.isValid()) {
                Person *c = var_c.value<Person *>();
                switch (c->phoneNumbers().size()) {
                    case 0:
                        text = g_strdup_printf("%s\n", c->formattedName().toUtf8().constData());
                        break;
                    case 1:
                    {
                        QString number;
                        QVariant var_n = c->phoneNumbers().first()->roleData(Qt::DisplayRole);
                        if (var_n.isValid())
                            number = var_n.value<QString>();

                        /* we want the color of the status text to be the default color if this iter is
                         * selected so that the treeview is able to invert it against the selection color */
                        if (is_selected) {
                            text = g_strdup_printf("%s\n %s",
                                                   c->formattedName().toUtf8().constData(),
                                                   number.toUtf8().constData());
                        } else {
                            text = g_strdup_printf("%s\n <span fgcolor=\"gray\">%s</span>",
                                                   c->formattedName().toUtf8().constData(),
                                                   number.toUtf8().constData());
                        }
                        break;
                    }
                    default:
                        /* more than one, for now don't show any of the contact methods */
                        text = g_strdup_printf("%s\n", c->formattedName().toUtf8().constData());
                        break;
                }
            } else {
                /* should never happen since depth 2 should always be a contact (person) */
                text = g_strdup_printf("%s", var.value<QString>().toUtf8().constData());
            }
        } else {
            auto var_object = idx.data(static_cast<int>(Ring::Role::Object));
            auto cm = var_object.value<ContactMethod *>();
            if (cm && cm->category()) {
                // try to get the number category, eg: "home"
                text = g_strdup_printf("(%s) %s", cm->category()->name().toUtf8().constData(),
                                                  cm->uri().toUtf8().constData());
            } else if (cm) {
                text = g_strdup_printf("%s", cm->uri().toUtf8().constData());
            } else {
                /* should only ever be a CM, so this should never execute */
                text = g_strdup_printf("%s", var.value<QString>().toUtf8().constData());
            }
        }
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);

    /* set the colour */
    if ( depth == 1) {
        /* nice blue taken from the ring logo */
        GdkRGBA rgba = {0.2, 0.75294117647, 0.82745098039, 0.1};
        g_object_set(G_OBJECT(cell), "cell-background-rgba", &rgba, NULL);
    } else {
        g_object_set(G_OBJECT(cell), "cell-background", NULL, NULL);
    }
}

static void
expand_if_child(G_GNUC_UNUSED GtkTreeModel *tree_model,
                GtkTreePath  *path,
                G_GNUC_UNUSED GtkTreeIter  *iter,
                GtkTreeView  *treeview)
{
    if (gtk_tree_path_get_depth(path) == 2)
        gtk_tree_view_expand_to_path(treeview, path);
}

static void
activate_contact_item(GtkTreeView *tree_view,
                      GtkTreePath *path,
                      G_GNUC_UNUSED GtkTreeViewColumn *column,
                      G_GNUC_UNUSED gpointer user_data)
{
    /* expand / contract row */
    if (gtk_tree_view_row_expanded(tree_view, path))
        gtk_tree_view_collapse_row(tree_view, path);
    else
        gtk_tree_view_expand_row(tree_view, path, FALSE);

    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

    /* get iter */
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
        if (idx.isValid()) {
            int depth = gtk_tree_path_get_depth(path);
            switch (depth) {
                case 0:
                case 1:
                    /* category, nothing to do */
                    break;
                case 2:
                {
                    /* contact (person), use contact method if there is only one */
                    QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
                    if (var_c.isValid()) {
                        Person *c = var_c.value<Person *>();
                        if (c->phoneNumbers().size() == 1) {
                            /* call with contact method */
                            place_new_call(c->phoneNumbers().first());
                        }
                    }
                    break;
                }
                default:
                {
                    /* contact method (or deeper) */
                    QVariant var_n = idx.data(static_cast<int>(ContactMethod::Role::Object));
                    if (var_n.isValid()) {
                        /* call with contat method */
                        place_new_call(var_n.value<ContactMethod *>());
                    }
                    break;
                }
            }
        }
    }
}

static void
copy_contact_info(GtkWidget *item, G_GNUC_UNUSED gpointer user_data)
{
    gpointer data = g_object_get_data(G_OBJECT(item), COPY_DATA_KEY);
    g_return_if_fail(data);
    gchar* text = (gchar *)data;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, text, -1);
}


static gboolean
remove_contact_dialog(GtkWidget *widget, Person *person)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            _("Are you sure you want to delete contact \"%s\"?"
                            " It will be removed from your system's addressbook."),
                            person->formattedName().toUtf8().constData());

    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(widget));
    if (gtk_widget_is_toplevel(parent)) {
        gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ON_PARENT);
    }

    switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
        case GTK_RESPONSE_OK:
            response = TRUE;
            break;
        default:
            response = FALSE;
            break;
    }

    gtk_widget_destroy(dialog);

    return response;
}

static void
remove_contact(GtkWidget *item, G_GNUC_UNUSED gpointer user_data)
{
    gpointer data = g_object_get_data(G_OBJECT(item), COPY_DATA_KEY);
    g_return_if_fail(data);
    Person* person = (Person *)data;
    if (remove_contact_dialog(item, person)) {
        person->remove();
    }
}

static gboolean
contacts_popup_menu(G_GNUC_UNUSED GtkWidget *widget, GdkEventButton *event, GtkTreeView *treeview)
{
    /* build popup menu when right clicking on contact item
     * user should be able to copy the contact's name or "number".
     * other functionality may be added later.
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    /* we don't want a popup menu for categories for now, so everything deeper
     * than one */
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return FALSE;

    GtkTreePath *path = gtk_tree_model_get_path(model, &iter);
    int depth = gtk_tree_path_get_depth(path);
    gtk_tree_path_free(path);

    if (depth < 2)
        return FALSE;

    /* deeper than a category, so create a menu */
    GtkWidget *menu = gtk_menu_new();
    QModelIndex idx = get_index_from_selection(selection);

    /* if depth == 2, it is a contact, offer to copy name, and if only one
     * contact method exists then also the "number",
     * if depth > 2, then its a contact method, so only offer to copy the number
     */
    if (depth == 2) {
        QVariant var_c = idx.data(static_cast<int>(Person::Role::Object));
        if (var_c.isValid()) {
            Person *c = var_c.value<Person *>();

            /* copy name */
            gchar *name = g_strdup_printf("%s", c->formattedName().toUtf8().constData());
            GtkWidget *copy_name_item = gtk_menu_item_new_with_mnemonic(_("_Copy name"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy_name_item);
            g_object_set_data_full(G_OBJECT(copy_name_item), COPY_DATA_KEY, name, (GDestroyNotify)g_free);
            g_signal_connect(copy_name_item,
                             "activate",
                             G_CALLBACK(copy_contact_info),
                             NULL);

            /* copy number if there is only one */
            if (c->phoneNumbers().size() == 1) {
                gchar *number = g_strdup_printf("%s",c->phoneNumbers().first()->uri().toUtf8().constData());
                GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
                gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
                g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
                g_signal_connect(item,
                                "activate",
                                G_CALLBACK(copy_contact_info),
                                NULL);
            }

            /* delete contact */
            GtkWidget *remove_contact_item = gtk_menu_item_new_with_mnemonic(_("_Remove contact"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), remove_contact_item);
            g_object_set_data_full(G_OBJECT(remove_contact_item), COPY_DATA_KEY, c, (GDestroyNotify)g_free);
            g_signal_connect(remove_contact_item,
                             "activate",
                             G_CALLBACK(remove_contact),
                             NULL);

        }
    } else if (depth > 2) {
        /* copy number */
        QVariant var_n = idx.data(static_cast<int>(ContactMethod::Role::Object));
        if (var_n.isValid()) {
            ContactMethod *n = var_n.value<ContactMethod *>();
            gchar *number = g_strdup_printf("%s",n->uri().toUtf8().constData());
            GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
            g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
            g_signal_connect(item,
                             "activate",
                             G_CALLBACK(copy_contact_info),
                             NULL);
        }
    }

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return TRUE; /* we handled the event */
}

static void
contacts_view_init(ContactsView *self)
{
    ContactsViewPrivate *priv = CONTACTS_VIEW_GET_PRIVATE(self);

    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(self), FALSE);

    /* disable default search, we will handle it ourselves;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(self), FALSE);

    /* initial set up to be categorized by name and sorted alphabetically */
    priv->q_sorted_proxy = &CategorizedContactModel::SortedProxy::instance();
    CategorizedContactModel::instance().setUnreachableHidden(true);

    /* for now we always want to sort by ascending order */
    priv->q_sorted_proxy->model()->sort(0);

    /* select default category (the first one, which is by name) */
    priv->q_sorted_proxy->categorySelectionModel()->setCurrentIndex(
        priv->q_sorted_proxy->categoryModel()->index(0, 0),
        QItemSelectionModel::ClearAndSelect);

    GtkQSortFilterTreeModel *contact_model = gtk_q_sort_filter_tree_model_new(
        priv->q_sorted_proxy->model(),
        1,
        Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(self), GTK_TREE_MODEL(contact_model));

    /* photo and name/contact method column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);

    /* photo renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_contact_photo,
        NULL,
        NULL);

    /* name and contact method renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_contact_method,
        self,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(self), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(self));
    g_signal_connect(contact_model, "row-inserted", G_CALLBACK(expand_if_child), self);
    g_signal_connect(self, "button-press-event", G_CALLBACK(contacts_popup_menu), self);
    g_signal_connect(self, "row-activated", G_CALLBACK(activate_contact_item), NULL);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
contacts_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(contacts_view_parent_class)->dispose(object);
}

static void
contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(contacts_view_parent_class)->finalize(object);
}

static void
contacts_view_class_init(ContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = contacts_view_dispose;
}

GtkWidget *
contacts_view_new()
{
    gpointer self = g_object_new(CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
