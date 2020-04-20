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

    /* this is used to keep track of the state of the preview when the settings
     * are opened; if a call is in progress, then the preview should already be
     * started and we don't want to stop it when the settings are closed, in this
     * case */
    gboolean video_started_by_settings;

    QMetaObject::Connection local_renderer_connection;
    QMetaObject::Connection device_event_connection;
    QMetaObject::Connection audio_meter_connection;

    /* hardware accel settings */
    GtkWidget *checkbutton_hardware_acceleration;

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
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(widgets->checkbutton_hardware_acceleration),
        avModel_->getHardwareAcceleration());

    auto activeIdx = 0;
    auto currentManager = avModel_->getAudioManager();
    auto i = 0;
    for (const auto& manager : avModel_->getSupportedAudioManagers()) {
        if (manager == currentManager) {
            activeIdx = i;
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_manager), nullptr, qUtf8Printable(manager));
        i++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_manager), activeIdx);
    drawAudioDevices();
    drawVideoDevices();

    gtk_level_bar_set_value(GTK_LEVEL_BAR(widgets->levelbar_input), 0.0);
}

void
CppImpl::drawAudioDevices()
{
    if (!avModel_) {
        g_warning("AVModel not initialized yet");
        return;
    }
    auto activeOutput = 0, activeRingtone = 0;
    auto currentOutput = avModel_->getOutputDevice();
    auto currentRingtone = avModel_->getRingtoneDevice();
    auto i = 0;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_ringtone));
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_output));
    for (const auto& output : avModel_->getAudioOutputDevices()) {
        if (output == currentOutput) {
            activeOutput = i;
        }
        if (output == currentRingtone) {
            activeRingtone = i;
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_ringtone), nullptr, qUtf8Printable(output));
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_output), nullptr, qUtf8Printable(output));
        i++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_ringtone), activeRingtone);
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_output), activeOutput);

    auto activeInput = 0;
    auto currentInput = avModel_->getInputDevice();
    i = 0;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_input));
    for (const auto& input : avModel_->getAudioInputDevices()) {
        if (input == currentInput) {
            activeInput = i;
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_input), nullptr, qUtf8Printable(input));
        i++;
    }
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
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_framerate));
    for (const auto& rate : rates) {
        auto rateStr = QString::number(static_cast<uint8_t>(rate));
        if (rateStr == currentRate) {
            active = i;
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_framerate), nullptr, qUtf8Printable(rateStr));
        i++;
    }
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
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_resolution));
    for (const auto& item : resToRates) {
        if (item.first == currentRes) {
            active = i;
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_resolution), nullptr, qUtf8Printable(item.first));
        i++;
    }
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
    auto active = 0;
    auto currentDevice = avModel_->getDefaultDevice();
    QString currentChannel = "";
    try {
        currentChannel = avModel_->getDeviceSettings(currentDevice).channel;
    } catch (const std::out_of_range&) {
        g_warning("drawChannels out_of_range exception");
        return;
    }
    auto i = 0;
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_channel));
    for (const auto& capabilites : avModel_->getDeviceCapabilities(currentDevice).toStdMap()) {
        if (capabilites.first == currentChannel) {
            active = i;
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_channel), nullptr, qUtf8Printable(capabilites.first));
        i++;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(widgets->combobox_channel), active);
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
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_device));
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_channel));
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_resolution));
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(widgets->combobox_framerate));
    if (current == "") {
        // Avoid to draw devices if no camera is selected
        return;
    }
    auto i = 0;
    for (const auto& device : avModel_->getDevices()) {
        if (device == current) {
            active = i;
        }
        try {
            auto name = avModel_->getDeviceSettings(device).name;
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(widgets->combobox_device), nullptr, qUtf8Printable(name));
        } catch (...) {}
        i++;
    }
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
hardware_acceleration_toggled(GtkToggleButton *toggle_button, MediaSettingsView *self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    gboolean hardware_acceleration = gtk_toggle_button_get_active(toggle_button);
    priv->cpp->avModel_->setHardwareAcceleration(hardware_acceleration);
}

static void
set_audio_manager(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* audio_manager = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_manager));
    if (audio_manager) {
        priv->cpp->avModel_->setAudioManager(audio_manager);
        priv->cpp->drawAudioDevices();
    }
}

static void
set_ringtone_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* ringtone_device = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_ringtone));
    if (ringtone_device) {
        priv->cpp->avModel_->setRingtoneDevice(ringtone_device);
    }
}

static void
set_output_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* output_device = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_output));
    if (output_device) {
        priv->cpp->avModel_->setOutputDevice(output_device);
    }
}

static void
set_input_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* input_device = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_input));
    if (input_device) {
        priv->cpp->avModel_->setInputDevice(input_device);
    }
}

static void
set_video_device(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* device_name = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_device));
    if (device_name) {
        auto device_id = priv->cpp->avModel_->getDeviceIdFromName(device_name);
        if (device_id.isEmpty()) {
            g_warning("set_video_device couldn't find device: %s", device_name);
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
    }
}

static void
set_channel(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* video_channel = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_channel));
    if (video_channel) {
        auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
        try {
            auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
            if (settings.channel == video_channel) return;
            settings.channel = video_channel;
            priv->cpp->avModel_->setDeviceSettings(settings);
        } catch (const std::out_of_range&) {
            g_warning("set_channel out_of_range exception");
            return;
        }
        priv->cpp->drawChannels();
    }
}

static void
set_resolution(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* video_resolution = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_resolution));
    if (video_resolution) {
        auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
        try {
            auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
            if (settings.size == video_resolution) return;
            settings.size = video_resolution;
            priv->cpp->avModel_->setDeviceSettings(settings);
        } catch (const std::out_of_range&) {
            g_warning("set_resolution out_of_range exception");
            return;
        }
        priv->cpp->drawFramerates();
    }
}

static void
set_framerate(MediaSettingsView* self)
{
    g_return_if_fail(IS_MEDIA_SETTINGS_VIEW(self));
    MediaSettingsViewPrivate *priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* video_framerate = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priv->combobox_framerate));
    if (video_framerate) {
        auto currentDevice = priv->cpp->avModel_->getDefaultDevice();
        try {
            auto settings = priv->cpp->avModel_->getDeviceSettings(currentDevice);
            if (settings.rate == std::stoi(video_framerate)) return;
            settings.rate = std::stoi(video_framerate);
            priv->cpp->avModel_->setDeviceSettings(settings);
        } catch (...) {
            g_debug("Cannot convert framerate.");
        }
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
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, checkbutton_hardware_acceleration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), MediaSettingsView, levelbar_input);
}

GtkWidget *
media_settings_view_new(lrc::api::AVModel& avModel)
{
    auto self = g_object_new(MEDIA_SETTINGS_VIEW_TYPE, NULL);
    auto* priv = MEDIA_SETTINGS_VIEW_GET_PRIVATE(self);
    priv->cpp = new details::CppImpl(
        *(reinterpret_cast<MediaSettingsView*>(self)), avModel
    );

    // CppImpl ctor
    g_signal_connect(priv->checkbutton_hardware_acceleration, "toggled",
        G_CALLBACK(hardware_acceleration_toggled), self);
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

        try {
            const lrc::api::video::Renderer* previewRenderer =
                &priv->cpp->avModel_->getRenderer(
                lrc::api::video::PREVIEW_RENDERER_ID);
            priv->video_started_by_settings = previewRenderer->isRendering();
            if (priv->video_started_by_settings) {
                video_widget_add_new_renderer(VIDEO_WIDGET(priv->video_widget),
                    priv->cpp->avModel_, previewRenderer, VIDEO_RENDERER_REMOTE);
            } else {
                priv->video_started_by_settings = true;
                priv->device_event_connection = QObject::connect(
                    &*priv->cpp->avModel_,
                    &lrc::api::AVModel::deviceEvent,
                    [=]() {
                        priv->cpp->drawVideoDevices();
                    });
                priv->local_renderer_connection = QObject::connect(
                    &*priv->cpp->avModel_,
                    &lrc::api::AVModel::rendererStarted,
                    [=](const QString& id) {
                        if (id != lrc::api::video::PREVIEW_RENDERER_ID)
                            return;
                        video_widget_add_new_renderer(
                            VIDEO_WIDGET(priv->video_widget),
                            priv->cpp->avModel_,
                            previewRenderer, VIDEO_RENDERER_REMOTE);
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
            gtk_container_remove(GTK_CONTAINER(priv->vbox_main), priv->video_widget);
        priv->video_widget = NULL;
        priv->cpp->avModel_->setAudioMeterState(false);
        priv->cpp->avModel_->stopAudioDevice();
    }
}
