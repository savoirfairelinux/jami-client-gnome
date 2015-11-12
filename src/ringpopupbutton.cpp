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

#include "ringpopupbutton.h"

enum
{
    POPUP,
    POPDOWN,

    LAST_SIGNAL
};

struct _RingPopupButton
{
    GtkButton parent;
};

struct _RingPopupButtonClass
{
    GtkButtonClass parent_class;
};

typedef struct _RingPopupButtonPrivate RingPopupButtonPrivate;

struct _RingPopupButtonPrivate
{
    GtkWidget *dock;
    GtkWidget *frame;
    GtkWidget *content;

    GdkDevice *grab_pointer;
    GdkDevice *grab_keyboard;
};

static void     ring_popup_button_dispose    (GObject             *object);
static void     ring_popup_button_finalize   (GObject             *object);
static gboolean ring_popup_button_press      (GtkWidget           *widget,
                                             GdkEventButton      *event);
static gboolean ring_popup_button_key_release(GtkWidget           *widget,
                                             GdkEventKey         *event);
static void     ring_popup_button_popup      (GtkWidget           *widget);
static void     ring_popup_button_popdown    (GtkWidget           *widget);
static gboolean cb_dock_button_press        (GtkWidget           *widget,
                                             GdkEventButton      *event,
                                             gpointer             user_data);
static gboolean cb_dock_key_release         (GtkWidget           *widget,
                                             GdkEventKey         *event,
                                             gpointer             user_data);
static void     cb_dock_grab_notify         (GtkWidget           *widget,
                                             gboolean             was_grabbed,
                                             gpointer             user_data);
static gboolean cb_dock_grab_broken_event   (GtkWidget           *widget,
                                             gboolean             was_grabbed,
                                             gpointer             user_data);


G_DEFINE_TYPE_WITH_PRIVATE (RingPopupButton, ring_popup_button, GTK_TYPE_BUTTON)

#define RING_POPUP_BUTTON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), RING_TYPE_POPUP_BUTTON, RingPopupButtonPrivate))

static guint signals[LAST_SIGNAL] = { 0, };

static void
ring_popup_button_class_init (RingPopupButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet *binding_set;

  gobject_class->finalize = ring_popup_button_finalize;
  gobject_class->dispose = ring_popup_button_dispose;

  widget_class->button_press_event = ring_popup_button_press;
  widget_class->key_release_event = ring_popup_button_key_release;

  /**
   * RingPopupButton::popup:
   * @button: the object which received the signal
   *
   * The ::popup signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to popup the scale widget.
   *
   * The default bindings for this signal are Space, Enter and Return.
   *
   * Since: 2.12
   */
  signals[POPUP] =
    g_signal_new_class_handler ("popup",
                                G_OBJECT_CLASS_TYPE (klass),
                                (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                                G_CALLBACK (ring_popup_button_popup),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /**
   * RingPopupButton::popdown:
   * @button: the object which received the signal
   *
   * The ::popdown signal is a
   * <link linkend="keybinding-signals">keybinding signal</link>
   * which gets emitted to popdown the scale widget.
   *
   * The default binding for this signal is Escape.
   *
   * Since: 2.12
   */
  signals[POPDOWN] =
    g_signal_new_class_handler ("popdown",
                                G_OBJECT_CLASS_TYPE (klass),
                                (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                                G_CALLBACK (ring_popup_button_popdown),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID,
                                G_TYPE_NONE, 0);

  /* Key bindings */
  binding_set = gtk_binding_set_by_class(widget_class);

  gtk_binding_entry_add_signal(binding_set, GDK_KEY_space, (GdkModifierType) 0, "popup", 0);
  gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Space, (GdkModifierType) 0, "popup", 0);
  gtk_binding_entry_add_signal(binding_set, GDK_KEY_Return, (GdkModifierType) 0, "popup", 0);
  gtk_binding_entry_add_signal(binding_set, GDK_KEY_ISO_Enter, (GdkModifierType) 0, "popup", 0);
  gtk_binding_entry_add_signal(binding_set, GDK_KEY_KP_Enter, (GdkModifierType) 0, "popup", 0);
  gtk_binding_entry_add_signal(binding_set, GDK_KEY_Escape, (GdkModifierType) 0, "popdown", 0);
}

static void
ring_popup_button_init(RingPopupButton *button)
{
    auto *priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

    // button properties
    gtk_widget_set_can_focus(GTK_WIDGET(button), TRUE);
    gtk_widget_set_receives_default(GTK_WIDGET(button), TRUE);
    gtk_button_set_focus_on_click(GTK_BUTTON(button), FALSE);

    // instantiate the docks (the popover or window)
    priv->dock = GTK_WIDGET(g_object_new(GTK_TYPE_WINDOW,
                                 "type", GTK_WINDOW_POPUP,
                                 "decorated", FALSE,
                                 "can-focus", FALSE,
                                 "name", "gtk-scalebutton-popup-window",
                                 NULL));

    priv->frame = GTK_WIDGET(g_object_new(GTK_TYPE_FRAME,
                                 "visible", TRUE,
                                 "can-focus", FALSE,
                                 "label-xalign", 0.0,
                                 "shadow-type", GTK_SHADOW_OUT,
                                 NULL));

    gtk_container_add(GTK_CONTAINER(priv->dock), priv->frame);

    // insert test image into frame
    auto pixbuf = gdk_pixbuf_new_from_resource_at_scale("/cx/ring/RingGnome/ring-logo-blue",
                                                                  -1, 75, TRUE, NULL);
    auto image = gtk_image_new_from_pixbuf(pixbuf);

    gtk_container_add(GTK_CONTAINER(priv->frame), image);

    priv->content = image;

    // connect signals
    g_signal_connect(priv->dock, "button-press-event", G_CALLBACK(cb_dock_button_press), button);
    g_signal_connect(priv->dock, "grab-broken-event", G_CALLBACK(cb_dock_grab_broken_event), button);
    g_signal_connect(priv->dock, "grab-notify", G_CALLBACK(cb_dock_grab_notify), button);
    g_signal_connect(priv->dock, "key-release-event", G_CALLBACK(cb_dock_key_release), button);
}

static void
ring_popup_button_finalize(GObject *object)
{
    auto button = RING_POPUP_BUTTON(object);
  RingPopupButtonPrivate *priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

  G_OBJECT_CLASS (ring_popup_button_parent_class)->finalize (object);
}

static void
ring_popup_button_dispose(GObject *object)
{
    auto button = RING_POPUP_BUTTON(object);
  RingPopupButtonPrivate *priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

  if (priv->dock)
    {
      gtk_widget_destroy (priv->dock);
      priv->dock = NULL;
    }

  G_OBJECT_CLASS (ring_popup_button_parent_class)->dispose (object);
}

/**
 * ring_popup_button_new:
 * @size: (type int): a stock icon size
 * @min: the minimum value of the scale (usually 0)
 * @max: the maximum value of the scale (usually 100)
 * @step: the stepping of value when a scroll-wheel event,
 *        or up/down arrow event occurs (usually 2)
 * @icons: (allow-none) (array zero-terminated=1): a %NULL-terminated
 *         array of icon names, or %NULL if you want to set the list
 *         later with ring_popup_button_set_icons()
 *
 * Creates a #RingPopupButton, with a range between @min and @max, with
 * a stepping of @step.
 *
 * Return value: a new #RingPopupButton
 *
 * Since: 2.12
 */
GtkWidget *
ring_popup_button_new(GtkWidget *content)
{
  auto button = g_object_new (RING_TYPE_POPUP_BUTTON, NULL);

  return GTK_WIDGET (button);
}

static gboolean
gtk_scale_popup(GtkWidget *widget,
                GdkEvent  *event,
                guint32    time)
{
  GtkAllocation allocation, dock_allocation, frame_allocation;
  RingPopupButton *button;
  RingPopupButtonPrivate *priv;
  gint x, y, m, dx, dy, sx, sy, startoff;
  GdkScreen *screen;
  GdkDevice *device, *keyboard, *pointer;
  gint min_content_size;

  button = RING_POPUP_BUTTON(widget);
  priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

  screen = gtk_widget_get_screen (widget);
  gtk_widget_get_allocation (widget, &allocation);

  /* position roughly */
  gtk_window_set_screen (GTK_WINDOW (priv->dock), screen);

  gdk_window_get_origin (gtk_widget_get_window (widget),
                         &x, &y);
  x += allocation.x;
  y += allocation.y;

  GtkRequisition min_content_requ, natural_content_requ;
  gtk_widget_get_preferred_size(priv->content, &min_content_requ, &natural_content_requ);

  gtk_window_move (GTK_WINDOW (priv->dock), x, y - (natural_content_requ.height / 2));

  gtk_widget_show_all (priv->dock);

  gdk_window_get_origin (gtk_widget_get_window (priv->dock),
                         &dx, &dy);
  gtk_widget_get_allocation (priv->dock, &dock_allocation);
  dx += dock_allocation.x;
  dy += dock_allocation.y;


  gdk_window_get_origin (gtk_widget_get_window (priv->frame),
                         &sx, &sy);
  gtk_widget_get_allocation (priv->frame, &frame_allocation);
  sx += frame_allocation.x;
  sy += frame_allocation.y;

  /* position (needs widget to be shown already) */
  min_content_size = min_content_requ.height;

      startoff = sy - dy;

      x += (allocation.width - dock_allocation.width) / 2;
      y -= startoff;
      y -= min_content_size / 2;
      m = frame_allocation.height - min_content_size;

  /* Make sure the dock stays inside the monitor */
  if (event->type == GDK_BUTTON_PRESS)
    {
      GtkAllocation d_allocation;
      int monitor;
      GdkEventButton *button_event = (GdkEventButton *) event;
      GdkRectangle rect;
      GtkWidget *d;

      d = GTK_WIDGET (priv->dock);
      monitor = gdk_screen_get_monitor_at_point (screen,
						 button_event->x_root,
						 button_event->y_root);
      gdk_screen_get_monitor_workarea (screen, monitor, &rect);

        y += button_event->y;

      /* Move the dock, but set is_moved so we
       * don't forward the first click later on,
       * as it could make the scale go to the bottom */
      gtk_widget_get_allocation (d, &d_allocation);
      if (y < rect.y)
        {
          y = rect.y;
        }
      else if (y + d_allocation.height > rect.height + rect.y)
        {
          y = rect.y + rect.height - d_allocation.height;
        }

      if (x < rect.x)
        {
          x = rect.x;
        }
      else if (x + d_allocation.width > rect.width + rect.x)
        {
          x = rect.x + rect.width - d_allocation.width;
        }
    }

  gtk_window_move (GTK_WINDOW (priv->dock), x, y);

  if (event->type == GDK_BUTTON_PRESS)
    GTK_WIDGET_CLASS (ring_popup_button_parent_class)->button_press_event (widget, (GdkEventButton *) event);

  device = gdk_event_get_device (event);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      keyboard = device;
      pointer = gdk_device_get_associated_device (device);
    }
  else
    {
      pointer = device;
      keyboard = gdk_device_get_associated_device (device);
    }

  /* grab focus */
  gtk_device_grab_add (priv->dock, pointer, TRUE);

  if (gdk_device_grab (pointer, gtk_widget_get_window (priv->dock),
                       GDK_OWNERSHIP_WINDOW, TRUE,
                       (GdkEventMask) (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                       GDK_POINTER_MOTION_MASK), NULL, time) != GDK_GRAB_SUCCESS)
    {
      gtk_device_grab_remove (priv->dock, pointer);
      gtk_widget_hide (priv->dock);
      return FALSE;
    }

  if (gdk_device_grab (keyboard, gtk_widget_get_window (priv->dock),
                       GDK_OWNERSHIP_WINDOW, TRUE,
                       (GdkEventMask) (GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK),
                       NULL, time) != GDK_GRAB_SUCCESS)
    {
      gdk_device_ungrab (pointer, time);
      gtk_device_grab_remove (priv->dock, pointer);
      gtk_widget_hide (priv->dock);
      return FALSE;
    }

  gtk_widget_grab_focus (priv->dock);
  priv->grab_keyboard = keyboard;
  priv->grab_pointer = pointer;


  gtk_widget_grab_focus (priv->content);

  return TRUE;
}

static gboolean
ring_popup_button_press(GtkWidget      *widget,
                        GdkEventButton *event)
{
  return gtk_scale_popup (widget, (GdkEvent *) event, event->time);
}

static void
ring_popup_button_popup(GtkWidget *widget)
{
  GdkEvent *ev;

  /* This is a callback for a keybinding signal,
   * current event should  be the key event that
   * triggered it.
   */
  ev = gtk_get_current_event ();

  if (ev->type != GDK_KEY_PRESS &&
      ev->type != GDK_KEY_RELEASE)
    {
      gdk_event_free (ev);
      ev = gdk_event_new (GDK_KEY_RELEASE);
      ev->key.time = GDK_CURRENT_TIME;
    }

  gtk_scale_popup (widget, ev, ev->key.time);
  gdk_event_free (ev);
}

static gboolean
ring_popup_button_key_release(GtkWidget   *widget,
                              GdkEventKey *event)
{
  return gtk_bindings_activate_event (G_OBJECT (widget), event);
}

/* This is called when the grab is broken for
 * either the dock, or the scale itself */
static void
ring_popup_button_grab_notify(RingPopupButton *button,
                              gboolean        was_grabbed)
{
  RingPopupButtonPrivate *priv;
  GtkWidget *toplevel, *grab_widget;
  GtkWindowGroup *group;

  priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

  if (!priv->grab_pointer ||
      !gtk_widget_device_is_shadowed (GTK_WIDGET (button), priv->grab_pointer))
    return;

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (button));

  if (GTK_IS_WINDOW (toplevel))
    group = gtk_window_get_group (GTK_WINDOW (toplevel));
  else
    group = gtk_window_get_group (NULL);

  grab_widget = gtk_window_group_get_current_device_grab (group, priv->grab_pointer);

  if (grab_widget &&
      gtk_widget_is_ancestor (grab_widget, priv->dock))
    return;

  gdk_device_ungrab (priv->grab_keyboard, GDK_CURRENT_TIME);
  gdk_device_ungrab (priv->grab_pointer, GDK_CURRENT_TIME);
  gtk_device_grab_remove (priv->dock, priv->grab_pointer);

  priv->grab_keyboard = NULL;
  priv->grab_pointer = NULL;

  /* hide again */
  gtk_widget_hide (priv->dock);
}

static void
cb_dock_grab_notify(GtkWidget *widget,
                    gboolean   was_grabbed,
                    gpointer   user_data)
{
  RingPopupButton *button = (RingPopupButton *) user_data;

  ring_popup_button_grab_notify (button, was_grabbed);
}

static gboolean
cb_dock_grab_broken_event(GtkWidget *widget,
                          gboolean   was_grabbed,
                          gpointer   user_data)
{
  RingPopupButton *button = (RingPopupButton *) user_data;

  ring_popup_button_grab_notify (button, FALSE);

  return FALSE;
}

/* Scale callbacks  */

static void
ring_popup_button_release_grab(RingPopupButton *button,
                               GdkEventButton *event)
{
  GdkEventButton *e;
  RingPopupButtonPrivate *priv;

  priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

  /* ungrab focus */
  gdk_device_ungrab (priv->grab_keyboard, event->time);
  gdk_device_ungrab (priv->grab_pointer, event->time);
  gtk_device_grab_remove (priv->dock, priv->grab_pointer);

  priv->grab_keyboard = NULL;
  priv->grab_pointer = NULL;

  /* hide again */
  gtk_widget_hide (priv->dock);

  e = (GdkEventButton *) gdk_event_copy ((GdkEvent *) event);
  e->window = gtk_widget_get_window (GTK_WIDGET (button));
  e->type = GDK_BUTTON_RELEASE;
  gtk_widget_event (GTK_WIDGET (button), (GdkEvent *) e);
  e->window = event->window;
  gdk_event_free ((GdkEvent *) e);
}

static gboolean
cb_dock_button_press(GtkWidget      *widget,
                     GdkEventButton *event,
                     gpointer        user_data)
{
  RingPopupButton *button = RING_POPUP_BUTTON(user_data);

  if (event->type == GDK_BUTTON_PRESS)
    {
      ring_popup_button_release_grab (button, event);
      return TRUE;
    }

  return FALSE;
}

static void
ring_popup_button_popdown(GtkWidget *widget)
{
  RingPopupButton *button;
  RingPopupButtonPrivate *priv;

  button = RING_POPUP_BUTTON(widget);
  priv = RING_POPUP_BUTTON_GET_PRIVATE(button);

  /* ungrab focus */
  gdk_device_ungrab (priv->grab_keyboard, GDK_CURRENT_TIME);
  gdk_device_ungrab (priv->grab_pointer, GDK_CURRENT_TIME);
  gtk_device_grab_remove (priv->dock, priv->grab_pointer);

  priv->grab_keyboard = NULL;
  priv->grab_pointer = NULL;

  /* hide again */
  gtk_widget_hide (priv->dock);
}

static gboolean
cb_dock_key_release(GtkWidget   *widget,
                    GdkEventKey *event,
                    gpointer     user_data)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      ring_popup_button_popdown (GTK_WIDGET (user_data));
      return TRUE;
    }

  if (!gtk_bindings_activate_event (G_OBJECT (widget), event))
    {
      /* The popup hasn't managed the event, pass onto the button */
      gtk_bindings_activate_event (G_OBJECT (user_data), event);
    }

  return TRUE;
}
