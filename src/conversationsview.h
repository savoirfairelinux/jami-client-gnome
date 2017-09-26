/****************************************************************************
 *   Copyright (C) 2017 Savoir-faire Linux                                  *
 *   Author : Nicolas Jäger <nicolas.jager@savoirfairelinux.com>            *
 *   Author : Sébastien Blin <sebastien.blin@savoirfairelinux.com>          *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Lesser General Public             *
 *   License as published by the Free Software Foundation; either           *
 *   version 2.1 of the License, or (at your option) any later version.     *
 *                                                                          *
 *   This library is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *   Lesser General Public License for more details.                        *
 *                                                                          *
 *   You should have received a copy of the GNU General Public License      *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#pragma once

#include <gtk/gtk.h>

#include "accountcontainer.h"

G_BEGIN_DECLS

#define CONVERSATIONS_VIEW_TYPE            (conversations_view_get_type ())
#define CONVERSATIONS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONVERSATIONS_VIEW_TYPE, ConversationsView))
#define CONVERSATIONS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CONVERSATIONS_VIEW_TYPE, ConversationsViewClass))
#define IS_CONVERSATIONS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CONVERSATIONS_VIEW_TYPE))
#define IS_CONVERSATIONS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CONVERSATIONS_VIEW_TYPE))

typedef struct _ConversationsView      ConversationsView;
typedef struct _ConversationsViewClass ConversationsViewClass;

GType      conversations_view_get_type (void) G_GNUC_CONST;
GtkWidget *conversations_view_new(AccountContainer* accountContainer);
void conversations_view_select_conversation(ConversationsView *self, const std::string& uid);

G_END_DECLS
