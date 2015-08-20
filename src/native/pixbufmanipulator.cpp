/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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
 *  terms of the OpenSSL or SSLeay licenses, Savoir-faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "pixbufmanipulator.h"

#include "../utils/drawing.h"
#include <QtCore/QSize>
#include <QtCore/QMetaType>
#include <person.h>
#include <memory>
#include <call.h>
#include <contactmethod.h>

namespace Interfaces {

PixbufManipulator::PixbufManipulator()
    : fallbackAvatar_{ring_draw_fallback_avatar(FALLBACK_AVATAR_SIZE), g_object_unref}
{
}

std::shared_ptr<GdkPixbuf>
PixbufManipulator::scaleAndFrame(const GdkPixbuf *photo, const QSize& size)
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
    return {ring_frame_avatar(scaled_photo.get()), g_object_unref};
}

QVariant
PixbufManipulator::callPhoto(Call* c, const QSize& size, bool displayPresence)
{
    return callPhoto(c->peerContactMethod(), size, displayPresence);
}

QVariant
PixbufManipulator::callPhoto(const ContactMethod* n, const QSize& size, bool displayPresence)
{
    if (n->contact()) {
        return contactPhoto(n->contact(), size, displayPresence);
    } else {
        return QVariant::fromValue(scaleAndFrame(fallbackAvatar_.get(), size));
    }
}

QVariant
PixbufManipulator::contactPhoto(Person* c, const QSize& size, bool displayPresence)
{
    Q_UNUSED(displayPresence);

    /**
     * try to get the photo
     * otherwise use the fallback avatar
     */

    std::shared_ptr<GdkPixbuf> photo;

    if (c->photo().isValid())
        photo = c->photo().value<std::shared_ptr<GdkPixbuf>>();
    else
        photo = fallbackAvatar_;

    return QVariant::fromValue(scaleAndFrame(photo.get(), size));
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
    Q_UNUSED(pxm);
    return QByteArray();
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

} // namespace Interfaces
