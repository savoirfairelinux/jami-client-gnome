%define name        ring-gnome
%define version     0.1.0
%define release     1

Name:               %{name}
Version:            %{version}
Release:            %{release}%{?dist}
Summary:            SIP/IAX2 compatible enterprise-class software phone
Group:              Applications/Internet
License:            GPLv3
URL:                http://ring.cx/
Source:             /var/lib/joulupukki/ring-gnome
BuildRequires:      autoconf automake libtool dbus-devel pcre-devel yaml-cpp-devel gcc-c++
BuildRequires:      boost-devel dbus-c++-devel dbus-devel libupnp-devel qt5-qtbase-devel
BuildRequires:      gnome-icon-theme-symbolic chrpath check astyle gnutls-devel yasm git
BuildRequires:      cmake clutter-gtk-devel clutter-devel glib2-devel gtk3-devel
Requires:           gnome-icon-theme-symbolic

%description
SFLphone is a robust standards-compliant enterprise software phone,
for desktop and embedded systems. It is designed to handle
several hundreds of calls a day. It supports both SIP and IAX2
protocols.

%prep
%setup -q
# Daemon
echo "# Downloading Ring Daemon ..."
rm -rf ring
git clone https://gerrit-ring.savoirfairelinux.com/ring daemon
cd daemon 
git checkout release-2.0.x
rm -rf .git
cd ..
# LibRingClient
echo "# Downloading Lib Ring Client ..."
rm -rf libringclient
git clone git://anongit.kde.org/libringclient.git libringclient
cd libringclient
git checkout release-0.1.x
rm -rf .git
cd ..


%build
rm -rf %{buildroot}
cd libringclient
mkdir -p build
mkdir -p install
cd build
cmake -DRING_BUILD_DIR=$(pwd)/../../daemon/src -DCMAKE_INSTALL_PREFIX=$(pwd)/../../libringclient/install -DENABLE_VIDEO=true -DENABLE_STATIC=true ..
make -j 2
make install
cd ../..
mkdir -p build
mkdir -p install
cd build
cmake -DCMAKE_INSTALL_PREFIX=$(pwd)/../libringclient/install -DLIB_RING_CLIENT_LIBRARY=$(pwd)/../libringclient/install/lib/libringclient_static.a -DENABLE_STATIC=true ..
LDFLAGS="-lpthread" make -j 2

%install
cd build
make install
echo "SDSDDDDDDDDDDDDDDDDDDDDD"
pwd
ls -Rl
mkdir -p %{buildroot}/%{_bindir}
ls -Rl %{buildroot}
mv gnome-ring %{buildroot}/%{_bindir}


%files 
%defattr(-,root,root,-)
%{_bindir}/gnome-ring

%changelog
* Fri Mar 27 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 0.1.0-1
- New upstream version
