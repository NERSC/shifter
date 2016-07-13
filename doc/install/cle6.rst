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

Configuring udiRoot with custom ansible play
--------------------------------------------
1. Refer to the sample ansible play/role in the shifter distribution under 
   extra/cle6/ansible
2. Make modifications to each of the following to meet your needs
   - vars/main.yaml
   - templates/udiRoot.conf.j2
   - files/premount.sh
   - files/postmount.sh
3. Ensure the tasks/main.yaml is appropriate for your site


Configuration considerations for DataWarp
+++++++++++++++++++++++++++++++++++++++++
For DataWarp mount points to be available in shifter containers, you'll need
to ensure that all mounts under `/var/opt/cray/dws` are imported into the 
container.  Unfortunately, these mounts can sometimes come in after the 
container is setup.  Also, it is hard to predict the exact mount point,
thus, we use two mount flags:
- rec, enables a recursive mount of the path to pull in all mounts below it
- slave, propagates any changes from the external environment into the container
         slaved bind mounts are not functional in CLE5.2, but work in CLE6

Add the following to the udiRoot.conf siteFs:
 /var/opt/cray/dws:/var/opt/cray/dws:rec:slave

Or, to the "compute" section of the "shifterSiteFsByType" variable in shifter
ansible role.

Configuration considerations for MPI
++++++++++++++++++++++++++++++++++++
