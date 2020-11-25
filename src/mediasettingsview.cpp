/*
 *  Copyright (C) 2015-2020 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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
#include <string>

// LRC
#include <api/avmodel.h>
#include <api/newvideo.h>

#include "video/video_widget.h"

namespace { namespace details
{
class CppImpl;
}}

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
    GtkWidget *levelbar_input;

    /* camera settings */
    GtkWidget *combobox_device;
    GtkWidget *combobox_channel;
    GtkWidget *combobox_resolution;
    GtkWidget *combobox_framerate;
    GtkWidget *video_widget;
    GtkWidget *preview_box;
    GtkWidget *no_camera_row;
    GtkWidget *preview_row;
    GtkWidget *video_device_row;
    GtkWidget *video_channel_row;
    GtkWidget *video_resolution_row;
    GtkWidget *video_framerate_row;

    /* this is used to keep track of the state of the preview when the settings
     * are opened; if a call is in progress, then the preview should already be
     * started and we don't want to stop it when the settings are closed, in this
     * case */
    gboolean video_started_by_settings;

    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection device_event_connection;
    QMetaObject::Connection audio_meter_connection;

    /* hardware accel settings */
    GtkWidget *hardware_accel_button;

    details::CppImpl* cpp; ///< Non-UI and C++ only code
};

G_DEFINE_TYPE_WITH_PRIVATE(MediaSettingsView, media_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define MEDIA_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), MEDIA_SETTINGS_VIEW_TYPE, MediaSettingsViewPrivate))

namespace { namespace details
{

class CppImpl
{
public:
    explicit CppImpl(MediaSettingsView& widget, lrc::api::AVModel& avModel);

    void drawAudioDevices();
    void drawFramerates();
    void drawResolutions();
    void drawChannels();
    void drawVideoDevices();

    lrc::api::AVModel* avModel_ = nullptr;
    MediaSettingsView* self = nullptr; // The GTK widget itself
    MediaSettingsViewPrivate* widgets = nullptr;
};

CppImpl::CppImpl(MediaSettingsView& widget, lrc::api::AVModel& avModel)
: self {&widget}
, widgets {MEDIA_SETTINGS_VIEW_GET_PRIVATE(&widget)}
, avModel_(&avModel)
{
    gtk_switch_set_active(
        GTK_SWITCH(widgets->hardware_accel_button),
        avModel_->getHardwareAcceleration());

    auto activeIdx = 0;
    auto currentManager = avModel_->getAudioManager();
    auto i = 0;
    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    GtkTreeIter iter;
    for (const auto& manager : avModel_->getSupportedAudioManagers()) {
        if (manager == currentManager) {
            activeIdx = i;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                            0 /* col # */ , i /* celldata */,
                            1 /* col # */ , manager.toStdString().c_str() /* celldata */,
                            -1 /* end */);
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_manager),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_manager), activeIdx);

    gtk_level_bar_set_value(GTK_LEVEL_BAR(widgets->levelbar_input), 0.0);
    gtk_widget_set_size_request(widgets->levelbar_input, 390, -1);

    drawAudioDevices();
    drawVideoDevices();
}

void
CppImpl::drawAudioDevices()
{
    if (!avModel_) {
        g_warning("AVModel not initialized yet");
        return;
    }
    auto activeOutput = 0, activeInput = 0, activeRingtone = 0;
    auto currentOutput = avModel_->getOutputDevice();
    auto currentInput = avModel_->getInputDevice();
    auto currentRingtone = avModel_->getRingtoneDevice();
    auto i = 0;

    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    GtkTreeIter iter;
    for (const auto& output : avModel_->getAudioOutputDevices()) {
        if (output == currentOutput) {
            activeOutput = i;
        }
        if (output == currentRingtone) {
            activeRingtone = i;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                            0 /* col # */ , i /* celldata */,
                            1 /* col # */ , output.toStdString().c_str() /* celldata */,
                            -1 /* end */);
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_ringtone),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_output),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_ringtone), activeRingtone);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_output), activeOutput);

    store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    i = 0;
    for (const auto& input : avModel_->getAudioInputDevices()) {
        if (input == currentInput) {
            activeInput = i;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                            0 /* col # */ , i /* celldata */,
                            1 /* col # */ , input.toStdString().c_str() /* celldata */,
                            -1 /* end */);
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_input),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_input), activeInput);
}

void
CppImpl::drawFramerates()
{
    if (!avModel_) {
        g_warning("AVModel not initialized yet");
        return;
    }
    using namespace lrc::api;
    auto active = 0;
    auto currentDevice = avModel_->getDefaultDevice();
    auto deviceCaps = avModel_->getDeviceCapabilities(currentDevice);
    QString currentChannel = "", currentRes = "", currentRate = "";
    int currentResIndex;
    try {
        auto deviceSettings = avModel_->getDeviceSettings(currentDevice);
        currentChannel = deviceSettings.channel;
        currentRes = deviceSettings.size;
        currentRate = QString::number(static_cast<uint8_t>(deviceSettings.rate));
        if (deviceCaps.find(currentChannel) == deviceCaps.end()) return;
        auto resRates = deviceCaps.value(currentChannel);
        auto it = std::find_if(resRates.begin(), resRates.end(),
            [&currentRes](const QPair<video::Resolution, video::FrameratesList>& element) {
                return element.first == currentRes;
            });
        if (it == resRates.end()) {
            throw std::out_of_range("Can't find resolution");
        }
        currentResIndex = std::distance(resRates.begin(), it);
    } catch (const std::out_of_range&) {
        g_warning("drawFramerates out_of_range exception");
        return;
    }
    if (deviceCaps.find(currentChannel) == deviceCaps.end()
        || deviceCaps.value(currentChannel).size() < currentResIndex)
        return;
    auto rates = deviceCaps.value(currentChannel).value(currentResIndex).second;
    auto i = 0;
    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    GtkTreeIter iter;
    for (const auto& rate : rates) {
        auto rateStr = QString::number(static_cast<uint8_t>(rate));
        if (rateStr == currentRate) {
            active = i;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0 /* col # */ , i /* celldata */,
            1 /* col # */ , rateStr.toStdString().c_str() /* celldata */,
            -1 /* end */);
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_framerate),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_framerate), active);
}

void
CppImpl::drawResolutions()
{
    if (!avModel_) {
        g_warning("AVModel not initialized yet");
        return;
    }
    auto active = 0;
    auto currentDevice = avModel_->getDefaultDevice();
    QString currentChannel = "", currentRes = "";
    try {
        currentChannel = avModel_->getDeviceSettings(currentDevice).channel;
        currentRes = avModel_->getDeviceSettings(currentDevice).size;
    } catch (const std::out_of_range&) {
        g_warning("drawResolutions out_of_range exception");
        return;
    }
    auto capabilities = avModel_->getDeviceCapabilities(currentDevice);
    if (capabilities.find(currentChannel) == capabilities.end()) return;
    auto resToRates = avModel_->getDeviceCapabilities(currentDevice).value(currentChannel);
    auto i = 0;
    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    GtkTreeIter iter;
    for (const auto& item : resToRates) {
        if (item.first == currentRes) {
            active = i;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0 /* col # */ , i /* celldata */,
            1 /* col # */ , item.first.toStdString().c_str() /* celldata */,
            -1 /* end */);
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_resolution),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_resolution), active);
    drawFramerates();
}

void
CppImpl::drawChannels()
{
    if (!avModel_) {
        g_warning("AVModel not initialized yet");
        return;
    }
    auto currentDevice = avModel_->getDefaultDevice();
    QString currentChannel = "";
    try {
        currentChannel = avModel_->getDeviceSettings(currentDevice).channel;
    } catch (const std::out_of_range&) {
        g_warning("drawChannels out_of_range exception");
        return;
    }
    auto active = 0;
    auto i = 0;
    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    GtkTreeIter iter;
    for (const auto& capabilites : avModel_->getDeviceCapabilities(currentDevice).toStdMap()) {
        if (capabilites.first == currentChannel) {
            active = i;
        }
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
            0 /* col # */ , i /* celldata */,
            1 /* col # */ , capabilites.first.toStdString().c_str() /* celldata */,
            -1 /* end */);
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_channel),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_channel), active);
    if (i <= 1)
        gtk_widget_hide(widgets->video_channel_row);
    drawResolutions();
}

void
CppImpl::drawVideoDevices()
{
    if (!avModel_) {
        g_warning("AVModel not initialized yet");
        return;
    }
    auto active = 0;
    auto current = avModel_->getDefaultDevice();
    // Clean comboboxes
    auto storeEmpty = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    gtk_combo_box_set_model(GTK_COMBO_BOX(widgets->combobox_device), GTK_TREE_MODEL(storeEmpty));
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_device), -1);
    gtk_combo_box_set_model(GTK_COMBO_BOX(widgets->combobox_channel), GTK_TREE_MODEL(storeEmpty));
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_channel), -1);
    gtk_combo_box_set_model(GTK_COMBO_BOX(widgets->combobox_resolution), GTK_TREE_MODEL(storeEmpty));
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_resolution), -1);
    gtk_combo_box_set_model(GTK_COMBO_BOX(widgets->combobox_framerate), GTK_TREE_MODEL(storeEmpty));
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_framerate), -1);
    if (current == "") {
        // Avoid to draw devices if no camera is selected
        gtk_widget_show(widgets->no_camera_row);
        gtk_widget_hide(widgets->preview_row);
        gtk_widget_hide(widgets->video_device_row);
        gtk_widget_hide(widgets->video_channel_row);
        gtk_widget_hide(widgets->video_resolution_row);
        gtk_widget_hide(widgets->video_framerate_row);
        if (widgets->video_widget)
            avModel_->stopPreview();
        return;
    }
    if (gtk_widget_get_visible(widgets->no_camera_row)) {
        gtk_widget_hide(widgets->no_camera_row);
        gtk_widget_show(widgets->preview_row);
        gtk_widget_show(widgets->video_device_row);
        gtk_widget_show(widgets->video_channel_row);
        gtk_widget_show(widgets->video_resolution_row);
        gtk_widget_show(widgets->video_framerate_row);
        if (widgets->video_widget) {
            avModel_->startPreview();
        }
    }

    active = 0;
    auto i = 0;
    auto store = gtk_list_store_new(2 /* # of cols */ ,
                                G_TYPE_UINT,
                                G_TYPE_STRING);
    GtkTreeIter iter;
    for (const auto& device : avModel_->getDevices()) {
        if (device == current) {
            active = i;
        }
        try {
            auto name = avModel_->getDeviceSettings(device).name;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                                0 /* col # */ , i /* celldata */,
                                1 /* col # */ , name.toStdString().c_str() /* celldata */,
                                -1 /* end */);
        } catch (...) {}
        i++;
    }
    gtk_combo_box_set_model(
        GTK_COMBO_BOX(widgets->combobox_device),
        GTK_TREE_MODEL(store)
    );
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_device), active);
    drawChannels();
}

}} // namespace details



static void
media_settings_view_dispose(GObject *object)
{
    MediaSettingsView *view = MEDIA_SETTINGS_VIEW(object);
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(view);

    /* make sure to stop the preview if this view is getting destroyed */
    if (priv->video_started_by_settings) {
        priv->cpp->avModel_->stopPreview();
        priv->video_started_by_settings = FALSE;
    }

    QObject::disconnect(priv->local_renderer_connection);
    QObject::disconnect(priv->device_event_connection);
    QObject::disconnect(priv->audio_meter_connection);

    G_OBJECT_CLASS(media_settings_view_parent_class)->dispose(object);
}

static void
update_hardware_accel(GObject*, GParamSpec*, MediaSettingsView *self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto newState = gtk_switch_get_active(GTK_SWITCH(priv->hardware_accel_button));
    priv->cpp->avModel_->setHardwareAcceleration(newState);
}

static void
set_audio_manager(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_manager);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            priv->cpp->avModel_->setAudioManager(text);
            priv->cpp->drawAudioDevices();
        }
        g_free(text);
    }
}

static void
set_ringtone_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_ringtone);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            priv->cpp->avModel_->setRingtoneDevice(text);
        }
        g_free(text);
    }
}

static void
set_output_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_output);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            priv->cpp->avModel_->setOutputDevice(text);
        }
        g_free(text);
    }
}

static void
set_input_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_input);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            priv->cpp->avModel_->setInputDevice(text);
        }
        g_free(text);
    }
}

static void
set_video_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_device);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            auto device_id = priv->cpp->avModel_->getDeviceIdFromName(text);
            if (device_id.isEmpty()) {
                g_warning("set_video_device couldn't find device: %s", text);
                return;
            }
            auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
            if (currentDevice == device_id) return;
            priv->cpp->avModel_->setDefaultDevice(device_id);
            try {
                auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
                priv->cpp->avModel_->setDeviceSettings(settings);
            } catch (const std::out_of_range&) {
                g_warning("set_video_device out_of_range exception");
            }
            priv->cpp->drawVideoDevices();
            priv->cpp->avModel_->startPreview();
        }
        g_free(text);
    }
}

static void
set_channel(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_channel);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
            try {
                auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
                if (settings.channel == text) return;
                settings.channel = text;
                priv->cpp->avModel_->setDeviceSettings(settings);
            } catch (const std::out_of_range&) {
                g_warning("set_channel out_of_range exception");
                return;
            }
            priv->cpp->drawChannels();
        }
        g_free(text);
    }
}

static void
set_resolution(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_resolution);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
            try {
                auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
                if (settings.size == text) return;
                settings.size = text;
                priv->cpp->avModel_->setDeviceSettings(settings);
            } catch (const std::out_of_range&) {
                g_warning("set_resolution out_of_range exception");
                return;
            }
            priv->cpp->drawFramerates();
        }
        g_free(text);
    }
}

static void
set_framerate(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto combobox = GTK_COMBO_BOX(priv->combobox_framerate);
    auto model = gtk_combo_box_get_model(combobox);

    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combobox, &iter)) {
        gchar* text;
        gtk_tree_model_get(model, &iter, 1 /* col# */, &text /* data */, -1);
        if (text) {
            auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
            try {
                auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
                if (settings.rate == std::stoi(text)) return;
                settings.rate = std::stoi(text);
                priv->cpp->avModel_->setDeviceSettings(settings);
            } catch (...) {
                g_debug("Cannot convert framerate.");
            }
        }
        g_free(text);
    }
}

static void
media_settings_view_init(MediaSettingsView *view)
{
    gtk_widget_init_template(GTK_WIDGET(view));
}

static void
media_settings_view_class_init(MediaSettingsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = media_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/mediasettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, vbox_main);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_manager);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_ringtone);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_output);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_device);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_channel);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_resolution);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, combobox_framerate);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, hardware_accel_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, levelbar_input);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, preview_box);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, no_camera_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, preview_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, video_device_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, video_channel_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, video_resolution_row);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, video_framerate_row);
}

static void
print_data(GtkCellLayout*,
           GtkCellRenderer* cell,
           GtkTreeModel* model,
           GtkTreeIter* iter,
           gpointer*)
{
    gchar *text;

    gtk_tree_model_get(model, iter, 1 /* col# */, &text /* data */, -1);

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_object_set(G_OBJECT(cell), "height", 17, NULL);
    g_object_set(G_OBJECT(cell), "ypad", 0, NULL);

    g_free(text);
}

GtkWidget *
media_settings_view_new(lrc::api::AVModel& avModel)
{
    auto self = g_object_new(MEDIA_SETTINGS_VIEW_TYPE, NULL);
    auto* priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    priv->cpp = new details::CppImpl(
        *(reinterpret_cast<MediaSettingsView*>(self)), avModel
    );


    auto renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    g_object_set(G_OBJECT(renderer), "width-chars", 50, NULL);
    g_object_set(G_OBJECT(renderer), "max-width-chars", 50, NULL);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_manager), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_manager),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_ringtone), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_ringtone),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_output), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_output),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_input), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_input),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_device), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_device),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_channel), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_channel),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_resolution), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_resolution),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->combobox_framerate), renderer, true);
    gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->combobox_framerate),
                                       renderer,
                                       (GtkCellLayoutDataFunc )print_data,
                                       priv, nullptr);

    // CppImpl ctor
    g_signal_connect(priv->hardware_accel_button, "notify::active",
        G_CALLBACK(update_hardware_accel), self);
    g_signal_connect_swapped(priv->combobox_manager, "changed",
        G_CALLBACK(set_audio_manager), self);
    g_signal_connect_swapped(priv->combobox_ringtone, "changed",
        G_CALLBACK(set_ringtone_device), self);
    g_signal_connect_swapped(priv->combobox_output, "changed",
        G_CALLBACK(set_output_device), self);
    g_signal_connect_swapped(priv->combobox_input, "changed",
        G_CALLBACK(set_input_device), self);
    g_signal_connect_swapped(priv->combobox_device, "changed",
        G_CALLBACK(set_video_device), self);
    g_signal_connect_swapped(priv->combobox_channel, "changed",
        G_CALLBACK(set_channel), self);
    g_signal_connect_swapped(priv->combobox_resolution, "changed",
        G_CALLBACK(set_resolution), self);
    g_signal_connect_swapped(priv->combobox_framerate, "changed",
        G_CALLBACK(set_framerate), self);
    priv->audio_meter_connection = QObject::connect(
        &*priv->cpp->avModel_,
        &lrc::api::AVModel::audioMeter,
        [=](const QString& id, float level) {
            if (id == "audiolayer_id")
                gtk_level_bar_set_value(GTK_LEVEL_BAR(priv->levelbar_input), level);
        });
    return (GtkWidget *)self;
}

void
media_settings_view_show_preview(MediaSettingsView *self, gboolean show_preview)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);

    if (priv->cpp) {
        // Update view (we don't listen for events while not displaying)
        priv->cpp->drawAudioDevices();
        priv->cpp->drawVideoDevices();
    }

    /* if TRUE, create a VideoWidget, then check if the preview has already been
     * started (because a call was in progress); if not, then start it.
     * if FALSE, check if the preview was started by this function, if so
     * then stop the preview; then destroy the VideoWidget to make sure we don't
     * get useless frame updates */

    if (show_preview) {
        /* put video widget in */
        priv->video_widget = video_widget_new();
        gtk_widget_show_all(priv->video_widget);
        #if GTK_CHECK_VERSION(3,22,0)
            GdkRectangle workarea = {};
            gdk_monitor_get_workarea(
                gdk_display_get_primary_monitor(gdk_display_get_default()),
                &workarea);
            gtk_widget_set_size_request(priv->preview_box, workarea.height/3, workarea.height/4);
        #else
            gtk_widget_set_size_request(priv->preview_box, gdk_screen_height()/3, gdk_screen_height()/4);
        #endif

        gtk_box_pack_start(GTK_BOX(priv->preview_box), priv->video_widget, TRUE, TRUE, 0);
        // set minimum size for video so it doesn't shrink too much
        gtk_widget_set_size_request(priv->video_widget, 400, -1);

        try {
            const lrc::api::video::Renderer* previewRenderer = video_widget_get_renderer(VIDEO_WIDGET(priv->video_widget), VIDEO_RENDERER_REMOTE);
            priv->video_started_by_settings = previewRenderer && previewRenderer->isRendering();
            if (priv->video_started_by_settings) {
                video_widget_add_new_renderer(VIDEO_WIDGET(priv->video_widget),
                    priv->cpp->avModel_, previewRenderer, VIDEO_RENDERER_REMOTE);
            } else {
                priv->video_started_by_settings = true;
                priv->device_event_connection = QObject::connect(
                    &*priv->cpp->avModel_,
                    &lrc::api::AVModel::deviceEvent,
                    [=]() {
                        priv->cpp->drawAudioDevices();
                        priv->cpp->drawVideoDevices();
                    });
                priv->local_renderer_connection = QObject::connect(
                    &*priv->cpp->avModel_,
                    &lrc::api::AVModel::rendererStarted,
                    [=](const QString& id) {
                        if (id != lrc::api::video::PREVIEW_RENDERER_ID)
                            return;
                        const lrc::api::video::Renderer* prenderer = &priv->cpp->avModel_->getRenderer(id);
                        video_widget_add_new_renderer(
                            VIDEO_WIDGET(priv->video_widget),
                            priv->cpp->avModel_,
                            prenderer, VIDEO_RENDERER_REMOTE);
                    });
                priv->cpp->avModel_->startPreview();
            }
        } catch (const std::out_of_range& e) {
            g_warning("Cannot start preview");
        }
        priv->cpp->avModel_->startAudioDevice();
        priv->cpp->avModel_->setAudioMeterState(true);
    } else {
        if (priv->video_started_by_settings) {
            priv->cpp->avModel_->stopPreview();
            QObject::disconnect(priv->local_renderer_connection);
            QObject::disconnect(priv->device_event_connection);
            priv->video_started_by_settings = FALSE;
        }

        if (priv->video_widget && IS_VIDEO_WIDGET(priv->video_widget))
            gtk_container_remove(GTK_CONTAINER(priv->preview_box), priv->video_widget);
        priv->video_widget = NULL;
        priv->cpp->avModel_->setAudioMeterState(false);
        priv->cpp->avModel_->stopAudioDevice();
    }
}
