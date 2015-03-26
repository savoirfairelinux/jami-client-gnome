#include "pixbufdelegate.h"

#include "../utils/drawing.h"
#include <QtCore/QSize>
#include <QtCore/QMetaType>
#include <person.h>
#include <memory>

void deletePixbuf(GdkPixbuf* pixbuf)
{
//     g_debug("unref pixbuf, count: %d", G_OBJECT(pixbuf)->ref_count);
    g_object_unref(pixbuf);
//     g_debug("\tpointer: %p", pixbuf);
}

PixbufDelegate::PixbufDelegate()
    : PixmapManipulationDelegate()
    , fallback_avatar{ring_draw_fallback_avatar(FALLBACK_AVATAR_SIZE), g_object_unref}
{
}

QVariant PixbufDelegate::contactPhoto(Person* c, const QSize& size, bool displayPresence) {
    Q_UNUSED(displayPresence);
    /**
     * if a photo exists, scale it and frame it,
     * otherwise do the same to a fallback avatar
     * 
     * for now, respect the height requested
     * the framing process will add another 10px, so account for that
     * when scaling the photos
     */
    
    int height = size.height();
    if (size.height() != size.width())
        g_warning("requested contact photo width != height; only respecting the height as the largest dimension");
    int photo_h = height - 10;
    int photo_w = photo_h;
    
    std::shared_ptr<GdkPixbuf> photo;
    
    if (c->photo().isValid())
        photo = c->photo().value<std::shared_ptr<GdkPixbuf>>();
    else
        photo = fallback_avatar;
    
    /* scale photo, make sure to respect the request height as the largest dimension*/
    int w = gdk_pixbuf_get_width(photo.get());
    int h = gdk_pixbuf_get_height(photo.get());
    if (h > w)
        photo_w = w * ((double)photo_h / h);
    if (w > h)
        photo_h = h * ((double)photo_w / w);
    
    std::unique_ptr<GdkPixbuf, decltype(g_object_unref)&> scaled_photo{
        gdk_pixbuf_scale_simple(photo.get(), photo_w, photo_h, GDK_INTERP_BILINEAR),
        g_object_unref};
    
    /* frame photo */
    std::shared_ptr<GdkPixbuf> framed_photo{ring_frame_avatar(scaled_photo.get()), g_object_unref};
    
    return QVariant::fromValue(framed_photo);
}

QVariant PixbufDelegate::personPhoto(const QByteArray& data, const QString& type) {
//     g_debug("iamge type: %s", type.toUtf8().constData());

    /* get image and frame it; assume its square for now */
//     std::shared_ptr<GdkPixbuf> avatar(ring_draw_fallback_avatar(100), deletePixbuf);

//     g_debug("iamge size: %d", data.size());

    GError *error = NULL;
    GdkPixbuf *pixbuf = NULL;
    GInputStream *stream = NULL;
    
    /* first try using base64 */
    QByteArray ba64 = QByteArray::fromBase64(data);
    stream = g_memory_input_stream_new_from_data(ba64.constData(),
                                                 ba64.size(),
                                                 NULL);
    
    pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
    g_input_stream_close(stream, NULL, NULL);
    g_object_unref(stream);

    if (!pixbuf) {
//         g_debug("failed decoding person photo using base64: %s", error->message);
        g_error_free(error);
        error = NULL;

        /* failed with base64, try hex */
        QByteArray baHex = QByteArray::fromHex(data);
        stream = g_memory_input_stream_new_from_data(baHex.constData(),
                                                     baHex.size(),
                                                     NULL);
        
        pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);

        g_input_stream_close(stream, NULL, NULL);
        g_object_unref(stream);
    
        if (!pixbuf) {
//             g_debug("failed decoding person photo using hex (ASCII): %s", error->message);
            g_error_free(error);
            error = NULL;
        }
    }
    
    if (pixbuf) {
        std::shared_ptr<GdkPixbuf> avatar(pixbuf, deletePixbuf);
        return QVariant::fromValue(avatar);
    }
    
//     g_debug("data: %s", data.constData());
//     g_debug("data64: %s", ba64.constData());
//     g_warning("could not load person photo");
    return QVariant();
}
