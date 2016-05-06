/*
 *  Copyright (C) 2016 Savoir-faire Linux Inc.
 *  Author: Nicolas Jager <nicolas.jager@savoirfairelinux.com>
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

#ifndef _AVATARMANIPULATION_H
#define _AVATARMANIPULATION_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define AVATAR_MANIPULATION_TYPE            (avatar_manipulation_get_type ())
#define AVATAR_MANIPULATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), AVATAR_MANIPULATION_TYPE, AvatarManipulation))
#define AVATAR_MANIPULATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), AVATAR_MANIPULATION_TYPE, AvatarManipulation))
#define IS_AVATAR_MANIPULATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), AVATAR_MANIPULATION_TYPE))
#define IS_AVATAR_MANIPULATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), AVATAR_MANIPULATION_TYPE))


typedef struct _AvatarManipulation      AvatarManipulation;
typedef struct _AvatarManipulationClass AvatarManipulationClass;


GType      avatar_manipulation_get_type (void) G_GNUC_CONST;
GtkWidget *avatar_manipulation_new      (void);

G_END_DECLS

#endif /* _AVATARMANIPULATION_H */
