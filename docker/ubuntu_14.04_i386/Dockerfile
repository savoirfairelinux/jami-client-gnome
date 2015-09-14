FROM savoirfairelinux/ring-ubuntu32:14.04

RUN apt-get update && ( apt-get upgrade -y || apt-get upgrade -y || apt-get upgrade -y )
RUN apt-get install -y  autoconf devscripts debianutils debhelper build-essential tar rsync \
    automake libtool libdbus-1-dev \
    libpcre3-dev libyaml-cpp-dev libboost-dev \
    libdbus-c++-dev libupnp-dev qtbase5-dev \
    yasm autopoint unbound-anchor git-core \
    autotools-dev chrpath check astyle \
    libgnutls28-dev cmake libebook1.2-dev gnome-icon-theme-symbolic \
    libclutter-gtk-1.0-dev libclutter-1.0-dev libglib2.0-dev libgtk-3-dev \
    libnotify-dev software-properties-common qttools5-dev-tools gettext --fix-missing
RUN add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update -y
RUN apt-get install gcc-4.9 g++-4.9 cpp-4.9 -y
