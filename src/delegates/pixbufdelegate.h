#ifndef PIXBUFDELEGATE_H
#define PIXBUFDELEGATE_H

#include <gtk/gtk.h>
#include <memory>
#include <delegates/pixmapmanipulationdelegate.h>

Q_DECLARE_METATYPE(std::shared_ptr<GdkPixbuf>);

class Person;

void deletePixbuf(GdkPixbuf* pixbuf);

class PixbufDelegate : public PixmapManipulationDelegate {
public:
    PixbufDelegate();
   
    QVariant personPhoto(const QByteArray& data, const QString& type = "PNG") override;
};

#endif /* PIXBUFDELEGATE_H */