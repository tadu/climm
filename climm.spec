Summary:		text/line based ICQ client with many features%{?_without_tcl: [no Tcl]}%{?_without_ssl: [no SSL]}%{?_without_xmpp: [no XMPP]}
Name:			climm
Version:		0.7.1
Release:		1%{?_without_tcl:.notcl}%{?_without_ssl:.nossl}%{?_without_xmll:.noxmpp}
Source:			climm-%{version}.tgz
URL:			http://www.climm.org/
Group:			Networking/ICQ
Packager:		Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
License:		GPL-2
BuildRoot:		%{_tmppath}/build-climm-%{version}
Prefix:			%{_prefix}

%{!?_without_ssl:BuildRequires: gnutls-devel}
%{!?_without_tcl:BuildRequires: tcl-devel}
%{!?_without_xmpp:BuildRequires: libgloox-devel}

%description
climm is a portable, small, yet powerful console based ICQ client. It
supports password changing, auto-away, creation of new accounts, searching,
file transfer, acknowledged messages, SMS, client identification, logging,
scripting, transcoding, multi-UIN usage and other features that makes it a
very complete yet simple internationalized client supporting the current
ICQ v8 protocol.
It now also supports the XMPP protocol as well as OTR encrypted messages.

It has leading support for (ICQ2002+/ICQ Lite/ICQ2go) unicode encoded
messages unreached by other ICQ clones.

A lot of other ICQ clients are based in spirit on climm, nevertheless
climm is still _the_ console based ICQ client.
      
Authors: Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
         Matthew D. Smith (deceased; up to micq 0.4.8)

%prep
test $RPM_BUILD_ROOT != / && rm -rf $RPM_BUILD_ROOT

%setup -q -n climm-%{version}

%build
%configure --disable-dependency-tracking \
	%{!?_without_tcl:--enable-tcl}%{?_without_tcl:--disable-tcl} \
	%{!?_without_ssl:--enable-ssl}%{?_without_ssl:--disable-ssl} \
	%{!?_without_xmpp:--enable-xmpp}%{?_without_xmpp:--disable-xmpp} \
	--enable-autopackage
make

%install

make install DESTDIR=$RPM_BUILD_ROOT
%if %{?update_menus:1}%{!?update_menus:0}
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/menu
cat << EOF > $RPM_BUILD_ROOT%{_libdir}/menu/climm
?package(climm):needs=text section=Networking/ICQ \
  title="climm" command="%{_bindir}/climm" hints="ICQ client"\
  icon=%{_datadir}/pixmaps/climm.xpm
EOF
install -D -m 644 -p doc/climm.xpm $RPM_BUILD_ROOT%{_datadir}/pixmaps/climm.xpm
%endif

%clean
test $RPM_BUILD_ROOT != / && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc NEWS AUTHORS FAQ README TODO doc/COPYING-GPLv2
%doc doc/README.i18n doc/README.logformat doc/README.ssl COPYING doc/example-climm-event-script
%{_bindir}/*
%{_datadir}/climm
%if %{?update_menus:1}%{!?update_menus:0}
%{_libdir}/menu/climm
%{_datadir}/pixmaps/climm.xpm
%endif
%{_mandir}/man?/*
%{_mandir}/*/man?/*

%if %{?update_menus:1}%{!?update_menus:0}
%post
%{update_menus} || true

%postun
%{clean_menus} || true
%endif

%changelog
* Sat Mar 20 2010 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.7.1

* Sun May 17 2009 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.7

* Sun Feb 22 2009 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.6.4

* Tue Aug 19 2008 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.6.3

* Mon Feb 25 2008 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.6.2

* Sun Oct 14 2007 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.6.1

* Mon Sep 10 2007 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.6

* Mon Jun 04 2007 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.4

* Wed Apr 18 2007 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.3

* Mon Nov 06 2006 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.2

* Sun Jan 16 2006 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.1

* Sun Jun 05 2005 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.0.4

* Sun Apr 27 2005 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.0.3

* Sun Apr 24 2005 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.0.2

* Sun Feb 17 2005 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5.0.1

* Fri Feb 12 2005 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.5

* Sun Dec  5 2004 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.4.99.9

* Sat Jan 17 2004 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- new upstream release 0.4.11

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

