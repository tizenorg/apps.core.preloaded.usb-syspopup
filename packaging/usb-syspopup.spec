Name:       org.tizen.usb-syspopup
Summary:    USB system popup
Version:    0.0.10
Release:    0
Group:      framework-system
License:    APLv2
Source0:    org.tizen.usb-syspopup-%{version}.tar.gz
Source1:    org.tizen.usb-syspopup.manifest
Source2:    org.tizen.usbotg-syspopup.manifest

BuildRequires:  cmake
BuildRequires:  libattr-devel
BuildRequires:  gettext-devel
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(bundle)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-x)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(devman)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(appsvc)
BuildRequires:  pkgconfig(pmapi)
BuildRequires:  pkgconfig(syspopup)

%description
Description: USB system popup

%package -n org.tizen.usbotg-syspopup
Summary:    system-popup application (usb otg popup)
Group:      main
Requires:   %{name} = %{version}-%{release}

%description -n org.tizen.usbotg-syspopup
system-popup application (usb otg popup).

%prep
%setup -q
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix}

%build
cp %{SOURCE1} .
cp %{SOURCE2} .

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install


%post 
vconftool set -t int db/setting/select_popup_btn "0" -u 5000 -f

%files
%manifest %{name}.manifest
%defattr(440,root,root,-)
%attr(540,app,app) /usr/apps/org.tizen.usb-syspopup/bin/usb-syspopup
%attr(440,app,app) /usr/apps/org.tizen.usb-syspopup/res/locale/*/LC_MESSAGES/usb-syspopup.mo
/usr/share/packages/org.tizen.usb-syspopup.xml

%files -n org.tizen.usbotg-syspopup
%manifest %{name}.manifest
%defattr(440,root,root,-)
%attr(540,app,app) /usr/apps/org.tizen.usbotg-syspopup/bin/usbotg-syspopup
/usr/share/packages/org.tizen.usbotg-syspopup.xml
%attr(440,app,app) /usr/apps/org.tizen.usbotg-syspopup/res/icons/usb_icon.png
