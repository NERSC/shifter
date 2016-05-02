%{!?_shifter_sysconfdir: %global _shifter_sysconfdir /etc/shifter}
%define _sysconfdir %_shifter_sysconfdir

Summary:  shifter
Name:     shifter
Version:  16.04.0pre1
Release:  1
License:  BSD (LBNL-modified)
Group:    System Environment/Base
URL:      https://github.com/NERSC/shifter
Packager: Douglas Jacobsen <dmjacobsen@lbl.gov>
Source0:  %{name}-%{version}.tar.gz
%description
Shifter - environment containers for HPC

%prep
%setup -q
if [[ ! -e ./configure ]]; then
    ./autogen.sh
fi

%build
## build udiRoot (runtime) first
%configure \
    %{?with_slurm:--with-slurm=%{?with_slurm}}

MAKEFLAGS=%{?_smp_mflags} make

%install
%make_install

%if %{?with_slurm:1}0
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.a
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.la
%else
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.a
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.la
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.so
rm -f $RPM_BUILD_ROOT/%{_libexecdir}/shifter/shifter_slurm_dws_support
%endif

%package  runtime
Summary:  runtime component for shifter (formerly udiRoot)
Group:    System Environment/Base
%if 0%{?suse_version}
BuildRequires: munge
BuildRequires: libcurl-devel
BuildRequires: libjson-c-devel
BuildRequires: pam-devel
%else
BuildRequires: munge-devel
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: libtool autoconf automake
BuildRequires: libcurl libcurl-devel
BuildRequires: json-c json-c-devel
BuildRequires: pam-devel
%endif
%description runtime
runtime and user interface components of shifter
%files runtime
%attr(4755, root, root) %{_bindir}/shifter
%{_bindir}/shifterimg
%{_sbindir}/setupRoot
%{_sbindir}/unsetupRoot
%{_libexecdir}/shifter/mount
%{_libexecdir}/shifter/opt
%{_sysconfdir}/udiRoot.conf.example

%if 0%{?with_slurm:1}
%package slurm
Summary:  slurm spank module for shifter
BuildRequires: slurm-devel
%description slurm
spank module for integrating shifter into slurm
%files slurm
%{_libdir}/shifter/shifter_slurm.so
%{_libexecdir}/shifter/shifter_slurm_dws_support
%endif

%package imagegw
Summary: shifter image manager
%description imagegw
image manager
%files imagegw
%{_libdir}/python2.*/site-packages/shifter_imagegw
%{_libexecdir}/shifter/imagecli.py*
%{_libexecdir}/shifter/imagegwapi.py*
%{_datadir}/shifter/requirements.txt
%{_sysconfdir}/imagemanager.json.example
%defattr(-,root,root)



%changelog
* Sun Apr 24 2016 Douglas Jacobsen <dmjacobsen@lbl.gov> - 16.04.0pre1-1
- Initial version of spec file
