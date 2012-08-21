Name:       org.tizen.usb-syspopup
Summary:    USB system popup
Version: 	0.0.1
Release:    4
Group:      TO_BE/FILLED_IN
License:    TO_BE/FILLED_IN
Source0:    org.tizen.usb-syspopup-%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  libattr-devel
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(syspopup)

%description
Description:  USB system popup


%prep
%setup -q
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post 
chown 5000:5000 /usr/bin/usb-syspopup
chown 5000:5000 /usr/share/locale/*/LC_MESSAGES/usb-syspopup.mo
vconftool set -t int db/setting/select_popup_btn "0" -u 5000

%files
%defattr(440,root,root,-)
%attr(540,root,root) /usr/bin/usb-syspopup
/usr/share/locale/*/LC_MESSAGES/usb-syspopup.mo
/usr/share/packages/org.tizen.usb-syspopup.xml
