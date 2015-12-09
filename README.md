# Shifter - Containers for HPC

Shifter enables container images for HPC.  In a nutshell, Shifter allows an HPC system to efficiently and safely allow end-users 
to run a docker image.  Shifter consists of a few moving parts 1) a utility that typically runs on the compute node that creates
the run time environment for the application 2) an image gateway service that pulls images from a registry and repacks it in a 
format suitable for the HPC system (typically squashfs) 3) and examples scripts to integrate Shifter with various batch scheduler
systems.


# Installation

We will add more detailed installation instructions in the future.

# Mailing List

For updates and community support, please subscribe to the email list at https://groups.google.com/forum/#!forum/shifter-hpc.

# Change Log

See NEWS for a history of CHANGES

# Website

https://www.nersc.gov/research-and-development/user-defined-images/
