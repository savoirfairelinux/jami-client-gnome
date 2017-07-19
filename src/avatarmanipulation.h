/*
 *  Copyright (C) 2016-2017 Savoir-faire Linux Inc.
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

/**
 * AvatarManipulationState:
 * @AVATAR_MANIPULATION_STATE_CURRENT: The initial state. The widget will display the current
 *  or the default avatar, with the possible actions being to start the camera (if available) to
 *  take a new photo) or to select an existing image.
 * @AVATAR_MANIPULATION_STATE_PHOTO: The state in which the camera is on and the video is being
 *  shown. The possible actions are to take a snapshot or to return to the previous state.
 * @AVATAR_MANIPULATION_STATE_EDIT: The state after selecting a new image (or snapping a photo). The
 *  user must select the square area they wish to use for the avatar. The possible actions are to
 *  confirm the selection or to return to the previous state.
 */
typedef enum
{
    AVATAR_MANIPULATION_STATE_CURRENT,
    AVATAR_MANIPULATION_STATE_PHOTO,
    AVATAR_MANIPULATION_STATE_EDIT
} AvatarManipulationState;


GType      avatar_manipulation_get_type       (void) G_GNUC_CONST;
GtkWidget *avatar_manipulation_new            (void);

/* used from the account creation wizard */
GtkWidget *avatar_manipulation_new_from_wizard(void);
void       avatar_manipulation_wizard_completed(AvatarManipulation *);

G_END_DECLS

#endif /* _AVATARMANIPULATION_H */
