#ifndef PIXBUFDELEGATE_H
#define PIXBUFDELEGATE_H

#include <gtk/gtk.h>
#include <memory>
#include <delegates/pixmapmanipulationdelegate.h>

Q_DECLARE_METATYPE(std::shared_ptr<GdkPixbuf>);

class Person;

void deletePixbuf(GdkPixbuf* pixbuf);

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
    std::shared_ptr<GdkPixbuf> fallback_avatar;
};

#endif /* PIXBUFDELEGATE_H */