%{!?_shifter_sysconfdir: %{expand:%%global _shifter_sysconfdir %{_sysconfdir}/shifter}}
%define _sysconfdir %_shifter_sysconfdir

%if 0%{!?_without_shifter_user:1}
%{expand:%%global shifter_user %{?shifter_user}%{!?shifter_user:shifter}}
%{expand:%%global shifter_group %{?shifter_group}%{!?shifter_group:%{shifter_user}}}
%{expand:%%global shifter_uid %{?shifter_uid}%{!?shifter_uid:50}}
%{expand:%%global shifter_gid %{?shifter_gid}%{!?shifter_gid:%{shifter_uid}}}
%endif

%if 0%{!?_without_systemd:1}
%{!?_unitdir:          %global _without_systemd --without-systemd}
%{!?systemd_requires:  %global _without_systemd --without-systemd}
%endif

%{?_with_slurm: %global with_slurm %{_prefix}}

Summary:   NERSC Shifter -- Containers for HPC
Name:      shifter
Version:   16.08.5
Release:   1.nersc%{?dist}
License:   BSD (LBNL-modified)
Group:     System Environment/Base
URL:       https://github.com/NERSC/shifter
Packager:  Douglas Jacobsen <dmjacobsen@lbl.gov>
Source0:   %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Shifter enables container images for HPC. In a nutshell, Shifter
allows an HPC system to efficiently and safely permit end-users to run
jobs inside a Docker image. Shifter consists of a few moving parts:
  1) a utility that typically runs on the compute node that creates the
     runtime environment for the application,
  2) an image gateway service that pulls images from a registry and
     repacks them in a format suitable for the HPC system (typically
     squashfs), and
  3) example scripts/plugins to integrate Shifter with various batch
     scheduler systems.

This package contains no files; to install the core components of
Shifter, please install the "shifter-runtime" package.


%package  runtime
Summary:  Runtime component(s) for NERSC Shifter (formerly udiRoot)
Group:    System Environment/Base
%if 0%{?suse_version}
BuildRequires: munge, libcurl-devel, libjson-c-devel, pam-devel, libcap-devel
%else
BuildRequires: munge-devel, gcc, gcc-c++
BuildRequires: libtool autoconf automake
BuildRequires: libcurl libcurl-devel
BuildRequires: json-c json-c-devel
BuildRequires: pam-devel
BuildRequires: libcap-devel
%endif

%description runtime
Shifter enables container images for HPC. In a nutshell, Shifter
allows an HPC system to efficiently and safely permit end-users to run
jobs inside a Docker image. Shifter consists of a few moving parts:
  1) a utility that typically runs on the compute node that creates the
     runtime environment for the application,
  2) an image gateway service that pulls images from a registry and
     repacks them in a format suitable for the HPC system (typically
     squashfs), and
  3) example scripts/plugins to integrate Shifter with various batch
     scheduler systems.

This package contains the runtime and user interface components of
Shifter.


%package imagegw
Summary: Image Manager/Gateway for Shifter
Requires(pre): shadow-utils
%if 0%{!?_without_systemd:1}
%{systemd_requires}
%endif
%if 0%{?rhel}
Requires: squashfs-tools python-pip python-flask python-pymongo python-redis python-gunicorn munge
%endif

%description imagegw
Shifter enables container images for HPC. In a nutshell, Shifter
allows an HPC system to efficiently and safely permit end-users to run
jobs inside a Docker image. Shifter consists of a few moving parts:
  1) a utility that typically runs on the compute node that creates the
     runtime environment for the application,
  2) an image gateway service that pulls images from a registry and
     repacks them in a format suitable for the HPC system (typically
     squashfs), and
  3) example scripts/plugins to integrate Shifter with various batch
     scheduler systems.

This package contains the Image Gateway and image management tools for
use with Shifter.


%if 0%{?with_slurm:1}
%package slurm
Summary:  SLURM Spank Module for Shifter
BuildRequires: slurm-devel

%description slurm
Shifter enables container images for HPC. In a nutshell, Shifter
allows an HPC system to efficiently and safely permit end-users to run
jobs inside a docker image. Shifter consists of a few moving parts:
  1) a utility that typically runs on the compute node that creates the
     runtime environment for the application,
  2) an image gateway service that pulls images from a registry and
     repacks them in a format suitable for the HPC system (typically
     squashfs), and
  3) example scripts/plugins to integrate Shifter with various batch
     scheduler systems.

This package contains the Spank Plugin module which allows for the
integration of Shifter with the SLURM Workload Manager.
%endif


%prep
%setup -q
test -x configure || ./autogen.sh


%build
## build udiRoot (runtime) first
%configure \
    %{?with_slurm:--with-slurm=%{with_slurm}} %{?acflags}

MAKEFLAGS=%{?_smp_mflags} %{__make}


%install
%make_install

# Create directory for Celery/ImageGW API logs
%{__mkdir_p} $RPM_BUILD_ROOT%{_localstatedir}/log/%{name}_imagegw{,_worker}

: > $RPM_BUILD_ROOT/%{_sysconfdir}/shifter_etc_files/passwd
: > $RPM_BUILD_ROOT/%{_sysconfdir}/shifter_etc_files/group
%if %{?with_slurm:1}0
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.a
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.la
%else
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.a
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.la
rm -f $RPM_BUILD_ROOT/%{_libdir}/shifter/shifter_slurm.so
rm -f $RPM_BUILD_ROOT/%{_libexecdir}/shifter/shifter_slurm_dws_support
%endif

%if 0%{!?_without_systemd:1}
%{__mkdir_p} $RPM_BUILD_ROOT%{_unitdir}
%{__install} -m 0644 extra/systemd/shifter_imagegw.service $RPM_BUILD_ROOT%{_unitdir}/
%{__install} -m 0644 extra/systemd/shifter_imagegw_worker@.service $RPM_BUILD_ROOT%{_unitdir}/
%{__install} -m 0644 extra/systemd/shifter_imagegw_worker.target $RPM_BUILD_ROOT%{_unitdir}/
%endif


%check
%{__make} check


%if 0%{!?_without_shifter_user:1}
%pre imagegw
getent group %{shifter_group} >/dev/null || groupadd -f -g %{shifter_gid} -r %{shifter_group}
if ! getent passwd %{shifter_user} >/dev/null ; then
    if ! getent passwd %{shifter_uid} >/dev/null ; then
        useradd -r -u %{shifter_uid} -g %{shifter_group} -d %{_sysconfdir} -s /sbin/nologin -c "Shifter User" %{shifter_user}
    else
        useradd -r -g %{shifter_group} -d %{_sysconfdir} -s /sbin/nologin -c "Shifter User" %{shifter_user}
    fi
fi
exit 0
%endif

%post runtime
getent passwd > %{_sysconfdir}/shifter_etc_files/passwd
getent group > %{_sysconfdir}/shifter_etc_files/group

%post imagegw
%if 0%{?rhel}
pip install celery
%endif

%files
%defattr(-, root, root)
%doc AUTHORS Dockerfile LICENSE NEWS README* doc extra/cle6

%files runtime
%defattr(-, root, root)
%doc AUTHORS LICENSE NEWS README* udiRoot.conf.example
%attr(4755, root, root) %{_bindir}/shifter
%config(noreplace missingok) %verify(not filedigest mtime size) %{_sysconfdir}/shifter_etc_files/passwd
%config(noreplace missingok) %verify(not filedigest mtime size) %{_sysconfdir}/shifter_etc_files/group
%config(noreplace) %{_sysconfdir}/shifter_etc_files/nsswitch.conf
%{_bindir}/shifterimg
%{_sbindir}/setupRoot
%{_sbindir}/unsetupRoot
%{_libexecdir}/shifter/mount
%{_libexecdir}/shifter/opt
%{_sysconfdir}/udiRoot.conf.example

%files imagegw
%defattr(-, root, root)
%doc AUTHORS LICENSE NEWS README* contrib extra/systemd
%attr(0770, %{shifter_user}, %{shifter_group}) %dir %{_localstatedir}/log/%{name}_imagegw/
%attr(0770, %{shifter_user}, %{shifter_group}) %dir %{_localstatedir}/log/%{name}_imagegw_worker/
%{_libdir}/python2.*/site-packages/shifter_imagegw
%{_libexecdir}/shifter/imagecli.py*
%{_libexecdir}/shifter/imagegwapi.py*
%{_libexecdir}/shifter/sitecustomize.py*
%{_datadir}/shifter/requirements.txt
%{_sysconfdir}/imagemanager.json.example
%if 0%{!?_without_systemd:1}
%{_unitdir}/shifter_imagegw*
%endif

%if 0%{?with_slurm:1}
%files slurm
%defattr(-, root, root)
%doc AUTHORS LICENSE NEWS README*
%{_libdir}/shifter/shifter_slurm.so
%{_libexecdir}/shifter/shifter_slurm_dws_support
%endif


%changelog
* Sun Apr 24 2016 Douglas Jacobsen <dmjacobsen@lbl.gov> - 16.04.0pre1-1
- Initial version of spec file
