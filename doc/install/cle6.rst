Installing Shifter Runtime in Cray's CLE 6.0UP01
================================================

Setting up Compute node kernel support
--------------------------------------
1. Build compute node image
2. Boot compute node
3. Install appropriate kernel-sources and kernel-syms
e.g.,

    zypper install --oldpackage kernel-source-3.12.51-52.31.1_1.0600.9146.67.1
    zypper install --oldpackage kernel-syms.12.51-52.31.1_1.0600.9146.67.1

4. rpmbuild -bb /path/to/shifter_cle6_kmod_deps.spec
6. Put resulting RPM in a repo, pkgcoll, and update compute image
7. Add ansible play to modprobe ext4 and xfs upon boot.
