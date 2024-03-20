%define _prefix /opt
%define _unpackaged_files_terminate_build 0

Name:       ru.avroid.nlaudit-listener
Summary:    netlink socket listener
Version:    1.0
Release:    1
License:    TODO
URL:        TODO
BuildRequires:  gcc

Prefix: %{_prefix}

%description
netlink socket listener

%build
make

%install
mkdir -p %{buildroot}/%{_bindir}
cp ./nlaudit-listener %{buildroot}/%{_bindir}/

%files
%defattr(755,root,root,-)
%{_bindir}/nlaudit-listener
