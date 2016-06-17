#include "smartInfo.h"



SmartInfo::SmartInfo(){
  information = "test";
  GtkWidget *window;
  GtkWidget *align;
  GtkWidget *lblInfo;
  GtkWidget *box;


  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "SmartInfo");
  gtk_window_set_default_size(GTK_WINDOW(window), 300, 200);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(window), 5);
  align = gtk_alignment_new(0, 0, 0, 0);
  lblInfo = gtk_label_new(information);
  box = gtk_event_box_new();

  gtk_container_add(GTK_CONTAINER(align), lblInfo);
  gtk_container_add(GTK_CONTAINER(box), align);
  gtk_container_add(GTK_CONTAINER(window), box);

  //stop pull information when we close the window
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(closeSmartInfo), NULL);

  gtk_widget_show_all(window);

  gtk_main();
}

void SmartInfo::closeSmartInfo()
{
    SmartInfoHub::smartInfo(0);
}

void SmartInfo::refreshInfo(int a) {
    information = g_strdup_printf("%i", a);
}
