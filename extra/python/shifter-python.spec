#
# spec file for package shifter-skopeo
#


Name:           shifter-python
Version:        21.12
Release:        1
Summary:	Python packages
License:        Apache License 2.0
Group:          System Environment/Base
#Url:
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Requires:       skopeo
BuildRequires:  python3-pip
%description
This provides python tools to handle the image convesion for Shifter.

%prep

%build

%install
pip3 install -t $RPM_BUILD_ROOT/opt/shifter-python-%{version} pymongo==3.6
pip3 install -t $RPM_BUILD_ROOT/opt/shifter-python-%{version} sanic==20.12.3

%post
%postun

%files
%defattr(-,root,root)
/opt/shifter-python-%{version}

%changelog
* Fri Dec 3 2021 Shane Canon <scanon@lbl.gov> - 21.12-1
- Initial version

