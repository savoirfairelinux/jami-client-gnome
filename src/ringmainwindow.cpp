
#include "ringmainwindow.h"

#include <gtk/gtk.h>
#include "models/gtkqtreemodel.h"
#include <callmodel.h>

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

    /* models */
    GtkWidget *treeview_call;
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

    // gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (win), TRUE);

    /* gears menu */
    GtkBuilder *builder = gtk_builder_new_from_resource("/cx/ring/RingGnome/ringgearsmenu.ui");
    GMenuModel *menu = G_MENU_MODEL(gtk_builder_get_object(builder, "menu"));
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(priv->gears), menu);
    g_object_unref(builder);

    /* call model */
    GtkQTreeModel *model;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    model = gtk_q_tree_model_new(CallModel::instance(), 4,
        Call::Role::Name, G_TYPE_STRING,
        Call::Role::Number, G_TYPE_STRING,
        Call::Role::Length, G_TYPE_STRING,
        Call::Role::CallState, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->treeview_call), GTK_TREE_MODEL(model) );

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Name", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW (priv->treeview_call), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Number", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Duration", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("State", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->treeview_call), column);
}

static void
ring_main_window_dispose(GObject *object)
{
    RingMainWindow *win;
    RingMainWindowPrivate *priv;

    win = RING_MAIN_WINDOW(object);
    priv = RING_MAIN_WINDOW_GET_PRIVATE(win);

    // g_clear_object (&priv->settings);

    G_OBJECT_CLASS(ring_main_window_parent_class)->dispose(object);
}

static void
ring_main_window_class_init(RingMainWindowClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = ring_main_window_dispose;

    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS (klass),
                                                "/cx/ring/RingGnome/ringmainwindow.ui");

    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, treeview_history);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, treeview_call);
    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, treeview_accounts);
    // gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, entry_call);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, gears);
    gtk_widget_class_bind_template_child_private(GTK_WIDGET_CLASS (klass), RingMainWindow, gears_image);

    /* TODO: remove after doing this with GAction */
    // gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingMainWindow, toolbutton_pickup);
    // gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), RingMainWindow, toolbutton_hangup);

    /* bind handlers from template */
    // gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), search_text_changed);
    // gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (klass), visible_child_changed);
}

GtkWidget *
ring_main_window_new (GtkApplication *app)
{
    return (GtkWidget *)g_object_new(RING_MAIN_WINDOW_TYPE, "application", app, NULL);
}
