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

// GTK+ related
#include <gtk/gtk.h>

// std
#include <memory>

// LRC
#include <conversationmodel.h>

#include "accountcontainer.h"
#include <data/account.h>

G_BEGIN_DECLS

#define CONVERSATION_POPUP_MENU_TYPE            (conversation_popup_menu_get_type ())
#define CONVERSATION_POPUP_MENU(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CONVERSATION_POPUP_MENU_TYPE, ConversationPopupMenu))
#define CONVERSATION_POPUP_MENU_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), CONVERSATION_POPUP_MENU_TYPE, ConversationPopupMenuClass))
#define IS_CONVERSATION_POPUP_MENU(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), CONVERSATION_POPUP_MENU_TYPE))
#define IS_CONVERSATION_POPUP_MENU_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), CONVERSATION_POPUP_MENU_TYPE))

typedef struct _ConversationPopupMenu      ConversationPopupMenu;
typedef struct _ConversationPopupMenuClass ConversationPopupMenuClass;

GType      conversation_popup_menu_get_type (void) G_GNUC_CONST;
//GtkWidget *conversation_popup_menu_new      (GtkTreeView *treeview, std::shared_ptr<lrc::account::Info> accountInfo);
GtkWidget *conversation_popup_menu_new      (GtkTreeView *treeview, AccountContainer* accountContainer);
gboolean   conversation_popup_menu_show     (ConversationPopupMenu *self, GdkEventButton *event);

G_END_DECLS
