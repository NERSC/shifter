# Shifter - Containers for HPC

Shifter enables container images for HPC.  In a nutshell, Shifter allows an HPC system to efficiently and safely allow end-users 
to run a docker image.  Shifter consists of a few moving parts 1) a utility that typically runs on the compute node that creates
the run time environment for the application 2) an image gateway service that pulls images from a registry and repacks it in a 
format suitable for the HPC system (typically squashfs) 3) and example scripts/plugins to integrate Shifter with various batch
scheduler systems.


# Mailing List

For updates and community support, please subscribe to the email list at https://groups.google.com/forum/#!forum/shifter-hpc.

# Release Cycle

Shifter should be considered to be pre-release. Milestone tags will be labelled like YYYY.MM.vv (i.e. 2015.12.00) where vv is a minor release incremented with each snapshot generated.  We plan to release new major versions biannually with point releases as required.

# Contributing

If you want to contribute code or documentation, please join our slack chat channel at https://shifter-hpc.slack.com to
coordinate efforts.  Code can be presented for inclusion with the shifter project by providing Pull Requests against the
shifter master branch on github from your own forked repo.

# Change Log

See NEWS for a history of CHANGES

# Website

https://www.nersc.gov/research-and-development/user-defined-images/

# Documentation

https://github.com/NERSC/shifter/tree/master/doc
