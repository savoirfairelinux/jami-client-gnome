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

#include "accountvideotab.h"

#include <gtk/gtk.h>
#include <account.h>
#include <audio/codecmodel.h>
#include "models/gtkqsortfiltertreemodel.h"

struct _AccountVideoTab
{
    GtkBox parent;
};

struct _AccountVideoTabClass
{
    GtkBoxClass parent_class;
};

typedef struct _AccountVideoTabPrivate AccountVideoTabPrivate;

struct _AccountVideoTabPrivate
{
    Account   *account;
    GtkWidget *treeview_codecs;
    GtkWidget *checkbutton_enable;
};

G_DEFINE_TYPE_WITH_PRIVATE(AccountVideoTab, account_video_tab, GTK_TYPE_BOX);

#define ACCOUNT_VIDEO_TAB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), ACCOUNT_VIDEO_TAB_TYPE, AccountVideoTabPrivate))

static void
account_video_tab_dispose(GObject *object)
{
    G_OBJECT_CLASS(account_video_tab_parent_class)->dispose(object);
}

static void
account_video_tab_init(AccountVideoTab *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
account_video_tab_class_init(AccountVideoTabClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = account_video_tab_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/accountvideotab.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountVideoTab, treeview_codecs);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), AccountVideoTab, checkbutton_enable);
}

static void
video_enable(GtkToggleButton *checkbutton, AccountVideoTab *view)
{
    g_return_if_fail(IS_ACCOUNT_VIDEO_TAB(view));
   AccountVideoTabPrivate *priv = ACCOUNT_VIDEO_TAB_GET_PRIVATE(view);
    priv->account->setVideoEnabled(gtk_toggle_button_get_active(checkbutton));
}

static void
codec_active_toggled(GtkCellRendererToggle *renderer, gchar *path, AccountVideoTab *view)
{
    g_return_if_fail(IS_ACCOUNT_VIDEO_TAB(view));
    AccountVideoTabPrivate *priv = ACCOUNT_VIDEO_TAB_GET_PRIVATE(view);

    /* we want to set it to the opposite of the current value */
    gboolean toggle = !gtk_cell_renderer_toggle_get_active(renderer);

    /* get iter which was clicked */
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->treeview_codecs));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, tree_path);

    /* get qmodelindex from iter and set the model data */
    QModelIndex idx = gtk_q_sort_filter_tree_model_get_source_idx(GTK_Q_SORT_FILTER_TREE_MODEL(model), &iter);
    if (idx.isValid()) {
        priv->account->codecModel()->videoCodecs()->setData(idx, QVariant(toggle), Qt::CheckStateRole);
        priv->account->codecModel()->save();
    }
}

static void
build_tab_view(AccountVideoTab *view)
{
    g_return_if_fail(IS_ACCOUNT_VIDEO_TAB(view));
    AccountVideoTabPrivate *priv = ACCOUNT_VIDEO_TAB_GET_PRIVATE(view);

    /* codec model */
    GtkQSortFilterTreeModel *codec_model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    codec_model = gtk_q_sort_filter_tree_model_new(
        priv->account->codecModel()->videoCodecs(),
        3,
        Qt::CheckStateRole, G_TYPE_BOOLEAN,
        CodecModel::Role::NAME, G_TYPE_STRING,
        CodecModel::Role::BITRATE, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_codecs), GTK_TREE_MODEL(codec_model));

    renderer = gtk_cell_renderer_toggle_new();
    column = gtk_tree_view_column_new_with_attributes("Enabled", renderer, "active", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_codecs), column);

    g_signal_connect(renderer, "toggled", G_CALLBACK(codec_active_toggled), view);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_codecs), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Bitrate", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_codecs), column);

    /* enable video checkbutton */
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(priv->checkbutton_enable), priv->account->isVideoEnabled());
    g_signal_connect(priv->checkbutton_enable, "toggled", G_CALLBACK(video_enable), view);
}

GtkWidget *
account_video_tab_new(Account *account)
{
    g_return_val_if_fail(account != NULL, NULL);

    gpointer view = g_object_new(ACCOUNT_VIDEO_TAB_TYPE, NULL);

    AccountVideoTabPrivate *priv = ACCOUNT_VIDEO_TAB_GET_PRIVATE(view);
    priv->account = account;

    build_tab_view(ACCOUNT_VIDEO_TAB(view));

    return (GtkWidget *)view;
}
