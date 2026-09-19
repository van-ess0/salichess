# SPDX-FileCopyrightText: 2026 van-ess0 <https://github.com/van-ess0>
# SPDX-License-Identifier: GPL-3.0-or-later

Name:       harbour-salichess

Summary:    Unofficial Lichess client for Sailfish OS
Version:    0.5
Release:    2
Group:      Qt/Qt
License:    GPLv3+
URL:        https://github.com/van-ess0/salichess
Source0:    %{name}-%{version}.tar.bz2
# Providers of the QML modules the app imports, and the Secrets storage
# plugin that keeps the login token.
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   amber-web-authorization
Requires:   nemo-qml-plugin-notifications-qt5
Requires:   libkeepalive
Requires:   sailfishsecretsdaemon-secretsplugins-default
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Svg)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(sailfishsecrets)
BuildRequires:  desktop-file-utils

%description
Unofficial Lichess client for Sailfish OS: play games with friends and
solve puzzles on lichess.org.


%prep
%autosetup -n %{name}-%{version}

%build

# The app reports the package version (About page, user agent).
%qmake5 VERSION=%{version}

%make_build


%install
%qmake5_install


desktop-file-install --delete-original         --dir %{buildroot}%{_datadir}/applications                %{buildroot}%{_datadir}/applications/*.desktop

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png
