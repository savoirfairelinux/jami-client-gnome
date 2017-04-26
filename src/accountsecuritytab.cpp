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

#include "accountsecuritytab.h"

#include <gtk/gtk.h>
#include <account.h>
#include "models/activeitemproxymodel.h"
#include "models/gtkqtreemodel.h"
#include "utils/models.h"
#include <certificate.h>
#include <ciphermodel.h>
#include <QtCore/QItemSelectionModel>

struct _AccountSecurityTab
{
    GtkBox parent;
};

struct _AccountSecurityTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountSecurityTabPrivate AccountSecurityTabPrivate;

struct _AccountSecurityTabPrivate
{
    Account   *account;
    GtkWidget *frame_srtp;
    GtkWidget *checkbutton_use_srtp;
    GtkWidget *box_key_exchange;
    GtkWidget *combobox_key_exchange;
    GtkWidget *checkbutton_srtp_fallback;
    GtkWidget *checkbutton_use_tls;
    GtkWidget *grid_tls_settings_0;
    GtkWidget *filechooserbutton_ca_list;
    /* TODO: add when implemented
    GtkWidget *button_view_ca;
    GtkWidget *button_view_certificate;
    */
    GtkWidget *filechooserbutton_certificate;
    GtkWidget *filechooserbutton_private_key;
    GtkWidget *label_private_key_password;
    GtkWidget *entry_password;
    GtkWidget *label_password_matches;
    GtkWidget *grid_tls_settings_1;
    GtkWidget *combobox_tls_protocol_method;
    GtkWidget *entry_tls_server_name;
    GtkWidget *adjustment_tls_timeout;
    GtkWidget *buttonbox_cipher_list;
    GtkWidget *radiobutton_use_default_ciphers;
    GtkWidget *radiobutton_custom_ciphers;
    GtkWidget *revealer_cipher_list;
    GtkWidget *treeview_cipher_list;
    GtkWidget *checkbutton_verify_certs_server;
    GtkWidget *checkbutton_verify_certs_client;
    GtkWidget *checkbutton_require_incoming_tls_certs;

    QMetaObject::Connection account_updated;
    QMetaObject::Connection key_exchange_selection;
    QMetaObject::Connection tls_method_selection;
    QMetaObject::Connection cert_changed;

    ActiveItemProxyModel *qmodel_key_exchange;

    Certificate *user_cert;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountSecurityTab, account_security_tab, GTK_TYPE_BOX);

#define ACCOUNT_SECURITY_TAB_GET_PRIVATE(obj) \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_SECURITY_TAB_TYPE, \
                                  AccountSecurityTabPrivate))

static void
account_security_tab_dispose(GObject *object)
{
    AccountSecurityTab *view = ACCOUNT_SECURITY_TAB(object);
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(view);

    QObject::disconnect(priv->account_updated);
    QObject::disconnect(priv->key_exchange_selection);
    QObject::disconnect(priv->tls_method_selection);
    QObject::disconnect(priv->cert_changed);

    G_OBJECT_CLASS(account_security_tab_parent_class)->dispose(object);
}

static void
account_security_tab_finalize(GObject *object)
{
    AccountSecurityTab *view = ACCOUNT_SECURITY_TAB(object);
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(view);

    delete priv->qmodel_key_exchange;

    G_OBJECT_CLASS(account_security_tab_parent_class)->finalize(object);
}

static void
account_security_tab_init(AccountSecurityTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_security_tab_class_init(AccountSecurityTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_security_tab_dispose;
    G_OBJECT_CLASS(klass)->finalize = account_security_tab_finalize;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountsecuritytab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, frame_srtp);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, checkbutton_use_srtp);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, box_key_exchange);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, combobox_key_exchange);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, checkbutton_srtp_fallback);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, checkbutton_use_tls);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, grid_tls_settings_0);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, filechooserbutton_ca_list);
    /* TODO: add when implemented
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, button_view_ca);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, button_view_certificate);
    */
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, filechooserbutton_certificate);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, filechooserbutton_private_key);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, label_private_key_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, entry_password);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, label_password_matches);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, grid_tls_settings_1);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, combobox_tls_protocol_method);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, entry_tls_server_name);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, adjustment_tls_timeout);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, buttonbox_cipher_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, radiobutton_use_default_ciphers);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, radiobutton_custom_ciphers);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, revealer_cipher_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, treeview_cipher_list);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, checkbutton_verify_certs_server);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, checkbutton_verify_certs_client);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountSecurityTab, checkbutton_require_incoming_tls_certs);
}

static void
use_srtp_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean use_srtp = gtk_toggle_button_get_active(toggle_button);

    priv->account->setSrtpEnabled(use_srtp);
    priv->account->keyExchangeModel()->enableSRTP(use_srtp);

    /* the other options are not relevant if SRTP is not active */
    gtk_widget_set_sensitive(priv->box_key_exchange, priv->account->isSrtpEnabled());
    gtk_widget_set_sensitive(priv->checkbutton_srtp_fallback, priv->account->isSrtpEnabled());
}

static void
use_rtp_fallback_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean use_rtp_fallback = gtk_toggle_button_get_active(toggle_button);

    priv->account->setSrtpRtpFallback(use_rtp_fallback);
}

static void
use_tls_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean use_tls = gtk_toggle_button_get_active(toggle_button);

    priv->account->setTlsEnabled(use_tls);

    /* disable the other tls options if no tls */
    gtk_widget_set_sensitive(priv->grid_tls_settings_0, priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->grid_tls_settings_1, priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->buttonbox_cipher_list, priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->treeview_cipher_list, priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->checkbutton_verify_certs_server, priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->checkbutton_verify_certs_client, priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->checkbutton_require_incoming_tls_certs, priv->account->isTlsEnabled());
}

static void
tls_server_name_changed(GtkEntry *entry, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    priv->account->setTlsServerName(gtk_entry_get_text(entry));
}

static void
tls_timeout_changed(GtkAdjustment *adjustment, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    int timeout = (int)gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
    priv->account->setTlsNegotiationTimeoutSec(timeout);
}

static void
use_default_ciphers_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean use_default_ciphers = gtk_toggle_button_get_active(toggle_button);

    priv->account->cipherModel()->setUseDefault(use_default_ciphers);

    /* hide the cipher list if we're using the default ones */
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_cipher_list), !use_default_ciphers);
}

static void
cipher_enable_toggled(GtkCellRendererToggle *renderer, gchar *path, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    /* we want to set it to the opposite of the current value */
    gboolean toggle = !gtk_cell_renderer_toggle_get_active(renderer);

    /* get iter which was clicked */
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->treeview_cipher_list));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, tree_path);

    /* get qmodelindex from iter and set the model data */
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    if (idx.isValid())
        priv->account->cipherModel()->setData(idx, toggle ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole);
}

static void
render_check_state(G_GNUC_UNUSED GtkCellLayout *cell_layout,
                   GtkCellRenderer *cell,
                   GtkTreeModel *model,
                   GtkTreeIter *iter,
                   G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx;
    if (GTK_IS_Q_TREE_MODEL(model))
        idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    gboolean checked = FALSE;

    if (idx.isValid()) {
        checked = idx.data(Qt::CheckStateRole).value<int>() == Qt::Checked ? TRUE : FALSE;
    }

    g_object_set(G_OBJECT(cell), "active", checked, NULL);
}

static void
verify_certs_server_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean verify_certs = gtk_toggle_button_get_active(toggle_button);

    priv->account->setTlsVerifyServer(verify_certs);
}

static void
verify_certs_client_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean verify_certs = gtk_toggle_button_get_active(toggle_button);

    priv->account->setTlsVerifyClient(verify_certs);
}

static void
require_incoming_certs_toggled(GtkToggleButton *toggle_button, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean require = gtk_toggle_button_get_active(toggle_button);

    priv->account->setTlsRequireClientCertificate(require);
}

static void
ca_cert_file_set(GtkFileChooser *file_chooser, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gchar *filename = gtk_file_chooser_get_filename(file_chooser);

    priv->account->setTlsCaListCertificate(filename);
    g_free(filename);
}

static void
user_cert_file_set(GtkFileChooser *file_chooser, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gchar *filename = gtk_file_chooser_get_filename(file_chooser);

    priv->account->setTlsCertificate(filename);
    g_free(filename);
}

static void
private_key_file_set(GtkFileChooser *file_chooser, AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gchar *filename = gtk_file_chooser_get_filename(file_chooser);

    priv->account->setTlsPrivateKey(filename);
    g_free(filename);
}

static void
private_key_password_changed(GtkEntry *entry,  AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    priv->account->setTlsPassword(gtk_entry_get_text(entry));
}

static void
check_tlspassword_valid(AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    if (priv->user_cert && priv->user_cert->requirePrivateKeyPassword()) {
        if (priv->user_cert->privateKeyMatch() == Certificate::CheckValues::PASSED) {
            gtk_label_set_markup(
                GTK_LABEL(priv->label_password_matches),
                "<span fgcolor=\"green\">&#x2713;</span>"
            );
        } else {
            gtk_label_set_markup(
                GTK_LABEL(priv->label_password_matches),
                "<span fgcolor=\"red\">&#x2717;</span>"
            );
        }
    } else {
        gtk_label_set_text(GTK_LABEL(priv->label_password_matches), "");
    }
}

static void
build_tab_view(AccountSecurityTab *self)
{
    g_return_if_fail(IS_ACCOUNT_SECURITY_TAB(self));
    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(self);

    gboolean is_ring_account = priv->account->protocol() == Account::Protocol::RING;

    /* SRTP */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_srtp),
                                 priv->account->isSrtpEnabled());
    /* disable options if SRTP is off or if its a RING account */
    gtk_widget_set_sensitive(priv->checkbutton_use_srtp, !is_ring_account);
    gtk_widget_set_sensitive(priv->box_key_exchange,
                             priv->account->isSrtpEnabled() && !is_ring_account);
    gtk_widget_set_sensitive(priv->checkbutton_srtp_fallback,
                             priv->account->isSrtpEnabled() && !is_ring_account);
    g_signal_connect(priv->checkbutton_use_srtp, "toggled", G_CALLBACK(use_srtp_toggled), self);

    /* encryption key exchange type */
    priv->key_exchange_selection =  gtk_combo_box_set_qmodel_text(
        GTK_COMBO_BOX(priv->combobox_key_exchange),
        (QAbstractItemModel *)priv->account->keyExchangeModel(),
        priv->account->keyExchangeModel()->selectionModel());

    /* SRTP fallback */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_srtp_fallback),
                                 priv->account->isSrtpRtpFallback());
    g_signal_connect(priv->checkbutton_srtp_fallback, "toggled", G_CALLBACK(use_rtp_fallback_toggled), self);

    /* use TLS */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_tls),
                                 priv->account->isTlsEnabled());

    /* disable certain options if TLS is off */
    gtk_widget_set_sensitive(priv->checkbutton_use_tls, !is_ring_account);
    gtk_widget_set_sensitive(priv->grid_tls_settings_0,
                             priv->account->isTlsEnabled());
    gtk_widget_set_sensitive(priv->grid_tls_settings_1,
                             priv->account->isTlsEnabled() && !is_ring_account);
    gtk_widget_set_sensitive(priv->buttonbox_cipher_list,
                             priv->account->isTlsEnabled() && !is_ring_account);
    gtk_widget_set_sensitive(priv->treeview_cipher_list,
                             priv->account->isTlsEnabled() && !is_ring_account);
    gtk_widget_set_sensitive(priv->checkbutton_verify_certs_server,
                             priv->account->isTlsEnabled() && !is_ring_account);
    gtk_widget_set_sensitive(priv->checkbutton_verify_certs_client,
                             priv->account->isTlsEnabled() && !is_ring_account);
    gtk_widget_set_sensitive(priv->checkbutton_require_incoming_tls_certs,
                             priv->account->isTlsEnabled() && !is_ring_account);
    g_signal_connect(priv->checkbutton_use_tls, "toggled", G_CALLBACK(use_tls_toggled), self);

    /* hide everything that is not relevant to Ring accounts */
    if (is_ring_account)
    {
        gtk_widget_hide(priv->frame_srtp);
        gtk_widget_hide(priv->grid_tls_settings_1);
        gtk_widget_hide(priv->checkbutton_verify_certs_server);
        gtk_widget_hide(priv->checkbutton_verify_certs_client);
        gtk_widget_hide(priv->checkbutton_require_incoming_tls_certs);
        gtk_widget_hide(priv->buttonbox_cipher_list);
    }
    /* CA certificate */
    Certificate *ca_cert = priv->account->tlsCaListCertificate();
    if (ca_cert) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_ca_list),
                                      ca_cert->path().toUtf8().constData());
    }
    g_signal_connect(priv->filechooserbutton_ca_list, "file-set", G_CALLBACK(ca_cert_file_set), self);

    /* user certificate */
    if ( (priv->user_cert = priv->account->tlsCertificate()) ) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_certificate),
            priv->user_cert->path().toUtf8().constData()
        );
    }
    g_signal_connect(priv->filechooserbutton_certificate, "file-set", G_CALLBACK(user_cert_file_set), self);

    /* private key */
    if (!priv->account->tlsPrivateKey().isEmpty()) {
        gtk_file_chooser_set_filename(
            GTK_FILE_CHOOSER(priv->filechooserbutton_private_key),
            priv->account->tlsPrivateKey().toUtf8().constData()
        );
    }

    g_signal_connect(priv->filechooserbutton_private_key, "file-set", G_CALLBACK(private_key_file_set), self);

    /* password */
    check_tlspassword_valid(self);
    if (priv->user_cert && priv->user_cert->requirePrivateKeyPassword()) {
        gtk_entry_set_text(
            GTK_ENTRY(priv->entry_password),
            priv->account->tlsPassword().toUtf8().constData()
        );
        gtk_widget_set_sensitive(priv->entry_password, TRUE);
        gtk_widget_set_sensitive(priv->label_private_key_password, TRUE);
    } else {
        /* private key not chosen, or password not required, so disactivate the entry */
        gtk_widget_set_sensitive(priv->entry_password, FALSE);
        gtk_widget_set_sensitive(priv->label_private_key_password, FALSE);
    }

    g_signal_connect(priv->entry_password, "changed", G_CALLBACK(private_key_password_changed), self);

    /* TLS protocol method */
    priv->tls_method_selection = gtk_combo_box_set_qmodel_text(GTK_COMBO_BOX(priv->combobox_tls_protocol_method),
                                                          (QAbstractItemModel *)priv->account->tlsMethodModel(),
                                                          priv->account->tlsMethodModel()->selectionModel());

    /* outgoing TLS server */
    gtk_entry_set_text(GTK_ENTRY(priv->entry_tls_server_name),
                       priv->account->tlsServerName().toUtf8().constData());
    g_signal_connect(priv->entry_tls_server_name, "changed", G_CALLBACK(tls_server_name_changed), self);


    /* TLS nego timeout */
    gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_tls_timeout),
                             priv->account->tlsNegotiationTimeoutSec());
    g_signal_connect(priv->adjustment_tls_timeout, "value-changed", G_CALLBACK(tls_timeout_changed), self);

    /* cipher default or custom */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_use_default_ciphers),
                                 priv->account->cipherModel()->useDefault());
    /* hide the cipher list if we're using the default ones */
    gtk_revealer_set_reveal_child(GTK_REVEALER(priv->revealer_cipher_list),
                                  !priv->account->cipherModel()->useDefault());
    g_signal_connect(priv->radiobutton_use_default_ciphers,
                     "toggled", G_CALLBACK(use_default_ciphers_toggled), self);

    /* cipher list */
    GtkQTreeModel *cipher_model = gtk_q_tree_model_new(
        (QAbstractItemModel *)priv->account->cipherModel(),
        2,
        0, Qt::CheckStateRole, G_TYPE_BOOLEAN,
        0, Qt::DisplayRole, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_cipher_list),
                            GTK_TREE_MODEL(cipher_model));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->treeview_cipher_list),
                                      FALSE);

    GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
        "Enabled", renderer, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_cipher_list), column);

    /* we have to use a custom data function here because Qt::Checked and Qt::Unchecked
     * are not the same as true/false as there is an intermediate state */
    gtk_tree_view_column_set_cell_data_func(column,
                                            renderer,
                                            (GtkTreeCellDataFunc)render_check_state,
                                            NULL, NULL);

    g_signal_connect(renderer, "toggled", G_CALLBACK(cipher_enable_toggled), self);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Cipher", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_cipher_list), column);

    /* server certs */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_verify_certs_server),
                                 priv->account->isTlsVerifyServer());
    g_signal_connect(priv->checkbutton_verify_certs_server,
                     "toggled", G_CALLBACK(verify_certs_server_toggled), self);

    /* client certs */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_verify_certs_client),
                                 priv->account->isTlsVerifyClient());
    g_signal_connect(priv->checkbutton_verify_certs_client,
                     "toggled", G_CALLBACK(verify_certs_client_toggled), self);

    /* incoming certs */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_require_incoming_tls_certs),
                                 priv->account->isTlsRequireClientCertificate());
    g_signal_connect(priv->checkbutton_require_incoming_tls_certs,
                     "toggled", G_CALLBACK(require_incoming_certs_toggled), self);

    /* update account UI if model is updated */
    priv->account_updated = QObject::connect(
        priv->account,
        &Account::changed,
        [=] () {
            /* SRTP */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_srtp),
                                         priv->account->isSrtpEnabled());

            /* SRTP fallback */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_srtp_fallback),
                                         priv->account->isSrtpRtpFallback());
            /* use TLS */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_use_tls),
                                         priv->account->isTlsEnabled());

            /* CA certificate */
            Certificate *ca_cert = priv->account->tlsCaListCertificate();
            if (ca_cert) {
                gtk_file_chooser_set_filename(
                    GTK_FILE_CHOOSER(priv->filechooserbutton_ca_list),
                    ca_cert->path().toUtf8().constData()
                );
            } else {
                /* make sure nothing is selected */
                gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(priv->filechooserbutton_ca_list));
            }


            /* user certificate */
            QObject::disconnect(priv->cert_changed);
            if ( (priv->user_cert = priv->account->tlsCertificate()) ) {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(priv->filechooserbutton_certificate),
                    priv->user_cert->path().toUtf8().constData()
                );
            } else {
                /* make sure nothing is selected */
                gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(priv->filechooserbutton_certificate));
            }

            /* private key */
            if (!priv->account->tlsPrivateKey().isEmpty()) {
                gtk_file_chooser_set_filename(
                    GTK_FILE_CHOOSER(priv->filechooserbutton_private_key),
                    priv->account->tlsPrivateKey().toUtf8().constData()
                );
            } else {
                /* make sure nothing is selected */
                gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(priv->filechooserbutton_private_key));
            }

            /* password */
            check_tlspassword_valid(self);
            if (priv->user_cert && priv->user_cert->requirePrivateKeyPassword()) {
                gtk_entry_set_text(
                    GTK_ENTRY(priv->entry_password),
                    priv->account->tlsPassword().toUtf8().constData()
                );
                gtk_widget_set_sensitive(priv->entry_password, TRUE);
                gtk_widget_set_sensitive(priv->label_private_key_password, TRUE);
            } else {
                /* private key not chosen, or password not required, so disactivate the entry */
                gtk_widget_set_sensitive(priv->entry_password, FALSE);
                gtk_widget_set_sensitive(priv->label_private_key_password, FALSE);
            }

            /* outgoing TLS server */
            gtk_entry_set_text(GTK_ENTRY(priv->entry_tls_server_name),
                               priv->account->tlsServerName().toUtf8().constData());

            /* TLS nego timeout */
            gtk_adjustment_set_value(GTK_ADJUSTMENT(priv->adjustment_tls_timeout),
                                     priv->account->tlsNegotiationTimeoutSec());

            /* cipher default or custom */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->radiobutton_use_default_ciphers),
                                        priv->account->cipherModel()->useDefault());

            /* server certs */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_verify_certs_server),
                                        priv->account->isTlsVerifyServer());

            /* client certs */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_verify_certs_client),
                                        priv->account->isTlsVerifyClient());

            /* incoming certs */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_require_incoming_tls_certs),
                                        priv->account->isTlsRequireClientCertificate());
        }
    );
}

GtkWidget *
account_security_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_SECURITY_TAB_TYPE, NULL);

    AccountSecurityTabPrivate *priv = ACCOUNT_SECURITY_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_SECURITY_TAB(view));

    return (GtkWidget *)view;
}
