Summary:		text/line based ICQ client with many features
Name:			micq
Version:		0.4.10.6
Release:		1
Name:			%{name}
Version:		%{version}
Release:		%{release}
Source:			%{name}-%{version}.tgz
URL:			http://www.micq.org/
Group:			Networking/ICQ
Packager:		Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
License:		GPL-2
BuildRoot:		%{_tmppath}/build-%{name}-%{version}
Prefix:			%{_prefix}

%{?_with_ssl:BuildRequires: openssl-devel}
%{?_with_tcl:BuildRequires: tcl-devel}

%description
mICQ is a portable, small, yet powerful console based ICQ client. It
supports password changing, auto-away, creation of new accounts, searching,
file transfer, acknowledged messages, SMS, client identification, logging,
scripting, transcoding, multi-UIN usage and other features that makes it a
very complete yet simple internationalized client supporting the current
ICQ v8 protocol.

It has leading support for (ICQ2002+/ICQ Lite/ICQ2go) unicode encoded
messages unreached by other ICQ clones.

A lot of other ICQ clients are based in spirit on mICQ, nevertheless
mICQ is still _the_ console based ICQ client.
      
Authors: Matthew D. Smith (deceased)
         Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>

%changelog
* Sat Jan 17 2004 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.4.10.6

* Mon Oct  6 2003 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream bug fix release 0.4.10.5

* Mon Sep 22 2003 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream bug fix release 0.4.10.4
- fixes remote DoS

* Mon May 13 2003 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream bug fix release 0.4.10.3

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
test $RPM_BUILD_ROOT != / && rm -rf $RPM_BUILD_ROOT

%setup -q -n %{name}-%{version}

%build
%configure --disable-dependency-tracking %{?_with_tcl:--enable-tcl} \
	%{?_with_ssl:--enable-ssl} CFLAGS=-O4
make

%install

make install DESTDIR=$RPM_BUILD_ROOT
%if %{?update_menus:1}%{!?update_menus:0}
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/menu
cat << EOF > $RPM_BUILD_ROOT%{_libdir}/menu/micq
?package(micq):needs=text section=Networking/ICQ \
  title="mICQ" command="%{_bindir}/micq" hints="ICQ client"\
  icon=%{_datadir}/pixmaps/micq.xpm
EOF
install -D -m 644 -p doc/micq.xpm $RPM_BUILD_ROOT%{_datadir}/pixmaps/micq.xpm
%endif

%clean
test $RPM_BUILD_ROOT != / && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc NEWS AUTHORS FAQ README TODO COPYING COPYING-GPLv2
%doc doc/README.i18n doc/README.logformat doc/README.ssl
%{_bindir}/*
%{_datadir}/micq
%if %{?update_menus:1}%{!?update_menus:0}
%{_libdir}/menu/micq
%{_datadir}/pixmaps/micq.xpm
%endif
%{_mandir}/man?/*
%{_mandir}/*/man?/*

%if %{?update_menus:1}%{!?update_menus:0}
%post
%{update_menus} || true

%postun
%{clean_menus} || true
%endif
