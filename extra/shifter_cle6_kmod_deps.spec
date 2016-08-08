#
# spec file for package shifter_cle6_kmod_deps
#
# Copyright (c) 2016 Regents of the University of California
#
#

Name:           shifter_cle6_kmod_deps-%(uname -r)
Version:        1.0
Release:        3
License:        GPL
BuildRequires: kernel-source kernel-syms
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Summary:       kernel mod deps for cle6

%description
xfs, ext4 and deps

%prep

%build
cd /usr/src/linux
if [ -e "arch/x86/configs/cray_ari_c_defconfig" ]; then
  # assuming we are on a chroot environment
  cp arch/x86/configs/cray_ari_c_defconfig .config
else
  # assuming we are on a compute node
  cp /proc/config.gz ./
  gunzip config.gz
  mv config .config
fi
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

%changelog
* Mon Aug 08 2016 Miguel Gila <miguel.gila@cscs.ch> 1.0-3
 - Added if case to support building on chroot environment
* Wed Jul 20 2016 Miguel Gila <miguel.gila@cscs.ch> 1.0-2
 - Fixed Kernel config file location
