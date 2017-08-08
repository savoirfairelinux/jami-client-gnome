/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
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

#include "pixbufmanipulator.h"

#include "../utils/drawing.h"
#include <QtCore/QSize>
#include <QtCore/QMetaType>
#include <person.h>
#include <memory>
#include <call.h>
#include <contactmethod.h>
#include <string>
#include <algorithm>
#include <smartlistitem.h>

namespace Interfaces {

PixbufManipulator::PixbufManipulator()
    : conferenceAvatar_{ring_draw_conference_avatar(FALLBACK_AVATAR_SIZE), g_object_unref}
{
}

std::shared_ptr<GdkPixbuf>
PixbufManipulator::generateAvatar(const ContactMethod* cm) const
{
    auto cm_number = QString("0");
    auto letter = QChar('R'); // R for ring
    if (cm) {
        auto hashName = cm->uri().userinfo();
        if (hashName.size() > 0) {
            cm_number = hashName.at(0);
        }
        // Get the letter to draw
        if (!cm->bestName().isEmpty()) {
            // Prioritize the name
            letter = cm->bestName().toUpper().at(0);
        } else if (!cm->bestId().isEmpty()) {
            // If the contact has no name, use the id
            letter = cm->bestId().toUpper().at(0);
        } else {
            // R for ring is used
        }
    }

    bool ok;
    auto color = cm_number.toUInt(&ok, 16);
    if (!ok) color = 0;

    return std::shared_ptr<GdkPixbuf> {
        ring_draw_fallback_avatar(
            FALLBACK_AVATAR_SIZE,
            letter.toLatin1(),
            color
        ),
        g_object_unref
    };
}

std::shared_ptr<GdkPixbuf>
PixbufManipulator::generateAvatar(const std::string& alias, const std::string& uri) const
{
    auto name = alias;
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    auto letter = name.length() > 0 ? name[0] : 'R';
    auto color = uri.length() > 0 ? std::stoi(std::string(1, uri[0]), 0, 16) : 0;

    return std::shared_ptr<GdkPixbuf> {
        ring_draw_fallback_avatar(
            FALLBACK_AVATAR_SIZE,
            letter,
            color
        ),
        g_object_unref
    };
}


std::shared_ptr<GdkPixbuf>
PixbufManipulator::scaleAndFrame(const GdkPixbuf *photo, const QSize& size, bool display_presence, bool is_present)
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
        ring_frame_avatar(scaled_photo.get()),
        g_object_unref
    };

    /* draw presence */
    if (display_presence)
        result.reset(ring_draw_presence(result.get(), is_present), g_object_unref);

    return result;
}

QVariant
PixbufManipulator::callPhoto(Call* c, const QSize& size, bool displayPresence)
{
    if (c->type() == Call::Type::CONFERENCE) {
        /* conferences are always "online" */
        return QVariant::fromValue(scaleAndFrame(conferenceAvatar_.get(), size, displayPresence, TRUE));
    }
    return callPhoto(c->peerContactMethod(), size, displayPresence);
}

QVariant
PixbufManipulator::callPhoto(const ContactMethod* n, const QSize& size, bool displayPresence)
{
    if (n->contact()) {
        return contactPhoto(n->contact(), size, displayPresence);
    } else {
        return QVariant::fromValue(scaleAndFrame(generateAvatar(n).get(), size, displayPresence, n->isPresent()));
    }
}

QVariant
PixbufManipulator::itemPhoto(const SmartListItem* item, const QSize& size, bool displayPresence)
{
    auto avatar = item->getAvatar();
    if (avatar.length() > 0)
    {
        QByteArray byteArray(avatar.c_str(), avatar.length());
        QVariant photo = personPhoto(byteArray);
        return QVariant::fromValue(scaleAndFrame(photo.value<std::shared_ptr<GdkPixbuf>>().get(), size, displayPresence, true));
    } else {
        return QVariant::fromValue(scaleAndFrame(generateAvatar(item->getAlias(), item->getTitle()).get(), size, displayPresence, true));
    }

}


QVariant
PixbufManipulator::contactPhoto(Person* c, const QSize& size, bool displayPresence)
{
    /**
     * try to get the photo
     * otherwise use the generated avatar
     */

    std::shared_ptr<GdkPixbuf> photo;

    if (c->photo().isValid())
        photo = c->photo().value<std::shared_ptr<GdkPixbuf>>();
    else {
        auto cm = c->phoneNumbers().size() > 0 ? c->phoneNumbers().first() : nullptr;
        photo = generateAvatar(cm);
    }

    return QVariant::fromValue(scaleAndFrame(photo.get(), size, displayPresence, c->isPresent()));
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
PixbufManipulator::numberCategoryIcon(const QVariant& p, const QSize& size, bool displayPresence, bool isPresent)
{
    Q_UNUSED(p)
    Q_UNUSED(size)
    Q_UNUSED(displayPresence)
    Q_UNUSED(isPresent)
    return QVariant();
}

QVariant
PixbufManipulator::securityIssueIcon(const QModelIndex& index)
{
    Q_UNUSED(index)
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
PixbufManipulator::collectionIcon(const CollectionInterface* interface, PixmapManipulatorI::CollectionIconHint hint) const
{
    Q_UNUSED(interface)
    Q_UNUSED(hint)
    return QVariant();
}
QVariant
PixbufManipulator::securityLevelIcon(const SecurityEvaluationModel::SecurityLevel level) const
{
    Q_UNUSED(level)
    return QVariant();
}
QVariant
PixbufManipulator::historySortingCategoryIcon(const CategorizedHistoryModel::SortedProxy::Categories cat) const
{
    Q_UNUSED(cat)
    return QVariant();
}
QVariant
PixbufManipulator::contactSortingCategoryIcon(const CategorizedContactModel::SortedProxy::Categories cat) const
{
    Q_UNUSED(cat)
    return QVariant();
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

QVariant PixbufManipulator::decorationRole(const Call* c)
{
    Q_UNUSED(c)
    return QVariant();
}

QVariant PixbufManipulator::decorationRole(const ContactMethod* cm)
{
    Q_UNUSED(cm)
    return QVariant();
}

QVariant PixbufManipulator::decorationRole(const Person* p)
{
    Q_UNUSED(p)
    return QVariant();
}

QVariant PixbufManipulator::decorationRole(const Account* p)
{
    Q_UNUSED(p)
    return QVariant();
}

QVariant PixbufManipulator::decorationRole(const SmartListItem* i)
{
    Q_UNUSED(i)
    return QVariant();
}

} // namespace Interfaces
