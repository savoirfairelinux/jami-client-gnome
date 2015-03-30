%define name        ring-gnome
%define version     0.1.0
%define release     1

Name:               %{name}
Version:            %{version}
Release:            %{release}
Summary:            Ring GNOME client
Group:              Applications/Internet
License:            GPLv3
URL:                http://ring.cx/
Source:             ring-gnome
BuildRequires:      autoconf automake libtool dbus-devel pcre-devel yaml-cpp-devel gcc-c++
BuildRequires:      boost-devel dbus-c++-devel dbus-devel libupnp-devel qt5-qtbase-devel
BuildRequires:      gnome-icon-theme-symbolic chrpath check astyle gnutls-devel yasm git
BuildRequires:      cmake clutter-gtk-devel clutter-devel glib2-devel gtk3-devel
Requires:           gnome-icon-theme-symbolic

%description
Ring GNOME client
Ring is a secured and distributed communication software.


%prep
%setup -q
# Daemon
echo "# Downloading Ring Daemon ..."
rm -rf ring
git clone https://gerrit-ring.savoirfairelinux.com/ring daemon
cd daemon 
git checkout master
rm -rf .git
cd ..
# LibRingClient
echo "# Downloading Lib Ring Client ..."
rm -rf libringclient
git clone git://anongit.kde.org/libringclient.git libringclient
cd libringclient
git checkout master
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
# TODO test this
#cmake -DCMAKE_INSTALL_PREFIX=%{buildroot} -DLIB_RING_CLIENT_LIBRARY=$(pwd)/../libringclient/install/lib/libringclient_static.a -DENABLE_STATIC=true ..
LDFLAGS="-lpthread" make -j 2

%install
cd build
make install
# TODO clean this by a better cmake command
mkdir -p %{buildroot}/%{_bindir}
mv ../libringclient/install/bin/gnome-ring %{buildroot}/%{_bindir} 
mkdir -p %{buildroot}/%{_datadir}/icons/hicolor/scalable/apps/
mv ../libringclient/install/share/icons/hicolor/scalable/apps/ring.svg %{buildroot}/%{_datadir}/icons/hicolor/scalable/apps/ring.svg
mkdir -p %{buildroot}/%{_datadir}/appdata
mv ../libringclient/install/share/appdata/gnome-ring.appdata.xml %{buildroot}/%{_datadir}/appdata/gnome-ring.appdata.xml
mkdir -p %{buildroot}/%{_datadir}/applications
mv ../libringclient/install/share/applications/gnome-ring.desktop %{buildroot}/%{_datadir}/applications/gnome-ring.desktop
sed -i "s#Icon=.*#Icon=%{_datadir}/icons/hicolor/scalable/apps/ring.svg#g" %{buildroot}/%{_datadir}/applications/gnome-ring.desktop


%files 
%defattr(-,root,root,-)
%{_bindir}/gnome-ring
%{_datadir}/applications/gnome-ring.desktop
%{_datadir}/icons/hicolor/scalable/apps/ring.svg
%{_datadir}/appdata/gnome-ring.appdata.xml


%changelog
* Fri Mar 27 2015 Thibault Cohen <thibault.cohen@savoirfairelinux.com> - 0.1.0-1
- New upstream version
