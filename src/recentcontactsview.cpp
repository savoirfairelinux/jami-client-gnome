/*
 *  Copyright (C) 2015 Savoir-faire Linux Inc.
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

#include "recentcontactsview.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "models/gtkqtreemodel.h"
#include "utils/calling.h"
#include <memory>
#include <globalinstances.h>
#include "native/pixbufmanipulator.h"
#include <contactmethod.h>
#include "defines.h"
#include "utils/models.h"
#include <recentmodel.h>
#include <call.h>
#include "utils/menus.h"

#define COPY_DATA_KEY "copy_data"

struct _RecentContactsView
{
    GtkBox parent;
};

struct _RecentContactsViewClass
{
    GtkBoxClass parent_class;
};

typedef struct _RecentContactsViewPrivate RecentContactsViewPrivate;

struct _RecentContactsViewPrivate
{
    GtkWidget *treeview_recent;
    GtkWidget *overlay_button;
};

G_DEFINE_TYPE_WITH_PRIVATE(RecentContactsView, recent_contacts_view, GTK_TYPE_BOX);

#define RECENT_CONTACTS_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RECENT_CONTACTS_VIEW_TYPE, RecentContactsViewPrivate))


static void
copy_contact_info(GtkWidget *item, G_GNUC_UNUSED gpointer user_data)
{
    gpointer data = g_object_get_data(G_OBJECT(item), COPY_DATA_KEY);
    g_return_if_fail(data);
    gchar* text = (gchar *)data;
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clip, text, -1);
}

static void
render_contact_photo(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    /* we only want to render a photo for the top nodes: Person, ContactMethod (, later Conference) */
    QVariant var_p = idx.data(static_cast<int>(Person::Role::Object));
    QVariant var_cm = idx.data(static_cast<int>(Call::Role::ContactMethod));
    if (idx.isValid() && var_p.isValid()) {
        if (auto person = var_p.value<Person *>()) {
            /* get photo */
            QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(person, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    } else if (idx.isValid() && var_cm.isValid()) {
        if (auto cm = var_cm.value<ContactMethod *>()) {
            /* get photo, note that this should in all cases be the fallback avatar, since there
             * shouldn't be a person associated with this contact method */
            QVariant var_p = GlobalInstances::pixmapManipulator().callPhoto(cm, QSize(50, 50), false);
            std::shared_ptr<GdkPixbuf> photo = var_p.value<std::shared_ptr<GdkPixbuf>>();
            g_object_set(G_OBJECT(cell), "pixbuf", photo.get(), NULL);
            return;
        }
    }

    /* otherwise, make sure its an empty pixbuf */
    g_object_set(G_OBJECT(cell), "pixbuf", NULL, NULL);
}

static void
render_name_and_info(G_GNUC_UNUSED GtkTreeViewColumn *tree_column,
                     GtkCellRenderer *cell,
                     GtkTreeModel *model,
                     GtkTreeIter *iter,
                     G_GNUC_UNUSED gpointer data)
{
    gchar *text = NULL;

    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), iter);

    QVariant name = idx.data(Qt::DisplayRole);
    if (idx.isValid() && name.isValid()) {
        text = g_strdup_printf("%s\n <span fgcolor=\"gray\">%s</span>",
                                name.value<QString>().toUtf8().constData(),
                                "blah");
    }

    g_object_set(G_OBJECT(cell), "markup", text, NULL);
    g_free(text);
}

static void
activate_item(GtkTreeView *tree_view,
              GtkTreePath *path,
              G_GNUC_UNUSED GtkTreeViewColumn *column,
              G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);
        if (idx.isValid()) {
            QVariant var_n = idx.data(static_cast<int>(Call::Role::ContactMethod));
            if (var_n.isValid())
                place_new_call(var_n.value<ContactMethod *>());
        }
    }
}

static gboolean
create_popup_menu(GtkTreeView *treeview, GdkEventButton *event, G_GNUC_UNUSED gpointer user_data)
{
    /* build popup menu when right clicking on contact item
     * user should be able to copy the contact's name or "number".
     * or add the "number" to his contact list, if not already so
     */

    /* check for right click */
    if (event->button != BUTTON_RIGHT_CLICK || event->type != GDK_BUTTON_PRESS)
        return FALSE;

    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
    if (!gtk_tree_selection_get_selected(selection, &model, &iter))
        return FALSE;

    GtkWidget *menu = gtk_menu_new();
    QModelIndex idx = gtk_q_tree_model_get_source_idx(GTK_Q_TREE_MODEL(model), &iter);

    /* get name and number */
    QVariant c = idx.data(static_cast<int>(Call::Role::Name));
    QVariant n = idx.data(static_cast<int>(Call::Role::Number));

    /* copy name */
    gchar *name = g_strdup_printf("%s", c.value<QString>().toUtf8().constData());
    GtkWidget *item = gtk_menu_item_new_with_mnemonic(_("_Copy name"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, name, (GDestroyNotify)g_free);
    g_signal_connect(item,
                     "activate",
                     G_CALLBACK(copy_contact_info),
                     NULL);

    gchar *number = g_strdup_printf("%s", n.value<QString>().toUtf8().constData());
    item = gtk_menu_item_new_with_mnemonic(_("_Copy number"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_object_set_data_full(G_OBJECT(item), COPY_DATA_KEY, number, (GDestroyNotify)g_free);
    g_signal_connect(item,
                     "activate",
                     G_CALLBACK(copy_contact_info),
                     NULL);

     /* get the call object from the selected item
      * check if it is already linked to a person, if not, then offer to either
      * add to a new or existing contact */
     const auto& var_cm = idx.data(static_cast<int>(Call::Role::ContactMethod));
     if (idx.isValid() && var_cm.isValid()) {
         if (auto contactmethod = var_cm.value<ContactMethod *>()) {
             if (!contact_method_has_contact(contactmethod)) {
                 /* get rectangle */
                 auto path = gtk_tree_model_get_path(model, &iter);
                 auto column = gtk_tree_view_get_column(treeview, 0);
                 GdkRectangle rect;
                 gtk_tree_view_get_cell_area(treeview, path, column, &rect);
                 gtk_tree_view_convert_bin_window_to_widget_coords(treeview, rect.x, rect.y, &rect.x, &rect.y);
                 gtk_tree_path_free(path);
                 auto add_to = menu_item_add_to_contact(contactmethod, GTK_WIDGET(treeview), &rect);
                 gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_to);
             }
         }
     }

    /* show menu */
    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);

    return TRUE; /* we handled the event */
}

static gboolean
motion_event_cb(GtkWidget *treeview,
                GdkEvent  *event,
                RecentContactsView *self)
{
    g_return_val_if_fail(IS_RECENT_CONTACTS_VIEW(self), FALSE);
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    g_return_val_if_fail(event->type == GDK_MOTION_NOTIFY, FALSE);

    GdkEventMotion *motion = (GdkEventMotion*)event;

    g_return_val_if_fail(motion->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(treeview)), FALSE);

    /* convert, in case headers are show */
    gint bx, by;
    gtk_tree_view_convert_widget_to_bin_window_coords(
        GTK_TREE_VIEW(treeview),
        (gint)motion->x, (gint)motion->y,
        &bx, &by
    );

    // g_debug("got pointer position: %d, %d", bx, by);

    GtkTreePath *path = NULL;
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview), bx, by, &path, NULL, NULL, NULL)) {

        GdkRectangle rect;
        gtk_tree_view_get_cell_area(GTK_TREE_VIEW(treeview), path, NULL, &rect);
        gtk_tree_view_convert_bin_window_to_widget_coords(GTK_TREE_VIEW(treeview), rect.x, rect.y, &rect.x, &rect.y);

        gtk_widget_set_margin_top(priv->overlay_button, rect.y);

        gtk_tree_path_free(path);
    }


    return FALSE;
}

static gboolean
crossing_event_cb(GtkWidget *treeview,
                GdkEvent  *event,
                RecentContactsView *self)
{
    g_return_val_if_fail(IS_RECENT_CONTACTS_VIEW(self), FALSE);
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    g_return_val_if_fail(event->type == GDK_ENTER_NOTIFY || event->type == GDK_LEAVE_NOTIFY, FALSE);

    GdkEventCrossing *crossing = (GdkEventCrossing*)event;

    g_return_val_if_fail(crossing->window == gtk_tree_view_get_bin_window(GTK_TREE_VIEW(treeview)), FALSE);

    if (crossing->type == GDK_ENTER_NOTIFY)
        gtk_widget_show(priv->overlay_button);
    else {
        gboolean inside_button = FALSE;
        gint wx, wy;
        if (gtk_widget_translate_coordinates(priv->overlay_button, priv->treeview_recent, 0, 0, &wx, &wy)) {
            GtkAllocation allocation;
            gtk_widget_get_allocation(priv->overlay_button, &allocation);

            gdouble p_x = crossing->x;
            gdouble p_y = crossing->y;

            gdouble b_x = (gdouble)wx;
            gdouble b_y = (gdouble)wy;
            gdouble b_xw = b_x + allocation.width;
            gdouble b_yh = b_y + allocation.height;

            if (p_x >= b_x && p_x <= b_xw && p_y >= b_y && p_y <= b_yh )
                inside_button = TRUE;
        }

        if (!inside_button)
            gtk_widget_hide(priv->overlay_button);
    }

    return FALSE;
}

static gboolean
crossing_event_button_cb(GtkWidget *button,
                         GdkEvent  *event,
                         RecentContactsView *self)
{
    g_return_val_if_fail(IS_RECENT_CONTACTS_VIEW(self), FALSE);
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    g_return_val_if_fail(event->type == GDK_ENTER_NOTIFY || event->type == GDK_LEAVE_NOTIFY, FALSE);

    GdkEventCrossing *crossing = (GdkEventCrossing*)event;

    if (crossing->type == GDK_LEAVE_NOTIFY)
        gtk_widget_hide(priv->overlay_button);

    return FALSE;
}

static void
recent_contacts_view_init(RecentContactsView *self)
{
    RecentContactsViewPrivate *priv = RECENT_CONTACTS_VIEW_GET_PRIVATE(self);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);

    /* test overlay */
    GtkWidget *overlay = gtk_overlay_new();
    gtk_box_pack_start(GTK_BOX(self), overlay, TRUE, TRUE, 0);

    GtkWidget *treeview_recent = gtk_tree_view_new();
    priv->treeview_recent = treeview_recent;
    gtk_container_add(GTK_CONTAINER(overlay), treeview_recent);

    /* overlay button test */
    priv->overlay_button = gtk_button_new();
    gtk_widget_set_no_show_all(priv->overlay_button, TRUE);
    GError *error = NULL;
    GdkPixbuf* icon = gdk_pixbuf_new_from_resource("/cx/ring/RingGnome/video_call", &error);
    if (icon == NULL) {
        g_debug("Could not load icon: %s", error->message);
        g_clear_error(&error);
    } else {
        auto image = gtk_image_new_from_pixbuf(icon);
        gtk_button_set_image(GTK_BUTTON(priv->overlay_button), image);
    }
    gtk_widget_set_halign(priv->overlay_button, GTK_ALIGN_END);
    gtk_widget_set_valign(priv->overlay_button, GTK_ALIGN_START);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), priv->overlay_button);
    g_signal_connect(treeview_recent, "motion-notify-event", G_CALLBACK(motion_event_cb), self);
    g_signal_connect(treeview_recent, "enter-notify-event", G_CALLBACK(crossing_event_cb), self);
    g_signal_connect(treeview_recent, "leave-notify-event", G_CALLBACK(crossing_event_cb), self);
    g_signal_connect(priv->overlay_button, "leave-notify-event", G_CALLBACK(crossing_event_button_cb), self);

    /* set can-focus to false so that the scrollwindow doesn't jump to try to
     * contain the top of the treeview */
    gtk_widget_set_can_focus(treeview_recent, FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_recent), FALSE);
    /* no need to show the expander since it will always be expanded */
    gtk_tree_view_set_show_expanders(GTK_TREE_VIEW(treeview_recent), FALSE);
    /* disable default search, we will handle it ourselves via LRC;
     * otherwise the search steals input focus on key presses */
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(treeview_recent), FALSE);

    GtkQTreeModel *recent_model = gtk_q_tree_model_new(
        RecentModel::instance(),
        1,
        Qt::DisplayRole, G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview_recent),
                            GTK_TREE_MODEL(recent_model));

    /* photo and name/contact method column */
    GtkCellArea *area = gtk_cell_area_box_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_area(area);

    /* photo renderer */
    GtkCellRenderer *renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    /* get the photo */
    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_contact_photo,
        NULL,
        NULL);

    /* name and contact method renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(G_OBJECT(renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
    gtk_cell_area_box_pack_start(GTK_CELL_AREA_BOX(area), renderer, FALSE, FALSE, FALSE);

    gtk_tree_view_column_set_cell_data_func(
        column,
        renderer,
        (GtkTreeCellDataFunc)render_name_and_info,
        NULL,
        NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_recent), column);
    gtk_tree_view_column_set_resizable(column, TRUE);

    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview_recent));

    g_signal_connect(treeview_recent, "button-press-event", G_CALLBACK(create_popup_menu), NULL);
    g_signal_connect(treeview_recent, "row-activated", G_CALLBACK(activate_item), NULL);

    gtk_widget_show_all(GTK_WIDGET(self));
}

static void
recent_contacts_view_dispose(GObject *object)
{
    G_OBJECT_CLASS(recent_contacts_view_parent_class)->dispose(object);
}

static void
recent_contacts_view_finalize(GObject *object)
{
    G_OBJECT_CLASS(recent_contacts_view_parent_class)->finalize(object);
}

static void
recent_contacts_view_class_init(RecentContactsViewClass *klass)
{
    G_OBJECT_CLASS(klass)->finalize = recent_contacts_view_finalize;
    G_OBJECT_CLASS(klass)->dispose = recent_contacts_view_dispose;
}

GtkWidget *
recent_contacts_view_new()
{
    gpointer self = g_object_new(RECENT_CONTACTS_VIEW_TYPE, NULL);

    return (GtkWidget *)self;
}
