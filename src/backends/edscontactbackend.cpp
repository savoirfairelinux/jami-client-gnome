/*
 *  Copyright (C) 2015-2016 Savoir-faire Linux Inc.
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

#include "edscontactbackend.h"

#include <glib/gi18n.h>
#include <person.h>
#include <personmodel.h>
#include <contactmethod.h>
#include <collectioneditor.h>
#include <memory>

static void
client_cb(G_GNUC_UNUSED ESource *source, GAsyncResult *result, G_GNUC_UNUSED gpointer user_data)
{
    GError *error = NULL;
    EClient *client = e_book_client_connect_finish(result, &error);
    if (!client) {
        g_warning("%s", error->message);
        g_clear_error(&error);
    } else {
        /* got a client for this addressbook, add as backend */
        PersonModel::instance().addCollection<EdsContactBackend, EClient *>(
            client, LoadOptions::FORCE_ENABLED);
    }
}

static void
registry_cb(G_GNUC_UNUSED GObject *source, GAsyncResult *result, GCancellable *cancellable)
{
    GError *error = NULL;
    ESourceRegistry *registry = e_source_registry_new_finish(result, &error);
    if(!registry) {
        g_warning("Unable to create EDS registry: %s", error->message);
        g_clear_error(&error);
        return;
    } else {
        GList *list = e_source_registry_list_enabled(registry, E_SOURCE_EXTENSION_ADDRESS_BOOK);

        for (GList *l = list ; l; l = l->next) {
            ESource *source = E_SOURCE(l->data);
            /* try to connect to each source ansynch */
#if EDS_CHECK_VERSION(3,16,0)
            e_book_client_connect(source,
                                  EdsContactBackend::WAIT_FOR_CONNECTED_SECONDS,
                                  cancellable,
                                  (GAsyncReadyCallback)client_cb,
                                  NULL);
#else
            e_book_client_connect(source, cancellable, (GAsyncReadyCallback)client_cb, NULL);
#endif
        }

        g_list_free_full(list, g_object_unref);
    }
}

void load_eds_sources(GCancellable *cancellable)
{
    /* load the registery asynchronously, and then each source asynch as well
     * pass the cancellable as a param so that the loading of each source can
     * also be cancelled
     */
    e_source_registry_new(cancellable, (GAsyncReadyCallback)registry_cb, cancellable);
}

class EdsContactEditor : public CollectionEditor<Person>
{
public:
    EdsContactEditor(CollectionMediator<Person>* m, EdsContactBackend* parent);
    ~EdsContactEditor();
    virtual bool save       ( const Person* item ) override;
    virtual bool remove     ( const Person* item ) override;
    virtual bool edit       ( Person*       item ) override;
    virtual bool addNew     ( Person*       item ) override;
    virtual bool addExisting( const Person* item ) override;

private:
    virtual QVector<Person*> items() const override;

    QVector<Person*> items_;
    EdsContactBackend* collection_;
};

EdsContactEditor::EdsContactEditor(CollectionMediator<Person>* m, EdsContactBackend* parent) :
CollectionEditor<Person>(m),collection_(parent)
{
}

EdsContactEditor::~EdsContactEditor()
{
}

bool EdsContactEditor::save(const Person* item)
{
    return collection_->savePerson(item);
}

bool EdsContactEditor::remove(const Person* item)
{
    bool ret = collection_->removePerson(item);
    if (ret) {
        mediator()->removeItem(item);
    }
    return ret;
}

bool EdsContactEditor::edit( Person* item)
{
    Q_UNUSED(item)
    return false;
}

bool EdsContactEditor::addNew(Person* item)
{
    bool ret = collection_->addNewPerson(item);
    if (ret) {
        items_ << item;
        mediator()->addItem(item);
    }
    return ret;
}

bool EdsContactEditor::addExisting(const Person* item)
{
    items_ << const_cast<Person*>(item);
    mediator()->addItem(item);
    return true;
}

QVector<Person*> EdsContactEditor::items() const
{
    return items_;
}

static void free_client_view(EBookClientView *client_view) {
    g_return_if_fail(client_view);
    GError *error = NULL;
    e_book_client_view_stop(client_view, &error);
    if (error) {
        g_warning("error stopping EBookClientView: %s", error->message);
        g_clear_error(&error);
    }
    g_object_unref(client_view);
}

static void
free_object_list(GSList *list)
{
    g_slist_free_full(list, g_object_unref);
};

static void
free_string_list(GSList *list)
{
    g_slist_free_full(list, g_free);
};

EdsContactBackend::EdsContactBackend(CollectionMediator<Person>* mediator, EClient *client, CollectionInterface* parent)
    : CollectionInterface(new EdsContactEditor(mediator,this), parent)
    , mediator_(mediator)
    , client_(client, g_object_unref)
    , cancellable_(g_cancellable_new(), g_object_unref)
    , client_view_(nullptr, &free_client_view)
{
    // cache the name
    if (client_) {
        auto source = e_client_get_source(client_.get());
        auto extension = (ESourceAddressBook *)e_source_get_extension(source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
        auto backend = e_source_backend_get_backend_name(E_SOURCE_BACKEND(extension));
        auto addressbook = e_source_get_display_name(source);

        gchar *name = g_strdup_printf("%s (%s)", addressbook, backend);
        name_ = name;
        g_free(name);
    } else
        name_ = _("Unknown EDS addressbook");
}

EdsContactBackend::~EdsContactBackend()
{
    /* cancel any cancellable operations */
    g_cancellable_cancel(cancellable_.get());

    /* cancel contact loading timeout source, if its not finished */
    if (add_contacts_source_id != 0)
        g_source_remove(add_contacts_source_id);
}

QString EdsContactBackend::name() const
{
    return name_;
}

QString EdsContactBackend::category() const
{
    return C_("Backend type", "Contacts");
}

bool EdsContactBackend::isEnabled() const
{
    return true;
}

static void
contacts_added(G_GNUC_UNUSED EBookClientView *client_view, const GSList *objects, EdsContactBackend *self)
{
    std::unique_ptr<GSList,void(*)(GSList *)> contacts(
        g_slist_copy_deep((GSList *)objects, (GCopyFunc)g_object_ref, NULL ), &free_object_list);
    self->addContacts(std::move(contacts));
}

static void
contacts_modified(EBookClientView *client_view, const GSList *objects, EdsContactBackend *self)
{
    /* The parseContact function will check if the contacts we're "adding" have
     * the same URI as any existing ones and if so will update those instead */
    contacts_added(client_view, objects, self);
}

static void
contacts_removed(G_GNUC_UNUSED EBookClientView *client_view, const GSList *uids, EdsContactBackend *self)
{
    std::unique_ptr<GSList,void(*)(GSList *)> contact_uids(
        g_slist_copy_deep((GSList *)uids, (GCopyFunc)g_strdup, NULL ), &free_string_list);
    self->removeContacts(std::move(contact_uids));
}

static void
client_view_cb(EBookClient *client, GAsyncResult *result, EdsContactBackend *self)
{
    g_return_if_fail(E_IS_BOOK_CLIENT(client));
    EBookClientView *client_view = NULL;
    GError *error = NULL;
    if(!e_book_client_get_view_finish(client, result, &client_view, &error)) {
        g_warning("Unable to get EDS client view: %s", error->message);
        g_clear_error(&error);
        return;
    } else {
        /* we want the EBookClientView to have the same life cycle as the backend */
        std::unique_ptr<EBookClientView, void(*)(EBookClientView *)> client_view_ptr(
            client_view, &free_client_view);
        self->addClientView(std::move(client_view_ptr));
    }
}

void EdsContactBackend::parseContact(EContact *contact)
{
    /* Check if the photo is in-line or a URI, in the case that it is a URI,
     * try to make it inline so that the lrc vcard parser is able to get the
     * photo. Note that this will only work on local URIs
     */
    EContactPhoto *photo = (EContactPhoto *)e_contact_get(contact, E_CONTACT_PHOTO);
    if (photo) {
        if (photo->type == E_CONTACT_PHOTO_TYPE_URI) {
            GError *error = NULL;
            if (!e_contact_inline_local_photos(contact, &error)) {
                g_warning("could not inline photo from vcard URI: %s", error->message);
                g_clear_error(&error);
            }
        }
    }
    e_contact_photo_free(photo);

    EVCard *vcard = E_VCARD(contact);
    gchar *vcard_str = e_vcard_to_string(vcard, EVC_FORMAT_VCARD_30);

    /* check if this person already exists */
    Person *existing = nullptr;

    gchar *uid = (gchar *)e_contact_get(contact, E_CONTACT_UID);
    if (uid) {
        // g_warning("got uid: %s", uid);
        existing = PersonModel::instance().getPersonByUid(uid);
        g_free(uid);
    }

    if (existing) {
        /* update existing person */
        existing->updateFromVCard(vcard_str);
    } else {
        Person *p = new Person(vcard_str, Person::Encoding::vCard, this);
        editor<Person>()->addExisting(p);
    }

    g_free(vcard_str);
}

typedef struct AddContactsData_
{
    EdsContactBackend* backend;
    GSList *next;
    std::unique_ptr<GSList, void(*)(GSList *)> contacts;
} AddContactsData;

void
free_add_contacts_data(AddContactsData *data)
{
    g_return_if_fail(data && data->contacts);
    data->contacts.reset();
    g_free(data);
}

static gboolean
add_contacts(AddContactsData *data)
{
    for (int i = 0; i < data->backend->CONTACT_ADD_LIMIT && data->next; i++) {
        data->backend->parseContact(E_CONTACT(data->next->data));
        data->next = data->next->next;
    }

    if (!data->next) {
        data->backend->lastContactAdded();
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

void EdsContactBackend::lastContactAdded()
{
    /* Sets the source id to 0 to make sure we don't try to remove this source
     * after it has already finished */
    add_contacts_source_id = 0;
}

void EdsContactBackend::addClientView(std::unique_ptr<EBookClientView, void(*)(EBookClientView *)> client_view)
{
    client_view_ = std::move(client_view);

    /* connect signals for adding, removing, and modifying contacts */
    g_signal_connect(client_view_.get(), "objects-added", G_CALLBACK(contacts_added), this);
    g_signal_connect(client_view_.get(), "objects-modified", G_CALLBACK(contacts_modified), this);
    g_signal_connect(client_view_.get(), "objects-removed", G_CALLBACK(contacts_removed), this);

    /* start processing the signals */
    GError *error = NULL;
    e_book_client_view_start(client_view_.get(), &error);
    if (error) {
        g_warning("Unable to start EDS client view: %s", error->message);
        g_clear_error(&error);
    }
}

void EdsContactBackend::addContacts(std::unique_ptr<GSList, void(*)(GSList *)> contacts)
{
    /* add CONTACT_ADD_LIMIT # of contacts every CONTACT_ADD_INTERVAL miliseconds */
    AddContactsData *data = g_new0(AddContactsData, 1);
    data->backend = this;
    data->contacts = std::move(contacts);
    data->next = data->contacts.get();

    g_timeout_add_full(G_PRIORITY_DEFAULT,
                       CONTACT_ADD_INTERVAL,
                       (GSourceFunc)add_contacts,
                       data,
                       (GDestroyNotify)free_add_contacts_data);
}

void EdsContactBackend::removeContacts(std::unique_ptr<GSList, void(*)(GSList *)> contact_uids)
{
    GSList *next = contact_uids.get();
    while(next) {
        gchar *uid = (gchar *)next->data;
        if (uid) {
            Person *p = PersonModel::instance().getPersonByUid(uid);
            if (p) {
                g_debug("removing: %s", p->formattedName().toUtf8().constData());
                deactivate(p);
                mediator_->removeItem(p);
            } else {
                g_warning("person with given UID doesn't exist: %s", uid);
            }
        } else {
            g_warning("null UID in list");
        }
        next = g_slist_next(next);
    }
}

bool EdsContactBackend::load()
{
    /**
     * load the contacts by querying for them,
     * we want the contact to have some kind of name
     */
    EBookQuery *queries[4];
    int idx = 0;
    queries[idx++] = e_book_query_field_exists(E_CONTACT_NAME_OR_ORG);
    queries[idx++] = e_book_query_field_exists(E_CONTACT_GIVEN_NAME);
    queries[idx++] = e_book_query_field_exists(E_CONTACT_FAMILY_NAME);
    queries[idx++] = e_book_query_field_exists(E_CONTACT_NICKNAME);

    EBookQuery *name_query = e_book_query_or(idx, queries, TRUE);
    gchar *query_str = e_book_query_to_string(name_query);
    e_book_query_unref(name_query);

    /* test */
    e_book_client_get_view(E_BOOK_CLIENT(client_.get()),
                           query_str,
                           cancellable_.get(),
                           (GAsyncReadyCallback)client_view_cb,
                           this);
    g_free(query_str);

    return true;
}

bool EdsContactBackend::reload()
{
    return false;
}

FlagPack<CollectionInterface::SupportedFeatures> EdsContactBackend::supportedFeatures() const
{
    return (CollectionInterface::SupportedFeatures::NONE |
            CollectionInterface::SupportedFeatures::LOAD |
            CollectionInterface::SupportedFeatures::ADD  |
            CollectionInterface::SupportedFeatures::REMOVE  |
            CollectionInterface::SupportedFeatures::SAVE );
}

bool EdsContactBackend::clear()
{
    return false;
}

QByteArray EdsContactBackend::id() const
{
    if (client_)
        return e_source_get_uid(e_client_get_source(client_.get()));
    return "edscb";
}

bool EdsContactBackend::addNewPerson(Person *item)
{
    g_return_val_if_fail(client_.get(), false);

    auto contact = e_contact_new_from_vcard(item->toVCard().constData());
    gchar *uid = NULL;
    GError *error = NULL;

    /* FIXME: this methods returns True for a google addressbook, but it never
     * actually adds the new contact... not clear if this is possible */
    bool ret = e_book_client_add_contact_sync(
        E_BOOK_CLIENT(client_.get()),
        contact,
        &uid,
        cancellable_.get(),
        &error
    );

    if (!ret) {
        if (error) {
            g_warning("could not add contact to collection: %s", error->message);
            g_clear_error(&error);
        } else {
            g_warning("could not add contact to collection");
        }
    } else {
        item->setUid(uid);
    }

    g_free(uid);
    g_object_unref(contact);

    return ret;
}

bool EdsContactBackend::removePerson(const Person *item)
{
    g_return_val_if_fail(client_.get(), false);

    g_debug("removing person");

    GError *error = NULL;

    bool ret = e_book_client_remove_contact_by_uid_sync(
        E_BOOK_CLIENT(client_.get()),
        item->uid(),
        cancellable_.get(),
        &error
    );

    if(!ret) {
        if(error) {
            g_warning("could not delete contact: %s", error->message);
            g_clear_error(&error);
        }
        else {
            g_warning("could not delete contact");
        }
    }

    return ret;
}

bool EdsContactBackend::savePerson(const Person *item)
{
    g_return_val_if_fail(client_.get(), false);

    g_debug("saving person");

    auto contact = e_contact_new_from_vcard(item->toVCard().constData());
    GError *error = NULL;

    /* FIXME: this methods fails for a google addressbook, not clear if it is
     * possible to edit a google address book... gnome contacts simply creates
     * a local contact with the same uid */
    bool ret = e_book_client_modify_contact_sync(
        E_BOOK_CLIENT(client_.get()),
        contact,
        cancellable_.get(),
        &error
    );

    if (!ret) {
        if (error) {
            g_warning("could not modify contact: %s", error->message);
            g_clear_error(&error);
        } else {
            g_warning("could not modify contact");
        }
    }

    g_object_unref(contact);

    return ret;
}
