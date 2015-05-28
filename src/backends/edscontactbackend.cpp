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

#include "edscontactbackend.h"

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
        PersonModel::instance()->addCollection<EdsContactBackend, EClient *>(
            client, LoadOptions::FORCE_ENABLED);
    }
}

static void
registry_cb(G_GNUC_UNUSED GObject *source, GAsyncResult *result, GCancellable *cancellable)
{
    GError *error = NULL;
    ESourceRegistry *registry = e_source_registry_new_finish(result, &error);
    if(!registry) {
        g_critical("Unable to create EDS registry: %s", error->message);
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
    virtual bool addNew     ( const Person* item ) override;
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
    Q_UNUSED(item)
    return false;
}

bool EdsContactEditor::remove(const Person* item)
{
    Q_UNUSED(item)
    return false;
}

bool EdsContactEditor::edit( Person* item)
{
    Q_UNUSED(item)
    return false;
}

bool EdsContactEditor::addNew(const Person* item)
{
    Q_UNUSED(item)
    return false;
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
free_contact_list(GSList *list)
{
    g_slist_free_full(list, g_object_unref);
};

EdsContactBackend::EdsContactBackend(CollectionMediator<Person>* mediator, EClient *client, CollectionInterface* parent)
    : CollectionInterface(new EdsContactEditor(mediator,this), parent)
    , mediator_(mediator)
    , client_(client, g_object_unref)
    , cancellable_(g_cancellable_new(), g_object_unref)
    , client_view_(nullptr, &free_client_view)
{
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
    return QObject::tr("Evolution-data-server backend");
}

QString EdsContactBackend::category() const
{
    return QObject::tr("Contacts");
}

bool EdsContactBackend::isEnabled() const
{
    return true;
}

static void
contacts_added(G_GNUC_UNUSED EBookClientView *client_view, const GSList *objects, EdsContactBackend *self)
{
    std::unique_ptr<GSList,void(*)(GSList *)> contacts(
        g_slist_copy_deep((GSList *)objects, (GCopyFunc)g_object_ref, NULL ), &free_contact_list);
    self->addContacts(std::move(contacts));
}

static void
client_view_cb(EBookClient *client, GAsyncResult *result, EdsContactBackend *self)
{
    g_return_if_fail(E_IS_BOOK_CLIENT(client));
    EBookClientView *client_view = NULL;
    GError *error = NULL;
    if(!e_book_client_get_view_finish(client, result, &client_view, &error)) {
        g_critical("Unable to get client view: %s", error->message);
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
    Person *p = new Person(vcard_str, Person::Encoding::vCard, this);
    g_free(vcard_str);
    editor<Person>()->addExisting(p);
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

    /* start processing the signals */
    GError *error = NULL;
    e_book_client_view_start(client_view_.get(), &error);
    if (error) {
        g_critical("Unable to get start client view: %s", error->message);
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
            CollectionInterface::SupportedFeatures::LOAD);
}

bool EdsContactBackend::clear()
{
    return false;
}

QByteArray EdsContactBackend::id() const
{
   return "edscb";
}
