/*
 *  Copyright (C) 2015-2017 Savoir-faire Linux Inc.
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

#pragma once

#include <gtk/gtk.h>

#include "accountcontainer.h"

G_BEGIN_DECLS

#define RING_WELCOME_VIEW_TYPE            (ring_welcome_view_get_type ())
#define RING_WELCOME_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RING_WELCOME_VIEW_TYPE, RingWelcomeView))
#define RING_WELCOME_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), RING_WELCOME_VIEW_TYPE, RingWelcomeViewClass))
#define IS_RING_WELCOME_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), RING_WELCOME_VIEW_TYPE))
#define IS_RING_WELCOME_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RING_WELCOME_VIEW_TYPE))

typedef struct _RingWelcomeView      RingWelcomeView;
typedef struct _RingWelcomeViewClass RingWelcomeViewClass;

GType             ring_welcome_view_get_type (void) G_GNUC_CONST;
GtkWidget*        ring_welcome_view_new      (AccountContainer* accountContainer);
void              ring_welcome_update_view   (RingWelcomeView* self, AccountContainer* accountContainer = nullptr);

G_END_DECLS
