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

void load_eds_sources(GCancellable *cancellable);

class EdsContactBackend : public CollectionInterface
{
public:
    /* load 300 contacts every 10ms; this is fast enough to load
     * 1M contacts in about 30s, but can be increased if need be
     */
    /* this sets how many contacts will be added at a time */
    constexpr static int CONTACT_ADD_LIMIT {300};
    /* this sets the interval in miliseconds to wait before adding
     * more contacts */
    constexpr static int CONTACT_ADD_INTERVAL {10};

#if EDS_CHECK_VERSION(3,16,0)
    /* The wait_for_connected_seconds argument had been added since 3.16, to let
     * the caller decide how long to wait for the backend to fully connect to
     * its (possibly remote) data store. This is required due to a change in the
     * authentication process, which is fully asynchronous and done on the
     * client side, while not every client is supposed to response to
     * authentication requests. In case the backend will not connect within the
     * set interval, then it is opened in an offline mode. A special value -1
     * can be used to not wait for the connected state at all.
     */
    constexpr static guint32 WAIT_FOR_CONNECTED_SECONDS {5};
#endif

    explicit EdsContactBackend(CollectionMediator<Person>* mediator, EClient *client, CollectionInterface* parent = nullptr);
    virtual ~EdsContactBackend();

    virtual bool load() override;
    virtual bool reload() override;
    virtual bool clear() override;
    virtual QString    name     () const override;
    virtual QString    category () const override;
    virtual bool       isEnabled() const override;
    virtual QByteArray id       () const override;
    virtual FlagPack<SupportedFeatures>  supportedFeatures() const override;

    void addClientView(std::unique_ptr<EBookClientView, void(*)(EBookClientView *)> client_view);
    void addContacts(std::unique_ptr<GSList, void(*)(GSList *)> contacts);
    void removeContacts(std::unique_ptr<GSList, void(*)(GSList *)> contacts);
    void parseContact(EContact *contact);
    void lastContactAdded();
    bool addNewPerson(Person *item);
    bool removePerson(const Person *item);
    bool savePerson(const Person *item);

private:
   CollectionMediator<Person>*  mediator_;
   std::unique_ptr<EClient, decltype(g_object_unref)&> client_;
   std::unique_ptr<GCancellable, decltype(g_object_unref)&> cancellable_;
   std::unique_ptr<EBookClientView, void(*)(EBookClientView *)> client_view_;

   guint add_contacts_source_id {0};

   QString name_;
};

#endif /* EDSCONTACTBACKEND_H */
