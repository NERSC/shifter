shifter
=======

Synopsis
--------
*shifter* [options] _command_ [command options]

Description
-----------
*shifter* generates or attaches to an existing shifter container environment
and launches a process within that container environment.  This is done with
minimal overhead to ensure that container creation and process execution are
done as quickly as possible in support of High Performance Computing needs.

Options
-------
--image       Image Selection Specification
-V|--volume   Volume bind mount
-h|--help     This help text
-v|--verbose  Increased logging output

Image Selection
---------------
*shifter* identifies the desired image by examining its environment and command
line options.  In order of precedence, shifter selects image by looking at the
following sources:
   - SHIFTER environment variable containing both image type and image speicifier
   - SHIFTER_IMAGE and SHIFTER_IMAGETYPE environment variables
   - SLURM_SPANK_SHIFTER_IMAGE and SLURM_SPANK_SHIFTER_IMAGETYPE environment variables
   - --image command line option

Thus, the batch system can set effective defaults for image selection by manipulating
the job environemnt, however, the user can always override by specifying the --image
command line argument.

The format of --image or the SHIFTER environment variable are the same:
   imageType:imageSpecifier

where imageType is typically "docker" but could be other, site-defined types.
imageSpecifier is somewhat dependent on the imageType, however, for docker it
