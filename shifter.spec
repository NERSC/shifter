Summary:  shifter
Name:     shifter
Version:  16.04.0pre1
License:  BSD (LBNL-modified)
Group:    System Environment/Base
URL:      https://github.com/NERSC/shifter
Packager: Douglas Jacobsen <dmjacobsen@lbl.gov>
Source0:  %{name}-%{version}.tar.gz

%package  shifter-runtime
Summary:  runtime component for shifter (formerly udiRoot)
Group:    System Environment/Base
BuildRequires: munge
BuildRequires: libcurl libcurl-devel
BuildRequires: json-c json-c-devel
%description shifter-runtime
runtime and user interface components of shifter

%changelog
* Sun Apr 24 2016 Douglas Jacobsen <dmjacobsen@lbl.gov> - 16.04.0pre1-1
- Initial version of spec file
