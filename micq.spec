%define name		micq
%define version	0.4.10.2
%define release	1

%define prefix	/usr

Summary:		micq - text based ICQ client with many features
Name:			%{name}
Version:		%{version}
Release:		%{release}
Source:			%{name}-%{version}.tgz
URL:			http://www.micq.org
Group:			Networking/ICQ
Packager:		Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
Copyright:		GPL-2
BuildRoot:		%{_tmppath}/build-%{name}-%{version}
Prefix:			%{prefix}

%description
mICQ is a portable, small, yet powerful console based ICQ client. It
supports password changing, auto-away, creation of new accounts, and other
features that makes it a very complete yet simple client supporting the
current ICQ v8 protocol.

A lot of other ICQ clients are based in spirit on mICQ, nevertheless
mICQ is still _the_ console based ICQ client.
      
Authors: Matthew D. Smith (dead)
         Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>

%changelog
* Tue Feb 27 2003 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream bug fix release 0.4.10.2

* Tue Jan 24 2003 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream bug fix release 0.4.10.1

* Tue Jan 07 2003 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.4.10

* Tue Oct 03 2002 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.4.9.4

* Tue Aug 26 2002 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.4.9.3

* Tue Aug 08 2002 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release

* Tue Jun 11 2002 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- first RPM

%prep
rm -rf $RPM_BUILD_ROOT

%setup -q -n %{name}-%{version}

%build
%configure --disable-dependency-tracking CFLAGS=-O4
make

%install

%makeinstall
%{__mkdir_p} $RPM_BUILD_ROOT/usr/lib/menu
cat << EOF > $RPM_BUILD_ROOT/usr/lib/menu/micq
?package(micq):needs=text section=Networking/ICQ \
  title="mICQ" command="/usr/bin/micq" hints="ICQ client"\
  icon=/usr/X11R6/include/X11/pixmaps/micq.xpm
EOF
install -D -m 644 doc/micq.xpm $RPM_BUILD_ROOT/usr/X11R6/include/X11/pixmaps/micq.xpm

%clean
rm -rf "${RPM_BUILD_ROOT}"

%files
%defattr(-,root,root,0755)
%doc NEWS AUTHORS FAQ README TODO
%doc doc/README.i18n doc/README.logformat doc/icq091.txt doc/icqv7.txt
%{_bindir}/*
%{_datadir}/micq
/usr/lib/menu/micq

%post
%{update_menus} || true

%postun
%{clean_menus} || true
