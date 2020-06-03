================================================
Installing Shifter Runtime in Cray's CLE 6.0UP01
================================================

--------------------------------------
Setting up Compute node kernel support
--------------------------------------

1. Build compute node image.
2. Boot compute node.
3. Install appropriate ``kernel-source`` and ``kernel-syms``. For example:

   .. code-block:: bash

       zypper install --oldpackage kernel-source-3.12.51-52.31.1_1.0600.9146.67.1
       zypper install --oldpackage kernel-syms.12.51-52.31.1_1.0600.9146.67.1

4. :code:`rpmbuild -bb /path/to/shifter_cle6_kmod_deps.spec`
5. Put resulting RPM in a repo, pkgcoll, and update compute image.


--------------------------------------------
Configuring udiRoot with custom ansible play
--------------------------------------------

1. Refer to the sample ansible play/role in the shifter distribution under 
   ``extra/cle6/ansible``.
2. Make modifications to each of the following files to meet your needs

   * ``vars/main.yaml``
   * ``templates/udiRoot.conf.j2``
   * ``files/premount.sh``
   * ``files/postmount.sh``
   * ``vars/cluster.yaml``

3. Ensure the tasks/main.yaml is appropriate for your site


+++++++++++++++++++++++++++++++++++++++++
Configuration considerations for DataWarp
+++++++++++++++++++++++++++++++++++++++++

For DataWarp mount points to be available in shifter containers, you'll need
to ensure that all mounts under ``/var/opt/cray/dws`` are imported into the 
container.  Unfortunately, these mounts can sometimes come in after the 
container is setup.  Also, it is hard to predict the exact mount point,
thus, we use two mount flags:

* ``rec``, enables a recursive mount of the path to pull in all mounts below it
* ``slave``, propagates any changes from the external environment into the container
         slaved bind mounts are not functional in CLE5.2, but work in CLE6

Add the following to ``siteFs`` in your ``udiRoot.conf``::

    /var/opt/cray/dws:/var/opt/cray/dws:rec:slave

Or, to the "compute" section of the ``shifterSiteFsByType`` variable in shifter
ansible role.


++++++++++++++++++++++++++++++++++++
Configuration considerations for MPI
++++++++++++++++++++++++++++++++++++

To enable Cray-native MPI in shifter containers, there are a few needed changes
to the container environment.

1. Need to patch ``/var/opt/cray/alps`` and ``/etc/opt/cray/wlm_detect`` into the 
   container environment. ``/etc/opt/cray/wlm_detect`` is a NEW requirement in CLE6
2. To enable override of ``libmpi.so`` for client containers, run
   ``extra/prep_cray_mpi_libs.py`` from the shifter distribution (also installed in
   the ``%libexec%`` path if shifter-runtime RPM is used).

   .. code-block:: bash

       mkdir -p /tmp/cray
       cd /path/to/shifter/extra
       python ./prep_cray_mpi_libs.py /tmp/cray

   You can then either copy ``/tmp/cray/mpich-<version>`` to
   ``/usr/lib/shifter/opt/mpich-<version>`` or integrate the RPM in
   ``/tmp/cray/RPMS/x86_64/shifter_cray_mpich_<version>....rpm`` into your
   compute and login images.
3. Setup a shifter module to setup cray mpich by adding the following to
   your ``udiRoot.conf``::

       module_mpich_copyPath = /usr/lib/shifter/opt/mpich-<version>
       module_mpich_siteEnvPrepend = LD_LIBRARY_PATH=/opt/udiImage/modules/mpich/lib64
       module_mpich_siteEnv = SHIFTER_MODULE_MPICH=1

4. If you wish the mpich module to load by default on every shifter invocation,
   then add the following to your udiRoot.conf::

       defaultModules = mpich

``prep_cray_mpi_libs.py`` copies the shared libraries from the CrayPE ``cray-mpich``
and ``cray-mpich-abi`` modules (for both ``PrgEnv-gnu`` and ``PrgEnv-intel``), into the
target path.  It then recursively identifies dependencies for those shared
libraries and copies those to target/dep.  Finally it rewrites the ``RPATH`` entry
for all the shared libraries to ``/opt/udiImage/cray/lib64/dep``; this allows the
target libraries to exist in ``/opt/udiImage/cray/lib64``, and ONLY have that path
in ``LD_LIBRARY_PATH``, minimizing search time.  Also, since none of the dependency
libraries are copies to ``/opt/udiImage/cray/lib64``, but to
``/opt/udiImage/cray/lib64/dep``, and those are accessed via the modified ``RPATH``,
there is minimal bleedthrough of the dependency libraries into the container
environment.
``prep_cray_mpi_libs.py`` requires `patchelf`_.

.. _patchelf: https://github.com/NixOS/patchelf
