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

#include "accountview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <accountmodel.h>
#include <protocolmodel.h>
#include <QtCore/QItemSelectionModel>
#include "models/gtkqtreemodel.h"
#include "models/gtkqtreemodel.h"
#include "models/activeitemproxymodel.h"
#include "accountgeneraltab.h"
#include "accountcreationwizard.h"
#include "accountadvancedtab.h"
#include "accountsecuritytab.h"
#include "accountdevicestab.h"
#include "dialogs.h"
#include <glib/gprintf.h>
#include "utils/models.h"
#include "accountimportexportview.h"

static constexpr const char* ACCOUNT_CREATION_WIZARD_VIEW_NAME = "account-creation-wizard";

struct _AccountView
{
    GtkPaned parent;
};

struct _AccountViewClass
{
    GtkPanedClass parent_class;
};

typedef struct _AccountViewPrivate AccountViewPrivate;

struct _AccountViewPrivate
{
    GtkWidget *treeview_account_list;
    GtkWidget *account_import_view;
    GtkWidget *account_export_view;
    GtkWidget *stack_account;
    GtkWidget *button_remove_account;
    GtkWidget *button_add_account;
    GtkWidget *combobox_account_type;
    GtkWidget *button_import_account;
    GtkWidget *button_export_account;
    GtkWidget *account_creation_wizard;

    gint current_page; /* keeps track of current notebook page displayed */

    QMetaObject::Connection protocol_selection_changed;
    QMetaObject::Connection account_selection_changed;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountView, account_view, GTK_TYPE_PANED);

#define ACCOUNT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIEW_TYPE, AccountViewPrivate))

static void
account_view_dispose(GObject *object)
{
    AccountView *view = ACCOUNT_VIEW(object);
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->protocol_selection_changed);
    QObject::disconnect(priv->account_selection_changed);

    G_OBJECT_CLASS(account_view_parent_class)->dispose(object);
}

static void
update_account_model_selection(GtkTreeSelection *selection, G_GNUC_UNUSED gpointer user_data)
{
    QModelIndex current = get_index_from_selection(selection);
    if (current.isValid())
        AccountModel::instance().selectionModel()->setCurrentIndex(current, QItemSelectionModel::ClearAndSelect);
    else
        AccountModel::instance().selectionModel()->clearCurrentIndex();
}

static GtkWidget *
create_scrolled_account_view(GtkWidget *account_view)
{
    auto scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled), account_view);
    return scrolled;
}

static void
tab_selection_changed(G_GNUC_UNUSED GtkNotebook *notebook,
                      G_GNUC_UNUSED GtkWidget   *page,
                                    guint        page_num,
                                    AccountView *self)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(self));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(self);

    priv->current_page = page_num;
}

static void
account_selection_changed(GtkTreeSelection *selection, AccountView *self)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(self));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(self);

    auto old_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account));
    if(IS_ACCOUNT_CREATION_WIZARD(old_view))
    {
        /* When using the account creation wizard, The user might create
         * accounts that will be deleted by the daemon. The fact that the
         * selection has changed should be ignored. The account creation wizard
         * should be responsible to remove itself of the stack and then call
         * account_selection_changed.
         */
        return;
    }

    QModelIndex account_idx = get_index_from_selection(selection);
    if (!account_idx.isValid()) {
        /* it nothing is slected, simply display something empty */
        GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_show(empty_box);
        gtk_stack_add_named(GTK_STACK(priv->stack_account), empty_box, "placeholder");
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), empty_box);

        /* cannot delete nor export accounts */
        gtk_widget_set_sensitive(priv->button_remove_account, FALSE);
        gtk_widget_set_sensitive(priv->button_export_account, FALSE);
    } else {
        Account *account = AccountModel::instance().getAccountByModelIndex(account_idx);

        /* build new account view */
        GtkWidget *hbox_account = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_widget_show(hbox_account);

        /* create account notebook */
        auto account_notebook = gtk_notebook_new();
        gtk_widget_show(account_notebook);
        gtk_notebook_set_scrollable(GTK_NOTEBOOK(account_notebook), TRUE);
        gtk_notebook_set_show_border(GTK_NOTEBOOK(account_notebook), FALSE);
        gtk_box_pack_start(GTK_BOX(hbox_account), account_notebook, TRUE, TRUE, 0);

        /* customize account view based on account */
        auto general_tab = create_scrolled_account_view(account_general_tab_new(account));
        gtk_widget_show(general_tab);
        gtk_notebook_append_page(GTK_NOTEBOOK(account_notebook),
                                 general_tab,
                                 gtk_label_new(C_("Account settings", "General")));

        if (account->protocol() == Account::Protocol::RING)
        {
            auto devices_tab = create_scrolled_account_view(account_devices_tab_new(account));
            gtk_widget_show(devices_tab);
            gtk_notebook_append_page(GTK_NOTEBOOK(account_notebook),
                                     devices_tab,
                                     gtk_label_new(C_("Account settings", "Devices")));
        }
        auto security_tab = create_scrolled_account_view(account_security_tab_new(account));
        gtk_widget_show(security_tab);
        gtk_notebook_append_page(GTK_NOTEBOOK(account_notebook),
                                 security_tab,
                                 gtk_label_new(C_("Account settings", "Security")));
        auto advanced_tab = create_scrolled_account_view(account_advanced_tab_new(account));
        gtk_widget_show(advanced_tab);
        gtk_notebook_append_page(GTK_NOTEBOOK(account_notebook),
                                 advanced_tab,
                                 gtk_label_new(C_("Account settings", "Advanced")));

        /* set the tab displayed to the same as the prev account selected */
        gtk_notebook_set_current_page(GTK_NOTEBOOK(account_notebook), priv->current_page);
        /* now connect to the tab changed signal */
        g_signal_connect(account_notebook, "switch-page", G_CALLBACK(tab_selection_changed), self);

        /* set the new account view as visible */
        char *account_view_name = g_strdup_printf("%p_account", account);
        gtk_stack_add_named(GTK_STACK(priv->stack_account), hbox_account, account_view_name);
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), hbox_account);
        g_free(account_view_name);

        /* can delete and export accounts */
        gtk_widget_set_sensitive(priv->button_remove_account, TRUE);
        gtk_widget_set_sensitive(priv->button_export_account, TRUE);
    }

    /* remove the old view */
    if (old_view)
        gtk_container_remove(GTK_CONTAINER(priv->stack_account), old_view);
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
        QVariant alias = idx.data(static_cast<int>(Account::Role::Alias));
        AccountModel::instance().setData(idx, QVariant(toggle), Qt::CheckStateRole);
        /* save the account to apply the changed state right away */
        AccountModel::instance().getAccountByModelIndex(idx)->performAction(Account::EditAction::SAVE);
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
    AccountModel::instance().save();
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
        Account *account = AccountModel::instance().getAccountByModelIndex(idx);
        if (remove_account_dialog(view, account)) {
            /* show working dialog in case save operation takes time */
            GtkWidget *working = ring_dialog_working(GTK_WIDGET(view), NULL);
            gtk_window_present(GTK_WINDOW(working));

            AccountModel::instance().remove(idx);

            /* now save the time it takes to transition the account view to the new account (300ms)
             * the save doesn't happen before the "working" dialog is presented
             * the timeout function should destroy the "working" dialog when done saving
             */
            g_timeout_add_full(G_PRIORITY_DEFAULT, 300, (GSourceFunc)save_account, working, NULL);
        }
    }
}

static void
hide_account_creation_wizard(AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    auto old_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account));
    if (IS_ACCOUNT_CREATION_WIZARD(old_view))
    {
        gtk_container_remove(GTK_CONTAINER(priv->stack_account), old_view);
    }

    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    account_selection_changed(selection, view);
}

static void
show_account_creation_wizard(AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    auto old_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account));

    if (!priv->account_creation_wizard)
    {
        priv->account_creation_wizard = account_creation_wizard_new(true);
        g_object_add_weak_pointer(G_OBJECT(priv->account_creation_wizard), (gpointer *)&priv->account_creation_wizard);
        gtk_stack_add_named(GTK_STACK(priv->stack_account),
                            priv->account_creation_wizard,
                            ACCOUNT_CREATION_WIZARD_VIEW_NAME);
        g_signal_connect_swapped(priv->account_creation_wizard, "account-creation-completed", G_CALLBACK(hide_account_creation_wizard), view);
        g_signal_connect_swapped(priv->account_creation_wizard, "account-creation-canceled", G_CALLBACK(hide_account_creation_wizard), view);
    }

    gtk_widget_show(priv->account_creation_wizard);

    gtk_stack_set_visible_child_name(GTK_STACK(priv->stack_account), ACCOUNT_CREATION_WIZARD_VIEW_NAME);

    /* remove the old view */
    if (old_view)
        gtk_container_remove(GTK_CONTAINER(priv->stack_account), old_view);
}

static void
add_account(G_GNUC_UNUSED GtkWidget *entry, AccountView *view)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(view));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(view);

    auto idx = gtk_combo_box_get_index(GTK_COMBO_BOX(priv->combobox_account_type));
    if (idx.isValid()) {
        Account::Protocol protocol = qvariant_cast<Account::Protocol>(idx.data((int)ProtocolModel::Role::Protocol));
        if (protocol == Account::Protocol::RING)
        {
            show_account_creation_wizard(view);
        }
        else
        {
            /* show working dialog in case save operation takes time */
            GtkWidget *working = ring_dialog_working(GTK_WIDGET(view), NULL);
            gtk_window_present(GTK_WINDOW(working));

            AccountModel::instance().add(QString(_("New Account")), idx);

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
                GtkTreeView *treeview)
{
    // check if this iter is selected
    gboolean is_selected = FALSE;
    if (GTK_IS_TREE_VIEW(treeview)) {
        auto selection = gtk_tree_view_get_selection(treeview);
        is_selected = gtk_tree_selection_iter_is_selected(selection, iter);
    }

    gchar *display_state = NULL;

    /* get account */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(tree_model), iter);
    if (idx.isValid()) {

        auto account = AccountModel::instance().getAccountByModelIndex(idx);
        auto humanState = account->toHumanStateName();

        auto escaped_state = g_markup_escape_text(humanState.toUtf8().constData(), -1);
        /* we want the color of the status text to be the default color if this iter is
         * selected so that the treeview is able to invert it against the selection color */
        if (is_selected) {
            display_state = escaped_state;
        } else {
            switch (account->registrationState()) {
                case Account::RegistrationState::READY:
                    display_state = g_strdup_printf("<span fgcolor=\"green\">%s</span>", escaped_state);
                break;
                case Account::RegistrationState::UNREGISTERED:
                    display_state = g_strdup_printf("<span fgcolor=\"gray\">%s</span>", escaped_state);
                break;
                case Account::RegistrationState::TRYING:
                case Account::RegistrationState::INITIALIZING:
                    display_state = g_strdup_printf("<span fgcolor=\"orange\">%s</span>", escaped_state);
                break;
                case Account::RegistrationState::ERROR:
                    display_state = g_strdup_printf("<span fgcolor=\"red\">%s</span>", escaped_state);
                break;
                case Account::RegistrationState::COUNT__:
                    g_warning("registration state should never be \"count\"");
                    display_state = g_strdup_printf("<span fgcolor=\"red\">%s</span>", escaped_state);
                break;
            }
            g_free(escaped_state);
        }
    }

    g_object_set(G_OBJECT(cell), "markup", display_state, NULL);
    g_free(display_state);
}

static void
close_import_export_view(AccountView *self)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(self));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(self);

    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    account_selection_changed(selection, self);
}

static void
import_account(AccountView* self)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(self));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(self);

    auto old_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account));

    auto import_account = account_import_view_new();
    g_signal_connect_swapped(import_account, "import-export-canceled", G_CALLBACK(close_import_export_view), self);
    g_signal_connect_swapped(import_account, "import-export-completed", G_CALLBACK(close_import_export_view), self);
    auto scrolled_view = create_scrolled_account_view(import_account);
    gtk_widget_show_all(scrolled_view);
    gtk_stack_add_named(GTK_STACK(priv->stack_account), scrolled_view, "import_account");
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), scrolled_view);

    /* remove the old view */
    if (old_view)
        gtk_container_remove(GTK_CONTAINER(priv->stack_account), old_view);
}

static void
export_account(AccountView* self)
{
    g_return_if_fail(IS_ACCOUNT_VIEW(self));
    AccountViewPrivate *priv = ACCOUNT_VIEW_GET_PRIVATE(self);

    auto selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));
    auto idx = get_index_from_selection(selection);
    if (idx.isValid()) {
        auto old_view = gtk_stack_get_visible_child(GTK_STACK(priv->stack_account));

        auto account = AccountModel::instance().getAccountByModelIndex(idx);
        auto export_account = account_export_view_new();
        g_signal_connect_swapped(export_account, "import-export-canceled", G_CALLBACK(close_import_export_view), self);
        g_signal_connect_swapped(export_account, "import-export-completed", G_CALLBACK(close_import_export_view), self);
        GList *account_list = nullptr;
        account_list = g_list_append(account_list, account);
        account_export_view_set_accounts(ACCOUNT_IMPORTEXPORT_VIEW(export_account), account_list);
        g_list_free(account_list);
        auto scrolled_view = create_scrolled_account_view(export_account);
        gtk_widget_show_all(scrolled_view);
        gtk_stack_add_named(GTK_STACK(priv->stack_account), scrolled_view, "export_account");
        gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), scrolled_view);

        /* remove the old view */
        if (old_view)
            gtk_container_remove(GTK_CONTAINER(priv->stack_account), old_view);

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

    account_model = gtk_q_tree_model_new(&AccountModel::instance(), 4,
        0, Account::Role::Enabled, G_TYPE_BOOLEAN,
        0, Account::Role::Alias, G_TYPE_STRING,
        0, Account::Role::Proto, G_TYPE_STRING,
        0, Account::Role::RegistrationState, G_TYPE_UINT);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_account_list), GTK_TREE_MODEL(account_model));

    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes(C_("Account state column", "Enabled"), renderer, "active", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);

    g_signal_connect(renderer, "toggled", G_CALLBACK(account_active_toggled), view);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(C_("Account alias (name) column", "Alias"), renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);
    // set a min width so most of the account name is visible
    g_object_set(G_OBJECT(renderer), "width", 75, NULL);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(C_("Account status column", "Status"), renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_account_list), column);
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_tree_view_column_set_expand(column, TRUE);

    /* the registration state is an enum, we want to display it as a string */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)state_to_string,
        priv->treeview_account_list,
        NULL);

    /* add an empty box to the account stack initially, otherwise there will
     * be no cool animation when the first account is selected */
    GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_show(empty_box);
    gtk_stack_add_named(GTK_STACK(priv->stack_account), empty_box, "placeholder");
    gtk_stack_set_visible_child(GTK_STACK(priv->stack_account), empty_box);

    /* populate account type combo box */
    priv->protocol_selection_changed = gtk_combo_box_set_qmodel_text(
        GTK_COMBO_BOX(priv->combobox_account_type),
        AccountModel::instance().protocolModel(),
        AccountModel::instance().protocolModel()->selectionModel()
    );

    /* connect signals to and from the selection model of the account model */
    priv->account_selection_changed = QObject::connect(
        AccountModel::instance().selectionModel(),
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex & current, G_GNUC_UNUSED const QModelIndex & previous) {
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->treeview_account_list));

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

    /* connect signals to add/remove accounts */
    g_signal_connect(priv->button_remove_account, "clicked", G_CALLBACK(remove_account), view);
    g_signal_connect(priv->button_add_account, "clicked", G_CALLBACK(add_account), view);

    /* signals to import/export acounts */
    g_signal_connect_swapped(priv->button_import_account, "clicked", G_CALLBACK(import_account), view);
    g_signal_connect_swapped(priv->button_export_account, "clicked", G_CALLBACK(export_account), view);
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, button_import_account);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountView, button_export_account);
}

GtkWidget *
account_view_new(void)
{
    return (GtkWidget *)g_object_new(ACCOUNT_VIEW_TYPE, NULL);
}
