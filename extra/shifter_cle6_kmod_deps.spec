#
# spec file for package shifter_cle6_kmod_deps
#
# Copyright (c) 2016 Regents of the University of California
#
#

Name:           shifter_cle6_kmod_deps-%(uname -r)
Version:        1.0
Release:        1
License:        GPL
BuildRequires: kernel-source kernel-syms
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Summary:       kernel mod deps for cle6

%description
xfs, ext4 and deps

%prep

%build
cd /usr/src/linux
cp arch/x86/configs/cray_ari_c_defconfig .config
make oldconfig
make modules_prepare
make M=fs/xfs CONFIG_XFS_FS=m
make M=lib CONFIG_LIBCRC32C=m
make M=fs/ext4 CONFIG_EXT4_FS=m
make M=fs/jbd2 CONFIG_JBD2=m

%install
cd /usr/src/linux
make M=fs/xfs CONFIG_XFS_FS=m modules_install INSTALL_MOD_PATH=%{buildroot}
make M=lib CONFIG_LIBCRC32C=m modules_install INSTALL_MOD_PATH=%{buildroot}
make M=fs/ext4 CONFIG_EXT4_FS=m modules_install INSTALL_MOD_PATH=%{buildroot}
make M=fs/jbd2 CONFIG_JBD2=m modules_install INSTALL_MOD_PATH=%{buildroot}

%post
depmod -a

%postun
depmod -a

%files
%defattr(-,root,root)
/lib/*


