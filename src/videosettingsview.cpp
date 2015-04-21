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

#include "videosettingsview.h"

#include <gtk/gtk.h>
#include "models/gtkqtreemodel.h"
#include "video/video_widget.h"
#include <video/previewmanager.h>
#include <video/configurationproxy.h>
#include <QtCore/QItemSelectionModel>

struct _VideoSettingsView
{
    GtkBox parent;
};

struct _VideoSettingsViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _VideoSettingsViewPrivate VideoSettingsViewPrivate;

struct _VideoSettingsViewPrivate
{
    GtkWidget *combobox_device;
    GtkWidget *combobox_channel;
    GtkWidget *combobox_resolution;
    GtkWidget *combobox_framerate;
    GtkWidget *button_startstop;
    GtkWidget *vbox_camera_preview;
    GtkWidget *video_widget;

    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection device_selection;
    QMetaObject::Connection channel_selection;
    QMetaObject::Connection resolution_selection;
    QMetaObject::Connection rate_selection;
};

G_DEFINE_TYPE_WITH_PRIVATE(VideoSettingsView, video_settings_view, GTK_TYPE_BOX);

#define VIDEO_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), VIDEO_SETTINGS_VIEW_TYPE, VideoSettingsViewPrivate))

static void
video_settings_view_dispose(GObject *object)
{
    VideoSettingsView *view = VIDEO_SETTINGS_VIEW(object);
    VideoSettingsViewPrivate *priv = VIDEO_SETTINGS_VIEW_GET_PRIVATE(view);

    QObject::disconnect(priv->local_renderer_connection);
    QObject::disconnect(priv->device_selection);
    QObject::disconnect(priv->channel_selection);
    QObject::disconnect(priv->resolution_selection);
    QObject::disconnect(priv->rate_selection);

    G_OBJECT_CLASS(video_settings_view_parent_class)->dispose(object);
}

static void
preview_toggled(GtkToggleButton *togglebutton, G_GNUC_UNUSED gpointer user_data)
{
    if (gtk_toggle_button_get_active(togglebutton)) {
        Video::PreviewManager::instance()->startPreview();
        gtk_button_set_label(GTK_BUTTON(togglebutton), "stop");
    } else {
        Video::PreviewManager::instance()->stopPreview();
        gtk_button_set_label(GTK_BUTTON(togglebutton), "start");
    }
}

static QModelIndex
get_index_from_combobox(GtkComboBox *box)
{
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_combo_box_get_model(box);
    if (gtk_combo_box_get_active_iter(box, &iter)) {
        return gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
    } else {
        return QModelIndex();
    }
}

static void
update_selection(GtkComboBox *box, QItemSelectionModel *selection_model)
{
    QModelIndex idx = get_index_from_combobox(box);
    if (idx.isValid())
        selection_model->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect);
}

static QMetaObject::Connection
connect_combo_box_qmodel(GtkComboBox *box, QAbstractItemModel *qmodel, QItemSelectionModel *selection_model)
{
    QMetaObject::Connection connection;
    GtkCellRenderer *renderer;
    GtkQTreeModel *model = gtk_q_tree_model_new(qmodel,
                                                1,
                                                Qt::DisplayRole, G_TYPE_STRING);

    gtk_combo_box_set_model(box, GTK_TREE_MODEL(model));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer,
                                   "text", 0, NULL);

    /* connect signals to and from the selection model */
    connection = QObject::connect(
        selection_model,
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex & current, G_GNUC_UNUSED const QModelIndex & previous) {
            /* select the current */
            if (current.isValid()) {
                GtkTreeIter new_iter;
                GtkTreeModel *model = gtk_combo_box_get_model(box);
                g_return_if_fail(model != NULL);
                if (gtk_q_tree_model_source_index_to_iter(GTK_Q_TREE_MODEL(model), current, &new_iter)) {
                    gtk_combo_box_set_active_iter(box, &new_iter);
                } else {
                    g_warning("SelectionModel changed to invalid QModelIndex?");
                }
            }
        }
    );
    g_signal_connect(box,
                     "changed",
                     G_CALLBACK(update_selection),
                     selection_model);

    /* sync the initial selection */
    QModelIndex idx = selection_model->currentIndex();
    if (idx.isValid()) {
        GtkTreeIter iter;
        if (gtk_q_tree_model_source_index_to_iter(model, idx, &iter))
            gtk_combo_box_set_active_iter(box, &iter);
    }

    return connection;
}

static void
video_settings_view_init(VideoSettingsView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    VideoSettingsViewPrivate *priv = VIDEO_SETTINGS_VIEW_GET_PRIVATE(view);

    /* put video widget in */
    priv->video_widget = video_widget_new();
    gtk_widget_show_all(priv->video_widget);
    gtk_box_pack_start(GTK_BOX(priv->vbox_camera_preview), priv->video_widget, TRUE, TRUE, 0);

    /* if preview is already started, toggle start/stop button */
    if (Video::PreviewManager::instance()->isPreviewing()) {
        gtk_button_set_label(GTK_BUTTON(priv->button_startstop), "stop");
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->button_startstop), TRUE);
    }
    g_signal_connect(priv->button_startstop, "toggled", G_CALLBACK(preview_toggled), NULL);

    /* local renderer, but set as "remote" so that it takes up the whole screen */
    /* check to see if the renderere is running first */
    if (Video::PreviewManager::instance()->isPreviewing())
        video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                       Video::PreviewManager::instance()->previewRenderer(),
                                       VIDEO_RENDERER_REMOTE);

    priv->local_renderer_connection = QObject::connect(
        Video::PreviewManager::instance(),
        &Video::PreviewManager::previewStarted,
        [=](Video::Renderer *renderer) {
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           renderer,
                                           VIDEO_RENDERER_REMOTE);
        }
    );

    priv->device_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_device),
                                                      Video::ConfigurationProxy::deviceModel(),
                                                      Video::ConfigurationProxy::deviceSelectionModel());
    priv->channel_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_channel),
                                                       Video::ConfigurationProxy::channelModel(),
                                                       Video::ConfigurationProxy::channelSelectionModel());
    priv->resolution_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_resolution),
                                                          Video::ConfigurationProxy::resolutionModel(),
                                                          Video::ConfigurationProxy::resolutionSelectionModel());
    priv->rate_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_framerate),
                                                    Video::ConfigurationProxy::rateModel(),
                                                    Video::ConfigurationProxy::rateSelectionModel());
}

static void
video_settings_view_class_init(VideoSettingsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = video_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/videosettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), VideoSettingsView, combobox_device);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), VideoSettingsView, combobox_channel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), VideoSettingsView, combobox_resolution);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), VideoSettingsView, combobox_framerate);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), VideoSettingsView, button_startstop);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), VideoSettingsView, vbox_camera_preview);
}

GtkWidget *
video_settings_view_new()
{
    gpointer view = g_object_new(VIDEO_SETTINGS_VIEW_TYPE, NULL);

    return (GtkWidget *)view;
}
