
#include "ringmainwindow.h"

#include <gtk/gtk.h>

struct _RingMainWindow
{
    GtkApplicationWindow parent;
};

struct _RingMainWindowClass
{
    GtkApplicationWindowClass parent_class;
};

typedef struct _RingMainWindowPrivate RingMainWindowPrivate;

struct _RingMainWindowPrivate
{
    GtkWidget *gears;
    GtkWidget *gears_image;
};

G_DEFINE_TYPE_WITH_PRIVATE(RingMainWindow, ring_main_window, GTK_TYPE_APPLICATION_WINDOW);

#define RING_MAIN_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindowPrivate))

static void
ring_main_window_init(RingMainWindow *win)
{
    RingMainWindowPrivate *priv = RING_MAIN_WINDOW_GET_PRIVATE(win);
    gtk_widget_init_template(GTK_WIDGET(win));

     /* set window icon */
    GError *error = NULL;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/cx/ring/RingGnome/ring-symbol-blue", &error);
    if (icon == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_error_free(error);
    } else
        gtk_window_set_icon(GTK_WINDOW(win), icon);

    /* set menu icon */
    GdkPixbuf* ring_gears = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                                  -1, 22, TRUE, &error);
    if (ring_gears == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_error_free(error);
    } else
        gtk_image_set_from_pixbuf(GTK_IMAGE(priv->gears_image), ring_gears);

    /* gears menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/cx/ring/RingGnome/ringgearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(priv->gears), menu);
    g_object_unref(builder);
}

static void
ring_main_window_class_init(RingMainWindowClass *klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/ringmainwindow.ui");

    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, gears);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, gears_image);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    return (GtkWidget *)g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);
}
