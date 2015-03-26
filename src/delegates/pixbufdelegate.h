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

#ifndef PIXBUFDELEGATE_H
#define PIXBUFDELEGATE_H

#include <gtk/gtk.h>
#include <memory>
#include <delegates/pixmapmanipulationdelegate.h>

Q_DECLARE_METATYPE(std::shared_ptr<GdkPixbuf>);

class Person;

class PixbufDelegate : public PixmapManipulationDelegate {
    constexpr static int FALLBACK_AVATAR_SIZE {100};
public:
    PixbufDelegate();

    QVariant callPhoto(Call* c, const QSize& size, bool displayPresence = true) override;
    QVariant callPhoto(const ContactMethod* n, const QSize& size, bool displayPresence = true) override;
    QVariant contactPhoto(Person* c, const QSize& size, bool displayPresence = true) override;
    QVariant personPhoto(const QByteArray& data, const QString& type = "PNG") override;

private:
    std::shared_ptr<GdkPixbuf> scaleAndFrame(const GdkPixbuf *photo, const QSize& size);
    std::shared_ptr<GdkPixbuf> fallbackAvatar_;
};

#endif /* PIXBUFDELEGATE_H */