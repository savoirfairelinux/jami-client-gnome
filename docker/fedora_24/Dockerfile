FROM fedora:24

RUN dnf upgrade -y
RUN dnf install rpm-build tar rsync git make autoconf automake cmake libtool dbus-devel pcre-devel yaml-cpp-devel gcc-c++ \
    boost-devel dbus-c++-devel dbus-devel libupnp-devel qt5-qtbase-devel \
    gnome-icon-theme-symbolic chrpath check astyle gnutls-devel yasm git \
    cmake clutter-gtk-devel clutter-devel glib2-devel gtk3-devel evolution-data-server-devel \
    libnotify-devel qt5-qttools-devel gettext qrencode-devel libappindicator-gtk3-devel \
    NetworkManager-glib-devel -y
