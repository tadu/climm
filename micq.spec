%define name		micq
%define version	0.4.9
%define release	0

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
BuildRoot:		/var/tmp/build-%{name}-%{version}
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
* Tue Jun 11 2002 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
- first RPM

%prep
%setup
./prepare

%build
%configure
make

%install
%makeinstall
for mancat in 1 5 7; do
        %{__mkdir_p} "${RPM_BUILD_ROOT}%{_mandir}/man${mancat}"
        %{__install} -m0644 doc/*.${mancat} \
                "${RPM_BUILD_ROOT}%{_mandir}/man${mancat}/"
done
%{__mkdir_p} $RPM_BUILD_ROOT/usr/lib/menu
cat << EOF > $RPM_BUILD_ROOT/usr/lib/menu/micq
?package(micq):needs=text section=Networking/ICQ \
  title="mICQ" command="/usr/bin/micq"
EOF

%clean
rm -rf "${RPM_BUILD_ROOT}"

%files
%defattr(-,root,root,0755)
%doc NEWS README TODO
%doc doc/README doc/README.SOCKS5 doc/icq091.txt doc/icqv7.txt
%doc doc/html
%{_bindir}/*
%{_datadir}/micq
/usr/lib/menu/micq
%doc %{_mandir}/*/*

%post
%{update_menus} || true

%postun
%{clean_menus} || true
