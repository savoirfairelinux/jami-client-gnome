/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

#include "accountimportexportview.h"

#include <gtk/gtk.h>
#include <account.h>
#include <accountmodel.h>

#include <glib/gi18n.h>

struct _AccountImportExportView
{
    GtkBox parent;
};

struct _AccountImportExportViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountImportExportViewPrivate AccountImportExportViewPrivate;

struct _AccountImportExportViewPrivate
{
    Account   *account;

    GtkWidget *label_export;
    GtkWidget *label_import;
    GtkWidget *hbox_export_location;
    GtkWidget *label_export_location;
    GtkWidget *button_export_location;
    GtkWidget *filechooserbutton_import;
    GtkWidget *button_export;
    GtkWidget *button_import;
    GtkWidget *button_cancel;
    GtkWidget *entry_password;
    GtkWidget *label_error;

    GList *export_accounts_list;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountImportExportView, account_importexport_view, GTK_TYPE_BOX);

#define ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_IMPORTEXPORT_VIEW_TYPE, AccountImportExportViewPrivate))

/* signals */
enum {
    IMPORT_EXPORT_CANCELED,
    IMPORT_EXPORT_COMPLETED,
    LAST_SIGNAL
};

static guint account_importexport_view_signals[LAST_SIGNAL] = { 0 };

static void
account_importexport_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_importexport_view_parent_class)->dispose(object);
}

static void
account_importexport_view_finalize(GObject *object)
{
    AccountImportExportView *view = ACCOUNT_IMPORTEXPORT_VIEW(object);
    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(view);

    if (priv->export_accounts_list)
        g_list_free(priv->export_accounts_list);

    G_OBJECT_CLASS(account_importexport_view_parent_class)->finalize(object);
}

static void
choose_export_location(AccountImportExportView *self)
{
    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    // clear any existing error
    gtk_label_set_text(GTK_LABEL(priv->label_error), "");

    // create filechooser dialog and get export location
    auto dialog = gtk_file_chooser_dialog_new(_("Select account export location"),
                                              GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(self))),
                                              GTK_FILE_CHOOSER_ACTION_SAVE,
                                              _("Cancel"),
                                              GTK_RESPONSE_CANCEL,
                                              _("Select"),
                                              GTK_RESPONSE_ACCEPT,
                                              NULL);

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
    gtk_file_chooser_set_create_folders(GTK_FILE_CHOOSER(dialog), TRUE);

    if (!priv->export_accounts_list->next) {
        auto name = g_strconcat(static_cast<Account *>(priv->export_accounts_list->data)->alias().toUtf8().constData(), ".ring", NULL);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), name);
        g_free(name);
    } else {
        // TODO: handle multiple account export
    }

    /* start the file chooser */
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        if (auto filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog))) {
            gtk_label_set_text(GTK_LABEL(priv->label_export_location), filename);

            // if accounts and password are set then we're ready for export
            auto password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
            if (priv->export_accounts_list && priv->export_accounts_list->data) {
                gtk_widget_set_sensitive(priv->button_export, TRUE);
            }
            g_free (filename);
        }
    }

    gtk_widget_destroy (dialog);
}


static void
import_file_set(AccountImportExportView *self)
{
    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    // clear any existing error
    gtk_label_set_text(GTK_LABEL(priv->label_error), "");

    gtk_widget_set_sensitive(priv->button_import, TRUE);
}

static void
password_changed(AccountImportExportView *self)
{
    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    // clear any existing error
    gtk_label_set_text(GTK_LABEL(priv->label_error), "");

    // import
    if (auto filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_import))) {
        gtk_widget_set_sensitive(priv->button_import, TRUE);
        g_free(filename);
    }

    // export
    const auto filename = gtk_label_get_text(GTK_LABEL(priv->label_export_location));
    if (strlen(filename) > 0
        && priv->export_accounts_list
        && priv->export_accounts_list->data) {
            gtk_widget_set_sensitive(priv->button_export, TRUE);
        }
    }
}

static void
cancel_clicked(AccountImportExportView *self)
{
    g_signal_emit(G_OBJECT(self), account_importexport_view_signals[IMPORT_EXPORT_CANCELED], 0);
}

static void
import_account(AccountImportExportView *self)
{
    g_return_if_fail(IS_ACCOUNT_IMPORTEXPORT_VIEW(self));

    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    // clear any existing error
    gtk_label_set_text(GTK_LABEL(priv->label_error), "");

    if (auto filepath = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_import))) {
        const auto password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));

        auto ret = AccountModel::instance().importAccounts(filepath, password);
        switch(ret) {
            case 0:
                // done
                g_signal_emit(G_OBJECT(self), account_importexport_view_signals[IMPORT_EXPORT_COMPLETED], 0);
            break;
            default:
                //failed
                gtk_label_set_text(GTK_LABEL(priv->label_error), _("Error importing account(s)"));
                g_warning("failed to import account(s), err: %d", ret);
            break;
        }
        g_free(filepath);
    } else {
        g_warning("no file selected for account import");
    }
}

static void
export_account(AccountImportExportView *self)
{
    g_return_if_fail(IS_ACCOUNT_IMPORTEXPORT_VIEW(self));

    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    // clear any existing error
    gtk_label_set_text(GTK_LABEL(priv->label_error), "");

    // check that we have some accounts to export
    if ( !(priv->export_accounts_list && priv->export_accounts_list->data)) {
        g_warning("no accounts are selected for export");
        return;
    }

    const auto filepath = gtk_label_get_text(GTK_LABEL(priv->label_export_location));
    // validate filepath
    if (strlen(filepath)) {
        const auto password = gtk_entry_get_text(GTK_ENTRY(priv->entry_password));
        // get account id strings
        auto account_ids = QStringList();
        auto list = priv->export_accounts_list;
        while (list != nullptr) {
            auto account = static_cast<Account *>(list->data);
            account_ids << account->id();
            list = g_list_next(list);
        }

        auto ret = AccountModel::instance().exportAccounts(account_ids, filepath, password);
        switch (ret) {
            case 0:
                // done
                g_signal_emit(G_OBJECT(self), account_importexport_view_signals[IMPORT_EXPORT_COMPLETED], 0);
            break;
            default:
                //failed
                gtk_label_set_text(GTK_LABEL(priv->label_error), _("Error exporting account(s)"));
                g_warning("failed to export account(s), err: %d", ret);
            break;
        }
    } else {
        g_warning("no file selected for account export");
    }
}

static void
account_importexport_view_init(AccountImportExportView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    g_signal_connect_swapped(priv->button_export_location, "clicked", G_CALLBACK(choose_export_location), self);
    g_signal_connect_swapped(priv->filechooserbutton_import, "file-set", G_CALLBACK(import_file_set), self);
    g_signal_connect_swapped(priv->entry_password, "changed", G_CALLBACK(password_changed), self);
    g_signal_connect_swapped(priv->button_cancel, "clicked", G_CALLBACK(cancel_clicked), self);
    g_signal_connect_swapped(priv->button_import, "clicked", G_CALLBACK(import_account), self);
    g_signal_connect_swapped(priv->button_export, "clicked", G_CALLBACK(export_account), self);
}

static void
account_importexport_view_class_init(AccountImportExportViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = account_importexport_view_dispose;
    gobject_class->finalize = account_importexport_view_finalize;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountimportexportview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, label_export);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, label_import);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, hbox_export_location);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, label_export_location);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, button_export_location);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, filechooserbutton_import);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, button_export);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, button_import);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, button_cancel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountImportExportView, label_error);

    /* add signals */
    account_importexport_view_signals[IMPORT_EXPORT_CANCELED] = g_signal_new("import-export-canceled",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);

    account_importexport_view_signals[IMPORT_EXPORT_COMPLETED] = g_signal_new("import-export-completed",
                 G_TYPE_FROM_CLASS(klass),
                 (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
                 0,
                 nullptr,
                 nullptr,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE, 0);
}

static void
build_import_view(AccountImportExportView *self)
{
    g_return_if_fail(self);

    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    gtk_widget_hide(priv->label_export);
    gtk_widget_hide(priv->hbox_export_location);
    gtk_widget_hide(priv->button_export);

    gtk_widget_show(priv->label_import);
    gtk_widget_show(priv->filechooserbutton_import);
    gtk_widget_show(priv->button_import);
}

static void
build_export_view(AccountImportExportView *self)
{
    g_return_if_fail(self);

    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    gtk_widget_show(priv->label_export);
    gtk_widget_show(priv->hbox_export_location);
    gtk_widget_show(priv->button_export);

    gtk_widget_hide(priv->label_import);
    gtk_widget_hide(priv->filechooserbutton_import);
    gtk_widget_hide(priv->button_import);
}

GtkWidget *
account_import_view_new()
{
    gpointer view = g_object_new(ACCOUNT_IMPORTEXPORT_VIEW_TYPE, NULL);

    build_import_view(ACCOUNT_IMPORTEXPORT_VIEW(view));

    return (GtkWidget *)view;
}

GtkWidget *
account_export_view_new()
{
    gpointer view = g_object_new(ACCOUNT_IMPORTEXPORT_VIEW_TYPE, NULL);

    build_export_view(ACCOUNT_IMPORTEXPORT_VIEW(view));

    return (GtkWidget *)view;
}

void
account_export_view_set_accounts(AccountImportExportView *self, GList *accounts)
{
    g_return_if_fail(self);
    AccountImportExportViewPrivate *priv = ACCOUNT_IMPORTEXPORT_VIEW_GET_PRIVATE(self);

    // replace current list
    if (priv->export_accounts_list) {
        g_list_free(priv->export_accounts_list);
        priv->export_accounts_list = nullptr;
    }

    // make sure the new list isn't empty
    if (accounts && accounts->data) {
        priv->export_accounts_list = g_list_copy(accounts);

        if (!accounts->next) {
            auto location = g_strconcat(g_get_home_dir(), "/", static_cast<Account *>(priv->export_accounts_list->data)->alias().toUtf8().constData(), ".ring", NULL);
            gtk_label_set_text(GTK_LABEL(priv->label_export_location), location);
            g_free(location);
        } else {
            // TODO: handle multiple account export
        }
    } else {
        // no accounts are selected... this case should not normally be ever displayed
    }
}
