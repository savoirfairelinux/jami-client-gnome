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
#include <glib/gi18n.h>
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
#include "accountadvancedtab.h"
#include "accountsecuritytab.h"
#include "dialogs.h"
#include <glib/gprintf.h>
#include "utils/models.h"

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

    gint current_page; /* keeps track of current notebook page displayed */

    ActiveItemProxyModel *active_protocols;
    QMetaObject::Connection protocol_selection_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountView, account_view, GTK_TYPE_BOX);

#define ACCOUNT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIEW_TYPE, AccountViewPrivate))

static void
account_view_dispose(GObject *object)
{
    AccountView *view = ACCOUNT_VIEW(object);
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->protocol_selection_changed);

    G_OBJECT_CLASS(account_view_parent_class)->dispose(object);
}

static void
account_view_finalize(GObject *object)
{
    AccountView *view = ACCOUNT_VIEW(object);
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    delete priv->active_protocols;

    G_OBJECT_CLASS(account_view_parent_class)->finalize(object);
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

static GtkWidget *
create_scrolled_account_view(GtkWidget *account_view)
{
    auto scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled), account_view);
    return scrolled;
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
        auto general_tab = create_scrolled_account_view(account_general_tab_new(account));
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 general_tab,
                                 gtk_label_new(Q_("Account settings|General")));
        auto audio_tab = create_scrolled_account_view(account_audio_tab_new(account));
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 audio_tab,
                                 gtk_label_new(Q_("Account settings|Audio")));
        auto video_tab = create_scrolled_account_view(account_video_tab_new(account));
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 video_tab,
                                 gtk_label_new(Q_("Account settings|Video")));
        auto advanced_tab = create_scrolled_account_view(account_advanced_tab_new(account));
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 advanced_tab,
                                 gtk_label_new(Q_("Account settings|Advanced")));
        auto security_tab = create_scrolled_account_view(account_security_tab_new(account));
        gtk_notebook_append_page(GTK_NOTEBOOK(priv->current_account_notebook),
                                 security_tab,
                                 gtk_label_new(Q_("Account settings|Security")));

        /* set the tab displayed to the same as the prev account selected */
        gtk_notebook_set_current_page(GTK_NOTEBOOK(priv->current_account_notebook), priv->current_page);

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
        if (strcmp(alias.value<QString>().toLocal8Bit().constData(), "IP2IP") != 0) {
            AccountModel::instance()->setData(idx, QVariant(toggle), Qt::CheckStateRole);
            /* save the account to apply the changed state right away */
            AccountModel::instance()->getAccountByModelIndex(idx)->performAction(Account::EditAction::SAVE);
        }
    }
}

static gboolean
remove_account_dialog(AccountView *view, Account *account)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            _("Are you sure you want to delete account \"%s\"?"),
                            account->alias().toLocal8Bit().constData());

    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(view));
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

static gboolean
save_account(GtkWidget *working_dialog)
{
    AccountModel::instance()->save();
    if (working_dialog)
        gtk_widget_destroy(working_dialog);

    return G_SOURCE_REMOVE;
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
        if (remove_account_dialog(view, account)) {
            /* show working dialog in case save operation takes time */
            GtkWidget *working = ring_dialog_working(GTK_WIDGET(view), NULL);
            gtk_window_present(GTK_WINDOW(working));

            AccountModel::instance()->remove(idx);

            /* now save the time it takes to transition the account view to the new account (300ms)
             * the save doesn't happen before the "working" dialog is presented
             * the timeout function should destroy the "working" dialog when done saving
             */
            g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)save_account, working, NULL);
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

            /* show working dialog in case save operation takes time */
            GtkWidget *working = ring_dialog_working(GTK_WIDGET(view), NULL);
            gtk_window_present(GTK_WINDOW(working));

            auto account = AccountModel::instance()->add(QString(_("New Account")), protocol_idx);
            if (account->protocol() == Account::Protocol::RING)
                account->setDisplayName(_("New Account"));

            /* now save after a short timeout to make sure that
             * the save doesn't happen before the "working" dialog is presented
             * the timeout function should destroy the "working" dialog when done saving
             */
            g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)save_account, working, NULL);
        }
    }
}

static void
state_to_string(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                GtkCellRenderer *cell,
                GtkTreeModel *tree_model,
                GtkTreeIter *iter,
                G_GNUC_UNUSED gpointer data)
{
    gchar *display_state = NULL;

    /* get account */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {

        auto account = AccountModel::instance()->getAccountByModelIndex(idx);
        auto humanState = account->toHumanStateName();

        switch (account->registrationState()) {
            case Account::RegistrationState::READY:
                display_state = g_strdup_printf("<span fgcolor=\"green\">%s</span>", humanState.toUtf8().constData());
            break;
            case Account::RegistrationState::UNREGISTERED:
                display_state = g_strdup_printf("<span fgcolor=\"gray\">%s</span>", humanState.toUtf8().constData());
            break;
            case Account::RegistrationState::TRYING:
                display_state = g_strdup_printf("<span fgcolor=\"orange\">%s</span>", humanState.toUtf8().constData());
            break;
            case Account::RegistrationState::ERROR:
                display_state = g_strdup_printf("<span fgcolor=\"red\">%s</span>", humanState.toUtf8().constData());
            break;
            case Account::RegistrationState::COUNT__:
                g_warning("registration state should never be \"count\"");
                display_state = g_strdup_printf("<span fgcolor=\"red\">%s</span>", humanState.toUtf8().constData());
            break;
        }
    }

    g_object_set(G_OBJECT(cell), "markup", display_state, NULL);
    g_free(display_state);
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
        Account::Role::Proto, G_TYPE_STRING,
        Account::Role::RegistrationState, G_TYPE_UINT);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_account_list), GTK_TREE_MODEL(account_model));

    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes(Q_("Account|Enabled"), renderer, "active", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    g_signal_connect(renderer, "toggled", G_CALLBACK(account_active_toggled), view);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(Q_("Account|Alias"), renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(Q_("Account|Status"), renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    /* the registration state is an enum, we want to display it as a string */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)state_to_string,
        NULL,
        NULL);

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
    priv->protocol_selection_changed = QObject::connect(
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
                    g_warning("SelectionModel of AccountModel changed to invalid QModelIndex?");
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
    G_OBJECT_CLASS(klass)->finalize = account_view_finalize;

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
