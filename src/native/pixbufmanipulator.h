/*
 *  Copyright (C) 2015-2021 Savoir-faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
 *  Author: Sebastien Blin <sebastien.blin@savoirfairelinux.com>
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

#pragma once

#include <gtk/gtk.h>
#include <memory>
#include <interfaces/pixmapmanipulatori.h>
#include "../utils/drawing.h"

Q_DECLARE_METATYPE(std::shared_ptr<GdkPixbuf>);

class Person;

namespace Interfaces {

/**
 * TODO remove old methods (methods which use Call, ContactMethod, Person, etc)
 * But before, they should be removed from PixmapManipulatorI
 */
class PixbufManipulator : public PixmapManipulatorI {
    constexpr static int FALLBACK_AVATAR_SIZE {100};
public:
    PixbufManipulator();

    QVariant conversationPhoto(const lrc::api::conversation::Info& conversation,
                               const lrc::api::account::Info& accountInfo,
                               const QSize& size,
                               bool displayInformation = true) override;
    QVariant personPhoto(const QByteArray& data, const QString& type = "PNG") override;

    QVariant   numberCategoryIcon(const QVariant& p, const QSize& size, bool displayInformation = false, bool isPresent = false) override;
    QByteArray toByteArray(const QVariant& pxm) override;
    QVariant   userActionIcon(const UserActionElement& state) const override;
    QVariant   decorationRole(const QModelIndex& index) override;
    QVariant   decorationRole(const lrc::api::conversation::Info& conversation,
                              const lrc::api::account::Info& accountInfo) override;

    // Helpers
    std::shared_ptr<GdkPixbuf> temporaryItemAvatar() const;
    std::shared_ptr<GdkPixbuf> generateAvatar(const std::string& alias, const std::string& uri) const;

    std::shared_ptr<GdkPixbuf> scaleAndFrame(const GdkPixbuf *photo, const QSize &size, bool displayInformation = false, IconStatus status = IconStatus::INVALID, uint unreadMessages = 0);

  private:
    std::shared_ptr<GdkPixbuf> conferenceAvatar_;
};

} // namespace Interfaces
