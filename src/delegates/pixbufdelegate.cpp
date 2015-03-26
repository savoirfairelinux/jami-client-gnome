#include "pixbufdelegate.h"

#include "../utils/drawing.h"
#include <QtCore/QSize>
#include <QtCore/QMetaType>

void deletePixbuf(GdkPixbuf* pixbuf)
{
//     g_debug("unref pixbuf, count: %d", G_OBJECT(pixbuf)->ref_count);
    g_object_unref(pixbuf);
//     g_debug("\tpointer: %p", pixbuf);
}

PixbufDelegate::PixbufDelegate() : PixmapManipulationDelegate()
{

}

QVariant PixbufDelegate::personPhoto(const QByteArray& data, const QString& type) {
    g_debug("iamge type: %s", type.toUtf8().constData());
    /* FIXME: no need to generate the default avatar each time */

    /* get image and frame it; assume its square for now */
//     std::shared_ptr<GdkPixbuf> avatar(ring_draw_fallback_avatar(100), deletePixbuf);

    g_debug("iamge size: %d", data.size());

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
        g_debug("failed decoding person photo using base64: %s", error->message);
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
            g_debug("failed decoding person photo using hex (ASCII): %s", error->message);
            g_error_free(error);
            error = NULL;
        }
    }
    
    if (pixbuf) {
        std::shared_ptr<GdkPixbuf> avatar(pixbuf, deletePixbuf);
        return QVariant::fromValue(avatar);
    }
    
    g_debug("data: %s", data.constData());
    g_debug("data64: %s", ba64.constData());
    g_warning("could not load person photo");
    return QVariant();
}
