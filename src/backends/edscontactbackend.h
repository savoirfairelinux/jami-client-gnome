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

#ifndef EDSCONTACTBACKEND_H
#define EDSCONTACTBACKEND_H

/**
 * QT_NO_KEYWORDS is needed to resolve the conflict between the symbols used
 * in this C library (ie: signals) and Qt keywords
 */
#define QT_NO_KEYWORDS
#include <libebook/libebook.h>
#undef QT_NO_KEYWORDS

#include <collectioninterface.h>
#include <memory>

class Person;

template<typename T> class CollectionMediator;

void load_eds_sources();

class EdsContactBackend : public CollectionInterface
{
public:
    explicit EdsContactBackend(CollectionMediator<Person>* mediator, EClient *client, CollectionInterface* parent = nullptr);
    virtual ~EdsContactBackend();

    virtual bool load() override;
    virtual bool reload() override;
    virtual bool clear() override;
    virtual QString    name     () const override;
    virtual QString    category () const override;
    virtual bool       isEnabled() const override;
    virtual QByteArray id       () const override;
    virtual SupportedFeatures  supportedFeatures() const override;

    void parseContacts(GSList *contacts);

private:
   CollectionMediator<Person>*  mediator_;
   std::unique_ptr<EClient, decltype(g_object_unref)&> client_;
   std::unique_ptr<GCancellable, decltype(g_object_unref)&> cancellable_;

   static void free_contact_list(GSList *list) { g_slist_free_full(list, g_object_unref); };
   std::unique_ptr<GSList, decltype(free_contact_list)&> contacts_;
};

#endif /* EDSCONTACTBACKEND_H */