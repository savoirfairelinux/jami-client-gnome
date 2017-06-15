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

#include "mediasettingsview.h"

#include <gtk/gtk.h>
#include "models/gtkqtreemodel.h"
#include "video/video_widget.h"
#include <video/previewmanager.h>
#include <video/configurationproxy.h>
#include <QtCore/QItemSelectionModel>
#include <audio/settings.h>
#include <audio/managermodel.h>
#include <audio/alsapluginmodel.h>
#include <audio/outputdevicemodel.h>
#include <audio/inputdevicemodel.h>
#include <audio/ringtonedevicemodel.h>

struct _MediaSettingsView
{
    GtkScrolledWindow parent;
};

struct _MediaSettingsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _MediaSettingsViewPrivate MediaSettingsViewPrivate;

struct _MediaSettingsViewPrivate
{
    GtkWidget *vbox_main;

    /* audio settings */
    GtkWidget *combobox_manager;
    GtkWidget *combobox_ringtone;
    GtkWidget *combobox_output;
    GtkWidget *combobox_input;

    QMetaObject::Connection manager_selection;
    QMetaObject::Connection ringtone_selection;
    QMetaObject::Connection output_selection;
    QMetaObject::Connection input_selection;

    /* camera settings */
    GtkWidget *combobox_device;
    GtkWidget *combobox_channel;
    GtkWidget *combobox_resolution;
    GtkWidget *combobox_framerate;
    GtkWidget *video_widget;

    /* this is used to keep track of the state of the preview when the settings
     * are opened; if a call is in progress, then the preview should already be
     * started and we don't want to stop it when the settings are closed, in this
     * case */
    gboolean video_started_by_settings;

    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection device_selection;
    QMetaObject::Connection channel_selection;
    QMetaObject::Connection resolution_selection;
    QMetaObject::Connection rate_selection;

    /* hardware accel settings */
    GtkWidget *checkbutton_hardware_decoding;
};

G_DEFINE_TYPE_WITH_PRIVATE(MediaSettingsView, media_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define MEDIA_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MEDIA_SETTINGS_VIEW_TYPE, MediaSettingsViewPrivate))

static void
media_settings_view_dispose(GObject *object)
{
    MediaSettingsView *view = MEDIA_SETTINGS_VIEW(object);
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(view);

    /* make sure to stop the preview if this view is getting destroyed */
    if (priv->video_started_by_settings) {
        Video::PreviewManager::instance().stopPreview();
        priv->video_started_by_settings = FALSE;
    }

    QObject::disconnect(priv->manager_selection);
    QObject::disconnect(priv->ringtone_selection);
    QObject::disconnect(priv->output_selection);
    QObject::disconnect(priv->input_selection);

    QObject::disconnect(priv->local_renderer_connection);
    QObject::disconnect(priv->device_selection);
    QObject::disconnect(priv->channel_selection);
    QObject::disconnect(priv->resolution_selection);
    QObject::disconnect(priv->rate_selection);

    G_OBJECT_CLASS(media_settings_view_parent_class)->dispose(object);
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
                                                0, Qt::DisplayRole, G_TYPE_STRING);

    gtk_combo_box_set_model(box, GTK_TREE_MODEL(model));
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(box), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(box), renderer,
                                   "text", 0, NULL);
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    /* connect signals to and from the selection model */
    connection = QObject::connect(
        selection_model,
        &QItemSelectionModel::currentChanged,
        [=](const QModelIndex & current, G_GNUC_UNUSED const QModelIndex & previous) {
            /* select the current */
            if (current.isValid()) {
                GtkTreeIter new_iter;
                GtkTreeModel *model = gtk_combo_box_get_model(box);
                g_return_if_fail(model);
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
hardware_decoding_toggled(GtkToggleButton *toggle_button, MediaSettingsView *self)
{
    gboolean hardware_decoding = gtk_toggle_button_get_active(toggle_button);
    Video::ConfigurationProxy::setDecodingAccelerated(hardware_decoding);
}

static void
media_settings_view_init(MediaSettingsView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));

    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(view);

    /* video settings */
    /* TODO: this is a workaround for a possible LRC issue
     *       make sure all the models are instantiated before making a selection
     */
    Video::ConfigurationProxy::rateModel();
    Video::ConfigurationProxy::resolutionModel();
    Video::ConfigurationProxy::channelModel();
    Video::ConfigurationProxy::deviceModel();

    priv->device_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_device),
                                                      &Video::ConfigurationProxy::deviceModel(),
                                                      &Video::ConfigurationProxy::deviceSelectionModel());
    priv->channel_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_channel),
                                                       &Video::ConfigurationProxy::channelModel(),
                                                       &Video::ConfigurationProxy::channelSelectionModel());
    priv->resolution_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_resolution),
                                                          &Video::ConfigurationProxy::resolutionModel(),
                                                          &Video::ConfigurationProxy::resolutionSelectionModel());
    priv->rate_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_framerate),
                                                    &Video::ConfigurationProxy::rateModel(),
                                                    &Video::ConfigurationProxy::rateSelectionModel());

    /* audio settings */
    /* TODO: this is a workaround for a possible LRC issue
     *       make sure all the models are instantiated before making a selection
     */
    Audio::Settings::instance().alsaPluginModel();
    Audio::Settings::instance().ringtoneDeviceModel();
    Audio::Settings::instance().inputDeviceModel();
    Audio::Settings::instance().outputDeviceModel();
    priv->manager_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_manager),
                                                       Audio::Settings::instance().managerModel(),
                                                       Audio::Settings::instance().managerModel()->selectionModel());
    priv->ringtone_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_ringtone),
                                                        Audio::Settings::instance().ringtoneDeviceModel(),
                                                        Audio::Settings::instance().ringtoneDeviceModel()->selectionModel());
    priv->input_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_input),
                                                     Audio::Settings::instance().inputDeviceModel(),
                                                     Audio::Settings::instance().inputDeviceModel()->selectionModel());
    priv->output_selection = connect_combo_box_qmodel(GTK_COMBO_BOX(priv->combobox_output),
                                                      Audio::Settings::instance().outputDeviceModel(),
                                                      Audio::Settings::instance().outputDeviceModel()->selectionModel());

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->checkbutton_hardware_decoding), Video::ConfigurationProxy::getDecodingAccelerated());
    g_signal_connect(priv->checkbutton_hardware_decoding, "toggled", G_CALLBACK(hardware_decoding_toggled), view);
}

static void
media_settings_view_class_init(MediaSettingsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = media_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/mediasettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, vbox_main);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_manager);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_ringtone);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_output);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_device);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_channel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_resolution);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_framerate);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, checkbutton_hardware_decoding);
}

GtkWidget *
media_settings_view_new()
{
    gpointer view = g_object_new(MEDIA_SETTINGS_VIEW_TYPE, NULL);

    return (GtkWidget *)view;
}

void
media_settings_view_show_preview(MediaSettingsView *self, gboolean show_preview)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);

    /* if TRUE, create a VideoWidget, then check if the preview has already been
     * started (because a call was in progress); if not, then start it.
     * if FALSE, check if the preview was started by this function, if so
     * then stop the preview; then destroy the VideoWidget to make sure we don't
     * get useless frame updates */

    if (show_preview) {
        /* put video widget in */
        priv->video_widget = video_widget_new();
        gtk_widget_show_all(priv->video_widget);
        gtk_box_pack_start(GTK_BOX(priv->vbox_main), priv->video_widget, TRUE, TRUE, 0);
        // set minimum size for video so it doesn't shrink too much
        gtk_widget_set_size_request(priv->video_widget, 400, -1);

        if (Video::PreviewManager::instance().isPreviewing()) {
            priv->video_started_by_settings = FALSE;

            /* local renderer, but set as "remote" so that it takes up the whole screen */
            video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                           Video::PreviewManager::instance().previewRenderer(),
                                           VIDEO_RENDERER_REMOTE);
        } else {
            priv->video_started_by_settings = TRUE;
            priv->local_renderer_connection = QObject::connect(
                &Video::PreviewManager::instance(),
                &Video::PreviewManager::previewStarted,
                [=](Video::Renderer *renderer) {
                    video_widget_push_new_renderer(VIDEO_WIDGET(priv->video_widget),
                                                renderer,
                                                VIDEO_RENDERER_REMOTE);
                }
            );
            Video::PreviewManager::instance().startPreview();
        }
    } else {
        if (priv->video_started_by_settings) {
            Video::PreviewManager::instance().stopPreview();
            QObject::disconnect(priv->local_renderer_connection);
            priv->video_started_by_settings = FALSE;
        }

        if (priv->video_widget && IS_VIDEO_WIDGET(priv->video_widget))
            gtk_container_remove(GTK_CONTAINER(priv->vbox_main), priv->video_widget);
        priv->video_widget = NULL;
    }

}
