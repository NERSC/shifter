.. _shifter-command:

``shifter`` command
===================

Synopsis
--------
*shifter* [options] *command* [command options]

Description
-----------
``shifter`` command generates or attaches to an existing Shifter container environment
and launches a process within that container environment.  This is done with
minimal overhead to ensure that container creation and process execution are
done as quickly as possible in support of High Performance Computing needs.

Options
-------

``-i`` \| ``--image``
   Image selection specification
``-V`` \| ``--volume``
   Volume bind mount
``-h`` \| ``--help``
   This help text
``-v`` \| ``--verbose``
   Increased logging output

Image Selection
---------------
Shifter identifies the desired image by examining its environment and command
line options.  In order of precedence, shifter selects image by looking at the
following sources:
   - ``SHIFTER`` environment variable containing both image type and image speicifier
   - ``SHIFTER_IMAGE`` and ``SHIFTER_IMAGETYPE`` environment variables
   - ``SLURM_SPANK_SHIFTER_IMAGE`` and ``SLURM_SPANK_SHIFTER_IMAGETYPE`` environment variables
   - ``--image`` command line option

Thus, the batch system can set effective defaults for image selection by manipulating
the job environemnt, however, the user can always override by specifying the ``--image``
command line argument.

The format of ``--image`` or the ``SHIFTER`` environment variable are the same::

   imageType:imageSpecifier

where ``imageType`` is typically ``docker`` but could be other, site-defined types.
``imageSpecifier`` is somewhat dependent on the ``imageType``, however, for ``docker``, the
image gateway typically assigns the sha256 hash of the image manifest to be
the specifier.

Shifter will attempt to see if the global environment already has a Shifter
image configured matching the users arguments.  If a compatible image is already
setup on the system the existing environment will be used to launch the 
requested process.  If not, Shifter will generate a new mount namespace, and
setup a new shifter environment.  This ensures that multiple Shifter instances
can be used simultaneously on the same node.  Note that each Shifter instance
will consume at least one loop device, thus it is recommended that sites allow
for at least two available loop devices per Shifter instance that might be
reasonably started on a compute node.  At NERSC, we allow up to 128 loop
devices per compute node.

User-Specified Volume Mounts
----------------------------

.. todo:: Add documendation for user-specified volume mounts.
