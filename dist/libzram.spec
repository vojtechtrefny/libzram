Name:           libzram
Version:        0.1
Release:        %autorelease
Summary:        Library for zRAM management

License:        LGPL-2.1-or-later
URL:            https://github.com/vojtechtrefny/libzram
Source:         https://github.com/vojtechtrefny/libzram/releases/download/%{version}-%{release}/%{name}-%{version}.tar.xz

BuildRequires:  meson
BuildRequires:  libblockdev-utils-devel
BuildRequires:  glib2-devel

%description
%{summary}.

%package devel
Summary:        Development libraries and header files for %{name}
Requires:       %{name}%{?_isa} = %{?epoch:%{epoch}:}%{version}-%{release}

%description devel
%{summary}.

%prep
%autosetup -p1 -n %{name}-%{version}

%build
%meson
%meson_build

%install
%meson_install

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%{_libdir}/%{name}.so.*
%{_libdir}/girepository*/Zram*.typelib

%files devel
%{_libdir}/%{name}.so
%{_includedir}/%{name}.h
%{_libdir}/pkgconfig/zram.pc
%{_datadir}/gir*/Zram*.gir

%changelog
%autochangelog
