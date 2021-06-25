/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
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

#include "generalsettingsview.h"

// GTK+ related
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib.h>

// LRC
#include <api/avmodel.h>
#include <api/datatransfermodel.h>
#include <api/newaccountmodel.h>

// Jami Client
#include "utils/files.h"
#include "avatarmanipulation.h"

namespace { namespace details {
class CppImpl;
}}

enum
{
  PROP_MAIN_WIN_PNT = 1,
};

struct _GeneralSettingsView
{
    GtkScrolledWindow parent;
};

struct _GeneralSettingsViewClass
{
    GtkScrolledWindowClass parent_class;
};

typedef struct _GeneralSettingsViewPrivate GeneralSettingsViewPrivate;

struct _GeneralSettingsViewPrivate
{
    GSettings *settings;

    /* Jami settings */
    GtkWidget *at_startup_button;
    GtkWidget *systray_button;
    GtkWidget *incoming_open_button;
    GtkWidget *call_notifications_button;
    GtkWidget *request_notifications_button;
    GtkWidget *chat_notifications_button;
    GtkWidget *media_chatview_button;
    GtkWidget *typing_indication_button;
    GtkWidget *chatview_pos_button;
    GtkWidget *button_choose_downloads_directory;

    /* history settings */
    GtkWidget* adjustment_history_duration;
    GtkWidget *button_clear_history;

    /* Call recording settings */
    GtkWidget *record_preview_button;
    GtkWidget *always_record_button;
    GtkWidget *adjustment_record_quality;
    GtkWidget *filechooserbutton_record_path;

    /* main window pointer */
    GtkWidget* main_window_pnt;

    /* file transfer settings */
    GtkWidget* accept_untrusted_button;
    GtkWidget* transfer_limit_button;
    GtkWidget* spinbutton_limit_transfer;
    GtkWidget* adjustment_file_transfer;

    details::CppImpl* cpp; ///< Non-UI and C++ only code
};

G_DEFINE_TYPE_WITH_PRIVATE(GeneralSettingsView, general_settings_view, GTK_TYPE_SCROLLED_WINDOW);

#define GENERAL_SETTINGS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GENERAL_SETTINGS_VIEW_TYPE, GeneralSettingsViewPrivate))

namespace { namespace details {

class CppImpl
{
public:
    explicit CppImpl(GeneralSettingsView& widget, lrc::api::AVModel& avModel, lrc::api::NewAccountModel& accountModel);

    lrc::api::AVModel* avModel_ = nullptr;
    lrc::api::NewAccountModel* accountModel_ = nullptr;
    GeneralSettingsView* self = nullptr; // The GTK widget itself
    GeneralSettingsViewPrivate* widgets = nullptr;
};

CppImpl::CppImpl(GeneralSettingsView& widget, lrc::api::AVModel& avModel, lrc::api::NewAccountModel& accountModel)
    : self(&widget)
    , widgets(GENERAL_SETTINGS_VIEW_GET_PRIVATE(&widget))
    , avModel_(&avModel)
    , accountModel_(&accountModel)
{
    gtk_switch_set_active(
        GTK_SWITCH(widgets->record_preview_button),
        avModel_->getRecordPreview());
    gtk_switch_set_active(
        GTK_SWITCH(widgets->always_record_button),
        avModel_->getAlwaysRecord());
    gtk_file_chooser_set_action(
        GTK_FILE_CHOOSER(widgets->filechooserbutton_record_path),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_filename(
        GTK_FILE_CHOOSER(widgets->filechooserbutton_record_path),
        qUtf8Printable(avModel_->getRecordPath()));
    gtk_adjustment_set_value(
        GTK_ADJUSTMENT(widgets->adjustment_record_quality),
        avModel_->getRecordQuality());
}

}} // namespace details

enum {
    CLEAR_ALL_HISTORY,
    UPDATE_DOWNLOAD_FOLDER,
    LAST_SIGNAL
};

static guint general_settings_view_signals[LAST_SIGNAL] = { 0 };

static void
general_settings_view_dispose(GObject *object)
{
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(object);

    g_clear_object(&priv->settings);

    G_OBJECT_CLASS(general_settings_view_parent_class)->dispose(object);
}

static gboolean
clear_history_dialog(GeneralSettingsView *self)
{
    gboolean response = FALSE;
    GtkWidget *dialog = gtk_message_dialog_new(NULL,
                            (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                            _("This is a destructive operation. Are you sure you want to delete all of your chat and call history?"));

    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);

    /* get parent window so we can center on it */
    GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(self));
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

static void
clear_history(G_GNUC_UNUSED GtkWidget *button, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));

    if (clear_history_dialog(self) )
        g_signal_emit(G_OBJECT(self), general_settings_view_signals[CLEAR_ALL_HISTORY], 0);
}

static void
update_downloads_button_label(GeneralSettingsView *self)
{
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    // Get folder name
    const gchar *folder_dirname = g_path_get_basename(g_variant_get_string(g_settings_get_value(priv->settings, "download-folder"), NULL));
    gtk_button_set_label(GTK_BUTTON(priv->button_choose_downloads_directory), folder_dirname);
}

static void
change_prefered_directory (const gchar *directory, GeneralSettingsView *self, const gchar *id, void (*cb)(GeneralSettingsView*))
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    priv->settings = g_settings_new_full(get_settings_schema(), NULL, NULL);
    g_settings_set_value(priv->settings, id, g_variant_new("s", directory));
    cb(self);
}

static void
choose_downloads_directory(GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));

    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    gint res;
    gchar* filename = nullptr;

    if (!priv->main_window_pnt) {
        g_debug("Internal error: NULL main window pointer in GeneralSettingsView.");
        return;
    }

#if GTK_CHECK_VERSION(3,20,0)
    GtkFileChooserNative *native = gtk_file_chooser_native_new(
        _("Choose download folder"),
        GTK_WINDOW(priv->main_window_pnt),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        _("_Save"),
        _("_Cancel"));

    res = gtk_native_dialog_run(GTK_NATIVE_DIALOG(native));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(native));
    }

    g_object_unref(native);
#else
    GtkWidget *dialog = gtk_file_chooser_dialog_new (
        _("Choose download folder"),
        GTK_WINDOW(priv->main_window_pnt),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Save"), GTK_RESPONSE_ACCEPT,
        NULL);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    }

    gtk_widget_destroy (dialog);
#endif

    if (!filename) return;

    // set download folder
    change_prefered_directory(filename, self, "download-folder", update_downloads_button_label);

    g_signal_emit(G_OBJECT(self), general_settings_view_signals[UPDATE_DOWNLOAD_FOLDER], 0, filename);
}

static void
preview_toggled(GtkSwitch *button, GParamSpec* /*spec*/, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    gboolean preview = gtk_switch_get_active(button);
    if (priv && priv->cpp && priv->cpp->avModel_)
        priv->cpp->avModel_->setRecordPreview(preview);
}

static void
always_record_toggled(GtkSwitch *button, GParamSpec* /*spec*/, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    gboolean rec = gtk_switch_get_active(button);
    if (priv && priv->cpp && priv->cpp->avModel_)
        priv->cpp->avModel_->setAlwaysRecord(rec);
}

static void
record_quality_changed(GtkAdjustment *adjustment, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    gdouble qual = gtk_adjustment_get_value(adjustment);
    if (priv && priv->cpp && priv->cpp->avModel_)
        priv->cpp->avModel_->setRecordQuality(qual);
}

static void
file_transfer_limit_changed(GtkAdjustment *adjustment, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    gdouble limit = gtk_adjustment_get_value(adjustment);
    if (priv && priv->cpp && priv->cpp->accountModel_)
        priv->cpp->accountModel_->autoTransferSizeThreshold = limit;
}

static void
accept_untrusted_toggled(GtkSwitch *button, GParamSpec* /*spec*/, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    gboolean accept = gtk_switch_get_active(button);
    if (priv && priv->cpp && priv->cpp->accountModel_)
        priv->cpp->accountModel_->autoTransferFromUnstrusted = accept;
}

static void
accept_transfer_toggled(GtkSwitch *button, GParamSpec* /*spec*/, GeneralSettingsView *self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    gboolean accept = gtk_switch_get_active(button);
    gtk_widget_set_sensitive(priv->spinbutton_limit_transfer, accept);
    if (priv && priv->cpp && priv->cpp->accountModel_) {
        priv->cpp->accountModel_->autoTransferFromTrusted = accept;
    }
}

static void
update_record_path(GtkFileChooser *file_chooser, GeneralSettingsView* self)
{
    g_return_if_fail(IS_GENERAL_SETTINGS_VIEW(self));
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);
    auto* filename = gtk_file_chooser_get_filename(file_chooser);
    if (filename != priv->cpp->avModel_->getRecordPath())
        priv->cpp->avModel_->setRecordPath(filename);
    g_free(filename);
}

static void
general_settings_view_init(GeneralSettingsView *self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    priv->settings = g_settings_new_full(get_settings_schema(), NULL, NULL);

    GtkStyleContext* context;
    context = gtk_widget_get_style_context(GTK_WIDGET(priv->button_clear_history));
    gtk_style_context_add_class(context, "button_red");

    /* bind client option to gsettings */
    g_settings_bind(priv->settings, "start-on-login",
                    priv->at_startup_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "show-status-icon",
                    priv->systray_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "bring-window-to-front",
                    priv->incoming_open_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-call-notifications",
                    priv->call_notifications_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-display-links",
                    priv->media_chatview_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-typing-indication",
                    priv->typing_indication_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-pending-notifications",
                    priv->request_notifications_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "enable-chat-notifications",
                    priv->chat_notifications_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "chat-pane-horizontal",
                    priv->chatview_pos_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "history-limit",
                    priv->adjustment_history_duration, "value",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "accept-untrusted-transfer",
                    priv->accept_untrusted_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "accept-trusted-transfer",
                    priv->transfer_limit_button, "active",
                    G_SETTINGS_BIND_DEFAULT);
    g_settings_bind(priv->settings, "accept-size-transfer",
                    priv->adjustment_file_transfer, "value",
                    G_SETTINGS_BIND_DEFAULT);

    /* Set-up download directory settings, default if none specified */
    auto* download_directory_variant = g_settings_get_value(priv->settings, "download-folder");
    char* download_directory_value;
    g_variant_get(download_directory_variant, "&s", &download_directory_value);
    auto current_value = std::string(download_directory_value);
    if (current_value.empty()) {
        // First time Jami is opened
        std::string default_download_dir = lrc::api::DataTransferModel::createDefaultDirectory().toStdString();
        g_settings_set_value(priv->settings, "download-folder", g_variant_new("s", default_download_dir.c_str()));
        g_settings_set_boolean(priv->settings, "migrated-from-homedir", true);
    } else if (!g_settings_get_boolean(priv->settings, "migrated-from-homedir")) {
        // Migrate from HOME directory to the new download directory
        const auto* home_dir = g_get_home_dir ();
        if (home_dir && current_value == home_dir) {
            std::string default_download_dir = lrc::api::DataTransferModel::createDefaultDirectory().toStdString();
            g_settings_set_value(priv->settings, "download-folder", g_variant_new("s", default_download_dir.c_str()));
        }
        g_settings_set_boolean(priv->settings, "migrated-from-homedir", true);
    }
    update_downloads_button_label(self);

    /* clear history */
    g_signal_connect(priv->button_clear_history, "clicked", G_CALLBACK(clear_history), self);

    /* Recording callbacks */
    g_signal_connect(priv->record_preview_button, "notify::active", G_CALLBACK(preview_toggled), self);
    g_signal_connect(priv->always_record_button, "notify::active", G_CALLBACK(always_record_toggled), self);
    g_signal_connect(priv->adjustment_record_quality, "value-changed", G_CALLBACK(record_quality_changed), self);
    g_signal_connect(priv->filechooserbutton_record_path, "file-set", G_CALLBACK(update_record_path), self);

    g_signal_connect(priv->adjustment_file_transfer, "value-changed", G_CALLBACK(file_transfer_limit_changed), self);
    g_signal_connect(priv->accept_untrusted_button, "notify::active", G_CALLBACK(accept_untrusted_toggled), self);
    g_signal_connect(priv->transfer_limit_button, "notify::active", G_CALLBACK(accept_transfer_toggled), self);

    gtk_widget_set_sensitive(priv->spinbutton_limit_transfer, g_settings_get_boolean(priv->settings, "accept-trusted-transfer"));
}


static void
general_settings_view_set_property (GObject      *object,
                                    guint         property_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
    GeneralSettingsView *self = GENERAL_SETTINGS_VIEW (object);
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_MAIN_WIN_PNT) {
        GtkWidget *main_window_pnt = (GtkWidget*) g_value_get_pointer(value);

        if (!main_window_pnt) {
            g_debug("Internal error: NULL main window pointer passed to set_property");
            return;
        }

        priv->main_window_pnt = main_window_pnt;
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
general_settings_view_get_property (GObject    *object,
                                    guint       property_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
    GeneralSettingsView *self = GENERAL_SETTINGS_VIEW (object);
    GeneralSettingsViewPrivate *priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(self);

    if (property_id == PROP_MAIN_WIN_PNT) {
        g_value_set_pointer(value, priv->main_window_pnt);
    }
    else {
        // Invalid property id passed
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
general_settings_view_class_init(GeneralSettingsViewClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->dispose = general_settings_view_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/net/jami/JamiGnome/generalsettingsview.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, at_startup_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, systray_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, incoming_open_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, call_notifications_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, media_chatview_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, typing_indication_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, request_notifications_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, chat_notifications_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, chatview_pos_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, adjustment_history_duration);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, button_clear_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, button_choose_downloads_directory);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, record_preview_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, always_record_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, adjustment_record_quality);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, filechooserbutton_record_path);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, accept_untrusted_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, transfer_limit_button);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, spinbutton_limit_transfer);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), GeneralSettingsView, adjustment_file_transfer);

    general_settings_view_signals[CLEAR_ALL_HISTORY] = g_signal_new (
        "clear-all-history",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    general_settings_view_signals[UPDATE_DOWNLOAD_FOLDER] = g_signal_new("update-download-folder",
        G_TYPE_FROM_CLASS(klass),
        (GSignalFlags) (G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION),
        0,
        nullptr,
        nullptr,
        g_cclosure_marshal_VOID__BOOLEAN,
        G_TYPE_NONE,
        1, G_TYPE_STRING);

    /* Define class properties: e.g. pointer to main window, etc.*/
    object_class->set_property = general_settings_view_set_property;
    object_class->get_property = general_settings_view_get_property;

    GParamFlags flags = (GParamFlags) (G_PARAM_READWRITE);
    g_object_class_install_property (object_class, PROP_MAIN_WIN_PNT,
        g_param_spec_pointer ("main_window_pnt",
                              "MainWindow pointer",
                              "Pointer to the Main Window. This property is used by modal dialogs.",
                              flags));
}

GtkWidget *
general_settings_view_new(GtkWidget* main_window_pointer, lrc::api::AVModel& avModel, lrc::api::NewAccountModel& accountModel)
{
    auto self = g_object_new(GENERAL_SETTINGS_VIEW_TYPE, NULL);
    auto* priv = GENERAL_SETTINGS_VIEW_GET_PRIVATE(GENERAL_SETTINGS_VIEW (self));
    priv->cpp = new details::CppImpl(
        *(reinterpret_cast<GeneralSettingsView*>(self)),
        avModel, accountModel
    );

    // set_up ring main window pointer (needed by modal dialogs)
    GValue val = G_VALUE_INIT;
    g_value_init (&val, G_TYPE_POINTER);
    g_value_set_pointer (&val, main_window_pointer);
    g_object_set_property (G_OBJECT (self), "main_window_pnt", &val);
    g_value_unset (&val);

    g_signal_connect_swapped(priv->button_choose_downloads_directory, "clicked", G_CALLBACK(choose_downloads_directory), self);

    // CSS styles
    auto provider = gtk_css_provider_new();
    std::string css = ".button_red { color: white; background: #dc3a37; border: 0; }";
    css += ".button_red:hover { background: #dc2719; }";
    gtk_css_provider_load_from_data(provider, css.c_str(), -1, nullptr);
    gtk_style_context_add_provider_for_screen(gdk_display_get_default_screen(gdk_display_get_default()),
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    return (GtkWidget *)self;
}
