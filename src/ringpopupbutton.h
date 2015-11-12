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

#ifndef _RING_POPUP_BUTTON_H
#define _RING_POPUP_BUTTON_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RING_TYPE_POPUP_BUTTON            (ring_popup_button_get_type ())
#define RING_POPUP_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RING_TYPE_POPUP_BUTTON, RingPopupButton))
#define RING_POPUP_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), RING_TYPE_POPUP_BUTTON, RingPopupButtonClass))
#define RING_IS_POPUP_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_TYPE_POPUP_BUTTON))
#define RING_IS_POPUP_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RING_TYPE_POPUP_BUTTON))

typedef struct _RingPopupButton      RingPopupButton;
typedef struct _RingPopupButtonClass RingPopupButtonClass;

GType      ring_popup_button_get_type  (void) G_GNUC_CONST;

/**
 * For gtk+ >= 3.12 clicking the button will result in a GtkPopover to appear above it.
 * For lower versions, it will be a GtkWindow of type popup.
 * The contents of the popover can be any GtkWidget or NULL.
 */
GtkWidget *ring_popup_button_new       (GtkWidget *contents);
// GtkBin    *ring_popup_button_get_popup (RingPopupButton *button);

G_END_DECLS

#endif /* _RING_POPUP_BUTTON_H */
