#ifndef _RINGMAINWINDOW_H
#define _RINGMAINWINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define RING_MAIN_WINDOW_TYPE (ring_main_window_get_type ())
#define RING_MAIN_WINDOW(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), RING_MAIN_WINDOW_TYPE, RingMainWindow))


typedef struct _RingMainWindow      RingMainWindow;
typedef struct _RingMainWindowClass RingMainWindowClass;


GType      ring_main_window_get_type (void) G_GNUC_CONST;
GtkWidget *ring_main_window_new      (GtkApplication *app);

G_END_DECLS

#endif /* _RINGMAINWINDOW_H */
