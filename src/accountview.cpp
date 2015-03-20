/*
 *  Copyright (C) 2015 Savoir-Faire Linux Inc.
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
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "accountview.h"

#include <gtk/gtk.h>
#include <accountmodel.h>
#include <audio/codecmodel.h>
#include <protocolmodel.h>
#include <QtCore/QItemSelectionModel>
#include "models/gtkqtreemodel.h"
#include "models/gtkqsortfiltertreemodel.h"
#include "models/activeitemproxymodel.h"
#include "accountgeneraltab.h"
#include "accountaudiotab.h"
#include "accountvideotab.h"

struct _AccountView
{
    GtkBox parent;
};

struct _AccountViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountViewPrivate AccountViewPrivate;

struct _AccountViewPrivate
{
    GtkWidget *treeview_account_list;
    GtkWidget *stack_account;
    GtkWidget *current_account_notebook;
    GtkWidget *button_remove_account;
    GtkWidget *button_add_account;
    GtkWidget *combobox_account_type;

    gint current_page; /* keeps tr#include "activeitemproxymodel.h"ack of current notebook page displayed */

    QMetaObject::Connection account_changed;
    QMetaObject::Connection codecs_changed;

    ActiveItemProxyModel *active_protocols;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountView, account_view, GTK_TYPE_BOX);

#define ACCOUNT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIEW_TYPE, AccountViewPrivate))

static void
account_view_dispose(GObject *object)
{
    AccountView *view = ACCOUNT_VIEW(object);
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->account_changed);
    QObject::disconnect(priv->codecs_changed);

    G_OBJECT_CLASS(account_view_parent_class)->dispose(object);
}

static QModelIndex
get_index_from_selection(GtkTreeSelection *selection)
{
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else {
        return QModelIndex();
    }
}

static void
cancel_account_changes(G_GNUC_UNUSED GtkButton *button, AccountView *view)
{
    g_debug("cancel changes");

    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    QModelIndex account_idx = get_index_from_selection(selection);
    g_return_if_fail(account_idx.isValid());


    /* reload the current account view unselectin and re-selecting it */
    GtkTreeIter iter;
    gtk_q_tree_model_source_index_to_iter(
        GTK_Q_TREE_MODEL(gtk_tree_view_get_model(GTK_TREE_VIEW(priv->treeview_account_list))),
        account_idx,
        &iter);

    QObject::disconnect(priv->account_changed);
    QObject::disconnect(priv->codecs_changed);
    gtk_tree_selection_unselect_all(selection);

    /* reload account */
    Account *account = AccountModel::instance()->getAccountByModelIndex(account_idx);
    account->performAction(Account::EditAction::CANCEL);
    /* TODO: clear the codecModel changes once this method is added */
    // account->codecModel()->cancel();

    /* re-select same item */
    gtk_tree_selection_select_iter(selection, &iter);
}

static void
apply_account_changes(G_GNUC_UNUSED GtkButton *button, Account *account)
{
    account->performAction(Account::EditAction::SAVE);
    /* TODO: save the codecModel changes once the method to clear them is added */
    // account->codecModel()->save();
}

static void
update_account_model_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex current = get_index_from_selection(selection);
    if (current.isValid())
        AccountModel::instance()->selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);
    else
        AccountModel::instance()->selectionModel()->clearCurrentIndex();
}

static void
account_selection_changed(GtkTreeSelection *selection, AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    GtkWidget *old_account_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account));

    /* keep track of the last tab displayed */
    if (priv->current_account_notebook)
        priv->current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(priv->current_account_notebook));

    if (priv->current_page < 0)
        priv->current_page = 0;

    QModelIndex account_idx = get_index_from_selection(selection);
    if (!account_idx.isValid()) {
        /* it nothing is slected, simply display something empty */
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show(empty_box);
        gtk_stack_add_named(GTK_STACK(priv->stack_account), empty_box, "placeholder");
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), empty_box);
        priv->current_account_notebook = NULL;
    } else {
        Account *account = AccountModel::instance()->getAccountByModelIndex(account_idx);

        /* build new account view */
        GtkWidget *hbox_account = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

        /* create account notebook */
        priv->current_account_notebook = gtk_notebook_new();
        gtk_box_pack_start(GTK_BOX(hbox_account), priv->current_account_notebook, TRUE, TRUE, 0);

        /* customize account view based on account */
        GtkWidget *general_tab = account_general_tab_new(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 general_tab,
                                 gtk_label_new("General"));
        GtkWidget *audio_tab = account_audio_tab_new(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 audio_tab,
                                 gtk_label_new("Audio"));
        GtkWidget *video_tab = account_video_tab_new(account);
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 video_tab,
                                 gtk_label_new("Video"));

        /* set the tab displayed to the same as the prev account selected */
        gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->current_account_notebook), priv->current_page);

        /* button box with cancel and apply buttons */
        GtkWidget *account_button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        GtkWidget *account_change_cancel = gtk_button_new_with_label("Cancel");
        GtkWidget *account_change_apply = gtk_button_new_with_label("Apply");

        if (account->editState() != Account::EditState::MODIFIED) {
            gtk_widget_set_sensitive(account_change_cancel, FALSE);
            gtk_widget_set_sensitive(account_change_apply, FALSE);
        }

        gtk_box_pack_end(GTK_BOX(account_button_box), account_change_cancel, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(account_button_box), account_change_apply, FALSE, FALSE, 0);
        gtk_widget_set_halign(account_button_box, GTK_ALIGN_END);
        gtk_box_set_spacing(GTK_BOX(account_button_box), 10);
        gtk_box_pack_start(GTK_BOX(hbox_account), account_button_box, FALSE, FALSE, 0);

        /* connect signals to buttons */
        QObject::disconnect(priv->account_changed);
        priv->account_changed = QObject::connect(
            account,
            &Account::changed,
            [=](Account* a) {
                if (a->editState() == Account::EditState::MODIFIED) {
                    /* account has been modified, enable buttons */
                    gtk_widget_set_sensitive(account_change_cancel, TRUE);
                    gtk_widget_set_sensitive(account_change_apply, TRUE);
                } else {
                    gtk_widget_set_sensitive(account_change_cancel, FALSE);
                    gtk_widget_set_sensitive(account_change_apply, FALSE);
                }
            }
        );

        /* add the following signal once the 'cancel' method is added to the contactModel */
        // QObject::disconnect(priv->codecs_changed);
        // priv->codecs_changed = QObject::connect(
        //     account->codecModel(),
        //     &CodecModel::dataChanged,
        //     [=]() {
        //         /* data changed in codec model, this probably happened because
        //          * codecs were (de)selected */
        //         gtk_widget_set_sensitive(account_change_cancel, TRUE);
        //         gtk_widget_set_sensitive(account_change_apply, TRUE);
        //     }
        // );

        g_signal_connect(account_change_cancel, "clicked", G_CALLBACK(cancel_account_changes), view);
        g_signal_connect(account_change_apply, "clicked", G_CALLBACK(apply_account_changes), account);

        gtk_widget_show_all(hbox_account);

        /* set the new account view as visible */
        char *account_view_name = g_strdup_printf("%p_account", account);
        gtk_stack_add_named(GTK_STACK(priv->stack_account), hbox_account, account_view_name);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), hbox_account);
        g_free(account_view_name);
    }

    /* remove the old account view */
    if (old_account_view)
        gtk_container_remove(GTK_CONTAINER(priv->stack_account), old_account_view);
}

static void
account_active_toggled(GtkCellRendererToggle *renderer, gchar *path, AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    /* we want to set it to the opposite of the current value */
    gboolean toggle = !gtk_cell_renderer_toggle_get_active(renderer);

    /* get iter which was clicked */
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->treeview_account_list));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, tree_path);

    /* get qmodelindex from iter and set the model data */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    if (idx.isValid()) {
        /* check if it is the IP2IP account, as we don't want to be able to disable it */
        QVariant alias = idx.data(static_cast<int>(Account::Role::Alias));
        if (strcmp(alias.value<QString>().toLocal8Bit().constData(), "IP2IP") != 0)
            AccountModel::instance()->setData(idx, QVariant(toggle), Qt::CheckStateRole);
    }
}

static gboolean
remove_account_dialog(Account *account)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            "Are you sure you want to delete account \"%s\"?",
                            account->alias().toLocal8Bit().constData());

    // gtk_window_set_title(GTK_WINDOW(dialog), _("Ring delete account"));
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
remove_account(G_GNUC_UNUSED GtkWidget *entry, AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    QModelIndex idx = get_index_from_selection(selection);

    if (idx.isValid()) {
        /* this is a destructive operation, ask the user if they are sure */
        Account *account = AccountModel::instance()->getAccountByModelIndex(idx);
        if (remove_account_dialog(account)) {
            AccountModel::instance()->remove(idx);
            AccountModel::instance()->save();
        }
    }
}

static void
add_account(G_GNUC_UNUSED GtkWidget *entry, AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    GtkTreeIter protocol_iter;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(priv->combobox_account_type), &protocol_iter)) {
        /* get the qmodelindex of the protocol */
        GtkTreeModel *protocol_model = gtk_combo_box_get_model(GTK_COMBO_BOX(priv->combobox_account_type));
        QModelIndex protocol_idx = gtk_q_sort_filter_tree_model_get_source_idx(
                                    GTK_Q_SORT_FILTER_TREE_MODEL(protocol_model),
                                    &protocol_iter);
        if (protocol_idx.isValid()) {
            protocol_idx = priv->active_protocols->mapToSource(protocol_idx);
            AccountModel::instance()->add(QString("New Account"), protocol_idx);
            AccountModel::instance()->save();
        }
    }
}

static void
account_view_init(AccountView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    /* account model */
    GtkQTreeModel *account_model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    account_model = gtk_q_tree_model_new(AccountModel::instance(), 4,
        Account::Role::Enabled, G_TYPE_BOOLEAN,
        Account::Role::Alias, G_TYPE_STRING,
        Account::Role::Proto, G_TYPE_INT,
        Account::Role::RegistrationState, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_account_list), GTK_TREE_MODEL(account_model));

    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes("Enabled", renderer, "active", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    g_signal_connect(renderer, "toggled", G_CALLBACK(account_active_toggled), view);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Alias", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Status", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    /* add an empty box to the account stack initially, otherwise there will
     * be no cool animation when the first account is selected */
    GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_show(empty_box);
    gtk_stack_add_named(GTK_STACK(priv->stack_account), empty_box, "placeholder");
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), empty_box);

    /* populate account type combo box */
    /* TODO: when to delete this model? */
    priv->active_protocols = new ActiveItemProxyModel((QAbstractItemModel *)AccountModel::instance()->protocolModel());

    GtkQSortFilterTreeModel *protocol_model = gtk_q_sort_filter_tree_model_new(
                                                (QSortFilterProxyModel *)priv->active_protocols,
                                                1,
                                                Qt::DisplayRole, G_TYPE_STRING);

    gtk_combo_box_set_model(GTK_COMBO_BOX(priv->combobox_account_type), GTK_TREE_MODEL(protocol_model));

    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_account_type), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->combobox_account_type), renderer,
                                   "text", 0, NULL);

    /* connect signals to and from the selection model of the account model */
    QObject::connect(
        AccountModel::instance()->selectionModel(),
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex & current, const QModelIndex & previous) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));

            /* first unselect the previous */
            if (previous.isValid()) {
                GtkTreeIter old_iter;
                if (gtk_q_tree_model_source_index_to_iter(account_model, previous, &old_iter)) {
                    gtk_tree_selection_unselect_iter(selection, &old_iter);
                } else {
                    g_warning("Trying to unselect invalid GtkTreeIter");
                }
            }

            /* select the current */
            if (current.isValid()) {
                GtkTreeIter new_iter;
                if (gtk_q_tree_model_source_index_to_iter(account_model, current, &new_iter)) {
                    gtk_tree_selection_select_iter(selection, &new_iter);
                } else {
                    g_warning("SelectionModel of CallModel changed to invalid QModelIndex?");
                }
            }
        }
    );

    GtkTreeSelection *account_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    g_signal_connect(account_selection, "changed", G_CALLBACK(update_account_model_selection), NULL);
    g_signal_connect(account_selection, "changed", G_CALLBACK(account_selection_changed), view);

    /* select the default protocol */
    QModelIndex protocol_idx = AccountModel::instance()->protocolModel()->selectionModel()->currentIndex();
    if (protocol_idx.isValid()) {
        protocol_idx = priv->active_protocols->mapFromSource(protocol_idx);
        GtkTreeIter protocol_iter;
        if (gtk_q_sort_filter_tree_model_source_index_to_iter(
                (GtkQSortFilterTreeModel *)protocol_model,
                protocol_idx,
                &protocol_iter)) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(priv->combobox_account_type), &protocol_iter);
        }
    }

    /* connect signals to add/remove accounts */
    g_signal_connect(priv->button_remove_account, "clicked", G_CALLBACK(remove_account), view);
    g_signal_connect(priv->button_add_account, "clicked", G_CALLBACK(add_account), view);
}

static void
account_view_class_init(AccountViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, treeview_account_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, stack_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, button_remove_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, button_add_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, combobox_account_type);
}

GtkWidget *
account_view_new(void)
{
    return (GtkWidget *)g_object_new(ACCOUNT_VIEW_TYPE, NULL);
}
