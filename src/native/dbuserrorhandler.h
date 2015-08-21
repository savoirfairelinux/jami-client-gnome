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

#ifndef DBUSERRORHANDLER_H
#define DBUSERRORHANDLER_H

#include <gtk/gtk.h>
#include <interfaces/dbuserrorhandleri.h>
#include <atomic>

namespace Interfaces {

class DBusErrorHandler : public DBusErrorHandlerI {
public:
    void connectionError(const QString& error) override;
    void invalidInterfaceError(const QString& error) override;

    void finishedHandlingError();
private:
    /* keeps track if we're in the process of handling an error already, so that we don't keep
     * displaying error dialogs; we use an atomic in case the errors come from multiple threads */
    std::atomic_bool handlingError{false};
};

} // namespace Interfaces

#endif /* DBUSERRORHANDLER_H */
