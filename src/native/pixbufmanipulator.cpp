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

#include "pixbufmanipulator.h"

#include <QtCore/QSize>
#include <QtCore/QMetaType>
#include <memory>

#include <string>
#include <algorithm>

// lrc
#include <api/contactmodel.h>
#include <api/conversation.h>
#include <api/account.h>
#include <api/contact.h>

namespace Interfaces {

PixbufManipulator::PixbufManipulator()
    : conferenceAvatar_{draw_conference_avatar(FALLBACK_AVATAR_SIZE), g_object_unref}
{
}

std::shared_ptr<GdkPixbuf>
PixbufManipulator::temporaryItemAvatar() const
{
    GError *error = nullptr;
    std::shared_ptr<GdkPixbuf> result(
        gdk_pixbuf_new_from_resource("/net/jami/JamiGnome/temporary-item", &error),
        g_object_unref
    );

    if (result == nullptr) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
        return generateAvatar("", "");
    }
    return result;
}

std::shared_ptr<GdkPixbuf>
PixbufManipulator::generateAvatar(const std::string& alias, const std::string& uri) const
{
    std::string letter = {};
    if (!alias.empty()) {
        // Use QString for special characters like ø for example
        letter = QString(QString(alias.c_str()).toUpper().at(0)).toStdString();
    }
    auto color = 10; // 10 = default color (light grey)
    if (uri.length() > 0) {
        auto* md5 = g_compute_checksum_for_string(G_CHECKSUM_MD5, uri.c_str(), -1);
        color = std::string("0123456789abcdef").find(md5[0]);
    }

    return std::shared_ptr<GdkPixbuf> {
        draw_fallback_avatar(
            FALLBACK_AVATAR_SIZE,
            letter,
            color
        ),
        g_object_unref
    };
}

std::shared_ptr<GdkPixbuf>
PixbufManipulator::scaleAndFrame(const GdkPixbuf *photo,
                                 const QSize& size,
                                 bool displayInformation,
                                 IconStatus status,
                                 uint unreadMessages)
{
    /**
     * for now, respect the height requested
     * the framing process will add another 10px, so account for that
     * when scaling the photos
     */

    int height = size.height();
    if (size.height() != size.width())
        g_warning("requested contact photo width != height; only respecting the height as the largest dimension");
    int photo_h = height - 10;
    int photo_w = photo_h;

    /* scale photo, make sure to respect the request height as the largest dimension*/
    int w = gdk_pixbuf_get_width(photo);
    int h = gdk_pixbuf_get_height(photo);
    if (h > w)
        photo_w = w * ((double)photo_h / h);
    if (w > h)
        photo_h = h * ((double)photo_w / w);

    std::unique_ptr<GdkPixbuf, decltype(g_object_unref)&> scaled_photo{
        gdk_pixbuf_scale_simple(photo, photo_w, photo_h, GDK_INTERP_BILINEAR),
        g_object_unref};

    /* frame photo */
    std::shared_ptr<GdkPixbuf> result {
        frame_avatar(scaled_photo.get()),
        g_object_unref
    };

    /* draw information */
    if (displayInformation) {
        /* draw status */
        result.reset(draw_status(result.get(), status), g_object_unref);

        /* draw visual notification for unread messages */
        if (unreadMessages)
            result.reset(draw_unread_messages(result.get(), unreadMessages), g_object_unref);
    }

    return result;
}

QVariant PixbufManipulator::personPhoto(const QByteArray& data, const QString& type)
{
    Q_UNUSED(type);
    /* Try to load the image from the data provided by lrc vcard utils;
     * lrc is getting the image data assuming that it is inlined in the vcard,
     * for now URIs are not supported.
     *
     * The format of the data should be either base 64 or ascii (hex), try both
     */

    GError *error = NULL;
    GdkPixbuf *pixbuf = NULL;
    GInputStream *stream = NULL;

    /* first try using base64 */
    QByteArray ba64 = QByteArray::fromBase64(data);
    stream = g_memory_input_stream_new_from_data(ba64.constData(),
                                                 ba64.size(),
                                                 NULL);

    pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
    g_input_stream_close(stream, NULL, NULL);
    g_object_unref(stream);

    if (!pixbuf) {
        // g_debug("failed decoding person photo using base64: %s", error->message);
        g_clear_error(&error);

        /* failed with base64, try hex */
        QByteArray baHex = QByteArray::fromHex(data);
        stream = g_memory_input_stream_new_from_data(baHex.constData(),
                                                     baHex.size(),
                                                     NULL);

        pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
        g_input_stream_close(stream, NULL, NULL);
        g_object_unref(stream);

        if (!pixbuf) {
            // g_debug("failed decoding person photo using hex (ASCII): %s", error->message);
            g_clear_error(&error);
        }
    }

    if (pixbuf) {
        std::shared_ptr<GdkPixbuf> avatar(pixbuf, g_object_unref);
        return QVariant::fromValue(avatar);
    }

    /* could not load image, return emtpy QVariant */
    return QVariant();
}

QVariant
PixbufManipulator::conversationPhoto(const lrc::api::conversation::Info& conversationInfo,
                                     const lrc::api::account::Info& accountInfo,
                                     const QSize& size,
                                     bool displayInformation)
{
    auto contacts = conversationInfo.participants;
    if (!contacts.empty()) {
        try {
            // Get first contact photo
            auto contactUri = contacts.front();
            auto contactInfo = accountInfo.contactModel->getContact(contactUri);
            auto contactPhoto = contactInfo.profileInfo.avatar;
            auto alias = contactInfo.profileInfo.alias;
            alias.remove('\r');
            alias.remove('\n');
            auto bestName = alias.isEmpty() ? contactInfo.registeredName : alias;
            auto unreadMessages = conversationInfo.unreadMessages;
            auto status = contactInfo.isPresent? IconStatus::PRESENT : IconStatus::ABSENT;
            if (accountInfo.profileInfo.type == lrc::api::profile::Type::SIP && contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY)
            {
                return QVariant::fromValue(scaleAndFrame(generateAvatar("", "").get(), size, displayInformation, status));
            } else if (accountInfo.profileInfo.type == lrc::api::profile::Type::SIP) {
                return QVariant::fromValue(scaleAndFrame(generateAvatar("", "sip:" + contactInfo.profileInfo.uri.toStdString()).get(), size, displayInformation, status));
            } else if (contactInfo.profileInfo.type == lrc::api::profile::Type::TEMPORARY && contactInfo.profileInfo.uri.isEmpty()) {
                return QVariant::fromValue(scaleAndFrame(temporaryItemAvatar().get(), size, false, status, unreadMessages));
            } else if (!contactPhoto.isEmpty()) {
                QByteArray byteArray = contactPhoto.toUtf8();
                QVariant photo = personPhoto(byteArray);
                if (GDK_IS_PIXBUF(photo.value<std::shared_ptr<GdkPixbuf>>().get())) {
                    return QVariant::fromValue(scaleAndFrame(
                        photo.value<std::shared_ptr<GdkPixbuf>>().get(), size,
                        displayInformation, status, unreadMessages));
                } else {
                    return QVariant::fromValue(scaleAndFrame(
                        generateAvatar(bestName.toStdString(),
                            "ring:" + contactInfo.profileInfo.uri.toStdString()).get(),
                        size, displayInformation, status, unreadMessages));
                }
            } else {
                return QVariant::fromValue(scaleAndFrame(generateAvatar(bestName.toStdString(),
                     "ring:" + contactInfo.profileInfo.uri.toStdString()).get(), size, displayInformation, status, unreadMessages));
            }
        } catch (...) {}
    }
    return QVariant::fromValue(scaleAndFrame(temporaryItemAvatar().get(), size, displayInformation));

}

QVariant
PixbufManipulator::numberCategoryIcon(const QVariant& p, const QSize& size, bool displayInformation, bool isPresent)
{
    Q_UNUSED(p)
    Q_UNUSED(size)
    Q_UNUSED(displayInformation)
    Q_UNUSED(isPresent)
    return QVariant();
}

QByteArray
PixbufManipulator::toByteArray(const QVariant& pxm)
{
    std::shared_ptr<GdkPixbuf> pixbuf_photo = pxm.value<std::shared_ptr<GdkPixbuf>>();

    if(pixbuf_photo.get()) {
        gchar* png_buffer = nullptr;
        gsize png_buffer_size;
        GError *error = nullptr;

        gdk_pixbuf_save_to_buffer(pixbuf_photo.get(), &png_buffer, &png_buffer_size, "png", &error, NULL);
        QByteArray array = QByteArray(png_buffer, png_buffer_size);

        g_free(png_buffer);

        if (error != NULL) {
            g_warning("in toByteArray, gdk_pixbuf_save_to_buffer failed : %s\n", error->message);
            g_clear_error(&error);
        }

        return array;
    } else {
        g_debug("in toByteArray, failed to retrieve data from parameter pxm");
        return QByteArray();
    }
}

QVariant
PixbufManipulator::userActionIcon(const UserActionElement& state) const
{
    Q_UNUSED(state)
    return QVariant();
}

QVariant PixbufManipulator::decorationRole(const QModelIndex& index)
{
    Q_UNUSED(index)
    return QVariant();
}

QVariant PixbufManipulator::decorationRole(const lrc::api::conversation::Info& conversation,
                                           const lrc::api::account::Info& accountInfo)
{
    Q_UNUSED(conversation)
    Q_UNUSED(accountInfo)
    return QVariant();
}

} // namespace Interfaces
