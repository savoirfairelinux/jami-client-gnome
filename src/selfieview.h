/*
 *  Copyright (C) 2018 Savoir-faire Linux Inc.
 *  Author: Hugo Lefeuvre <hugo.lefeuvre@savoirfairelinux.com>
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

#ifndef _SELFIEVIEW_H
#define _SELFIEVIEW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SELFIE_VIEW_TYPE            (selfie_view_get_type ())
#define SELFIE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SELFIE_VIEW_TYPE, SelfieView))
#define SELFIE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), SELFIE_VIEW_TYPE, SelfieView))
#define IS_SELFIE_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), SELFIE_VIEW_TYPE))
#define IS_SELFIE_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), SELFIE_VIEW_TYPE))


typedef struct _SelfieView      SelfieView;
typedef struct _SelfieViewClass SelfieViewClass;

/**
 * TODO document states
 */
typedef enum
{
    SELFIE_VIEW_STATE_INIT,
    SELFIE_VIEW_STATE_PIC,
    SELFIE_VIEW_STATE_PIC_END,
    SELFIE_VIEW_STATE_VIDEO,
    SELFIE_VIEW_STATE_VIDEO_END,
    SELFIE_VIEW_STATE_SOUND,
    SELFIE_VIEW_STATE_SOUND_END
} SelfieViewState;


GType       selfie_view_get_type        (void) G_GNUC_CONST;
GtkWidget   *selfie_view_new            (void);
const char  *selfie_view_get_save_file  (SelfieView *self);

G_END_DECLS

#endif /* _SELFIEVIEW_H */
