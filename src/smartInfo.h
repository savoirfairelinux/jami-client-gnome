#ifndef SMARTINFO_H_
#define SMARTINFO_H_

#include <gtk/gtk.h>
#include <smartInfoHub.h>

class SmartInfo
{
  public:
    SmartInfo();
    static void closeSmartInfo();
    static void refreshInfo(int a);
  private:
    static const gchar* information;
};

#endif
