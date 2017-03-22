/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
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

#include "namenumberfilterproxymodel.h"

// LRC
#include <itemdataroles.h>
#include <person.h>
#include <contactmethod.h>
#include <call.h>

NameNumberFilterProxy::NameNumberFilterProxy(QAbstractItemModel* sourceModel)
{
    setParent(sourceModel);
    setSourceModel(sourceModel);
}

bool
NameNumberFilterProxy::filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
{
    // we filter the regex only on Calls, Contacts and ContactMethods; however we don't want to display
    // the top nodes (Categories) if they have no children
    if (!source_parent.isValid() && filterRegExp().isEmpty()) {
        return true;
    } else if (!source_parent.isValid()) {
        // check if there are any children, don't display the categroy if not
        auto idx = sourceModel()->index(source_row, 0, source_parent);
        if (!idx.isValid())
            return false;

        for (int row = 0; row <  sourceModel()->rowCount(idx); ++row) {
            if (filterAcceptsRow(row, idx)) {
                return true;
            }
        }
        return false;
    } else {
        auto idx = sourceModel()->index(source_row, 0, source_parent);

        if (!idx.isValid()) {
            return false;
        }

        //we want to filter on name and number; note that Person object may have many numbers
        if (idx.data(static_cast<int>(Ring::Role::Name)).toString().contains(filterRegExp())) {
            return true;
        } else {
            auto type = idx.data(static_cast<int>(Ring::Role::ObjectType));
            auto object = idx.data(static_cast<int>(Ring::Role::Object));

            if (!type.isValid() || !object.isValid()) {
                return false;
            }

            switch (type.value<Ring::ObjectType>()) {
                case Ring::ObjectType::Person:
                {
                    auto p = object.value<Person *>();
                    for (auto cm : p->phoneNumbers()) {
                        if (cm->uri().full().contains(filterRegExp())) {
                            return true;
                        }
                    }
                    return false;
                }
                break;
                case Ring::ObjectType::ContactMethod:
                {
                    auto cm = object.value<ContactMethod *>();
                    return cm->uri().full().contains(filterRegExp());
                }
                break;
                case Ring::ObjectType::Call:
                {
                    auto call = object.value<Call *>();
                    return call->peerContactMethod()->uri().full().contains(filterRegExp());
                }
                break;
                case Ring::ObjectType::Media:
                case Ring::ObjectType::Certificate:
                case Ring::ObjectType::TrustRequest:
                case Ring::ObjectType::COUNT__:
                break;
            }

        }
        return false; // no matches
    }
}
