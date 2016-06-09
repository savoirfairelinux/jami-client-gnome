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

#include "accountcertificatestab.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <account.h>
#include <certificate.h>
// #include "models/gtkqsortfiltertreemodel.h"
#include "models/gtkqtreemodel.h"
#include "utils/models.h"
#include <certificatemodel.h>

struct _AccountCertificatesTab
{
    GtkBox parent;
};

struct _AccountCertificatesTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountCertificatesTabPrivate AccountCertificatesTabPrivate;

struct _AccountCertificatesTabPrivate
{
    Account   *account;
    GtkWidget *treeview_allowed_certificates;
    GtkWidget *treeview_banned_certificates;
    GtkWidget *button_allow;
    GtkWidget *button_ban;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountCertificatesTab, account_certificates_tab, GTK_TYPE_BOX);

#define ACCOUNT_CERTIFICATES_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_CERTIFICATES_TAB_TYPE, AccountCertificatesTabPrivate))

static void
account_certificates_tab_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_certificates_tab_parent_class)->dispose(object);
}

static void
account_certificates_tab_init(AccountCertificatesTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_certificates_tab_class_init(AccountCertificatesTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_certificates_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountcertificatestab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCertificatesTab, treeview_allowed_certificates);
    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountCertificatesTab, treeview_banned_certificates);
}

static void
build_tab_view(AccountCertificatesTab *self)
{
    AccountCertificatesTabPrivate *priv = ACCOUNT_CERTIFICATES_TAB_GET_PRIVATE(self);

    /* codec model */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    // auto allowed_cert_model = gtk_q_sort_filter_tree_model_new(
    //     qobject_cast<QSortFilterProxyModel*>(priv->account->allowedCertificatesModel()),
    //     1,
    //     0, Qt::DisplayRole, G_TYPE_STRING);
    // auto allowed_cert_model = gtk_q_tree_model_new(
    //     priv->account->allowedCertificatesModel(),
    //     1,
    //     0, Qt::DisplayRole, G_TYPE_STRING);
    // gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_allowed_certificates), GTK_TREE_MODEL(allowed_cert_model));

    // try to show all the certs
    auto certs_model = gtk_q_tree_model_new(
        static_cast<QAbstractItemModel *>(&CertificateModel::instance()),
        1,
        0, Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_allowed_certificates), GTK_TREE_MODEL(certs_model));

    g_debug("certs: %d", CertificateModel::instance().rowCount());
    g_debug("allowed certs: %d", priv->account->allowedCertificatesModel()->rowCount());
    g_debug("banned certs: %d", priv->account->bannedCertificatesModel()->rowCount());
    g_debug("known certs: %d", priv->account->knownCertificateModel()->rowCount());

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Certificate"), renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_allowed_certificates), column);
    //
    // g_signal_connect(renderer, "toggled", G_CALLBACK(codec_active_toggled), view);
    //
    // renderer = gtk_cell_renderer_text_new();
    // column = gtk_tree_view_column_new_with_attributes(C_("Name of the codec", "Name"), renderer, "text", 1, NULL);
    // gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_codecs), column);
    //
    // renderer = gtk_cell_renderer_text_new();
    // column = gtk_tree_view_column_new_with_attributes(_("Bitrate"), renderer, "text", 2, NULL);
    // gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_codecs), column);
    //
    // renderer = gtk_cell_renderer_text_new();
    // column = gtk_tree_view_column_new_with_attributes(_("Samplerate"), renderer, "text", 3, NULL);
    // gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_codecs), column);
}

GtkWidget *
account_certificates_tab_new(Account *account)
{
    g_return_val_if_fail(account, NULL);

    gpointer view = g_object_new(ACCOUNT_CERTIFICATES_TAB_TYPE, NULL);

    AccountCertificatesTabPrivate *priv = ACCOUNT_CERTIFICATES_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_CERTIFICATES_TAB(view));

    return (GtkWidget *)view;
}
