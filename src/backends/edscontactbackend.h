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