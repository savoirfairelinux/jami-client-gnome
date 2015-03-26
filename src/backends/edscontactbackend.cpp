#include "edscontactbackend.h"

#include <person.h>
#include <contactmethod.h>
#include <collectioneditor.h>
#include <memory>

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

    //Attributes
    QVector<Person*> m_lItems;
    EdsContactBackend* m_pCollection;
};

EdsContactEditor::EdsContactEditor(CollectionMediator<Person>* m, EdsContactBackend* parent) :
CollectionEditor<Person>(m),m_pCollection(parent)
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
    m_lItems << const_cast<Person*>(item);
    mediator()->addItem(item);
    return true;
}

QVector<Person*> EdsContactEditor::items() const
{
    return m_lItems;
}

EdsContactBackend::EdsContactBackend(CollectionMediator<Person>* mediator, EClient *client, CollectionInterface* parent)
    : CollectionInterface(new EdsContactEditor(mediator,this), parent)
    , mediator_(mediator)
    , client_(client, g_object_unref)
    , cancellable_(g_cancellable_new(), g_object_unref)
    , contacts_(nullptr, free_contact_list)
{
}

EdsContactBackend::~EdsContactBackend()
{
    /* cancel any cancellable operations */
    g_cancellable_cancel(cancellable_.get());
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
contacts_cb(EBookClient *client, GAsyncResult *result, EdsContactBackend *self)
{
    g_return_if_fail(E_IS_BOOK_CLIENT(client));
    GSList *contacts = NULL;
    GError *error = NULL;
    if(!e_book_client_get_contacts_finish(client, result, &contacts, &error)) {
        g_critical("Unable to get contacts: %s", error->message);
        g_error_free(error);
        return;
    } else {
        self->parseContacts(contacts);
    }
    
//     g_debug("-------------------------");
//     
//     for (GSList *l = contacts; l; l = l->next) {
//         EContact *contact = E_CONTACT(l->data);
//         if (e_contact_get_const(contact, E_CONTACT_CATEGORIES))
//             g_debug("group: %s", (char*)e_contact_get_const(contact, E_CONTACT_CATEGORIES));
//         g_debug("contact: %s", (char*)e_contact_get_const(contact, E_CONTACT_NAME_OR_ORG));
//     }
//     g_slist_foreach(contacts, (GFunc) g_object_unref, NULL);
//     g_slist_free(contacts);
}

void EdsContactBackend::parseContacts(GSList *contacts)
{
    contacts_.reset(contacts);
    
    /**
     * parse each contact and create a person
     * 
     * TODO: parse only X at a time via a timeout function to reduce the load
     */
    
    for (GSList *l = contacts_.get(); l; l = l->next) {
        EContact *contact = E_CONTACT(l->data);
//         if (e_contact_get_const(contact, E_CONTACT_CATEGORIES))
//             g_debug("group: %s", (char*)e_contact_get_const(contact, E_CONTACT_CATEGORIES));
//         g_debug("contact: %s", (char*)e_contact_get_const(contact, E_CONTACT_NAME_OR_ORG));
//         
//         Person *p = new Person();
// 
//         if (e_contact_get_const(contact, E_CONTACT_FULL_NAME)) {
//             g_debug("fullname");
//             p->setFormattedName((const char*)e_contact_get_const(contact, E_CONTACT_FULL_NAME));
//         } else {
//             g_debug("first name");
//             p->setFirstName((const char*)e_contact_get_const(contact, E_CONTACT_GIVEN_NAME));
//             p->setFamilyName((const char*)e_contact_get_const(contact, E_CONTACT_FAMILY_NAME));
//         }
//         g_debug("nickname");
//         p->setNickName((const char*)e_contact_get_const(contact, E_CONTACT_NICKNAME));
//         g_debug("org");
//         p->setOrganization((const char*)e_contact_get_const(contact, E_CONTACT_ORG));
//         g_debug("email");
//         p->setPreferredEmail((const char*)e_contact_get_const(contact, E_CONTACT_EMAIL_1));
//         g_debug("dep");
//         p->setDepartment((const char*)e_contact_get_const(contact, E_CONTACT_ORG_UNIT));
//         g_debug("uid");
//         p->setUid((const char*)e_contact_get_const(contact, E_CONTACT_UID));
        g_debug("photo");
        EContactPhoto *photo = (EContactPhoto *)e_contact_get(contact, E_CONTACT_PHOTO);
        
        if (photo) {
            if (photo->type == E_CONTACT_PHOTO_TYPE_URI) {
                g_debug("oh no, photo is URI");
                g_debug("uri: %s", photo->data.uri);
                
                GError *error = NULL;
                if (!e_contact_inline_local_photos(contact, &error)) {
                    g_warning("could not inline photo: %s", error->message);
                    g_error_free(error);
                } else {
                    /* check if it was inlined */
                    photo = (EContactPhoto *)e_contact_get(contact, E_CONTACT_PHOTO);
                    if (photo->type == E_CONTACT_PHOTO_TYPE_URI) {
                        g_debug("shit, its still a uri");
                    }
                }
            } else {
                g_debug("photo is inline");
                g_debug("type: %s, length: %lu", photo->data.inlined.mime_type, photo->data.inlined.length);
            }
        }
        
//         p->setPhoto((EContactPhoto*)e_contact_get(contact, E_CONTACT_PHOTO));
        
//         p->setCollection(this);
//         editor<Person>()->addExisting(p);
        
        EVCard *vcard = E_VCARD(l->data);
        Person *p = new Person(e_vcard_to_string(vcard, EVC_FORMAT_VCARD_30), Person::Encoding::vCard, this);
//         
//         /* check if the photo was loaded */
//         if (!p->photo().isValid())
//             g_debug("photo not loaded");
        
        editor<Person>()->addExisting(p);
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
    e_book_client_get_contacts(E_BOOK_CLIENT(client_.get()),
                               query_str,
                               cancellable_.get(),
                               (GAsyncReadyCallback)contacts_cb,
                               this);
    g_free(query_str);

    return true;
}

bool EdsContactBackend::reload()
{
    return false;
}

CollectionInterface::SupportedFeatures EdsContactBackend::supportedFeatures() const
{
    return (CollectionInterface::SupportedFeatures) (
        CollectionInterface::SupportedFeatures::NONE  |
        CollectionInterface::SupportedFeatures::LOAD   );
}

bool EdsContactBackend::clear()
{
    return false;
}

QByteArray EdsContactBackend::id() const
{
   return "edscb";
}
