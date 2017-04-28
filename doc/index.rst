.. shifter documentation master file, created by
   sphinx-quickstart on Sun May  1 23:12:17 2016.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

shifter - Environment Containers for HPC
========================================

shifter is a purpose-built tool for adapting concepts from Linux containers to
extreme scale High Performance Computing resources. It allows a user to create
their own software environment, typically with Docker, then run it at a
supercomputing facility.

The core goal of shifter is to increase scientific computing productivity.
This is achieved by:

1. Increasing scientist productivity by simplifying software deployment and
   management; allow your code to be portable!
2. Enabling scientists to share HPC software directly using the Docker
   framework and Dockerhub community of software.
3. Encouraging repeatable and reproducible science with more durable
   software environments.
4. Providing software solutions to improve system utilization by optimizing
   common bottlenecks in software delivery and I/O in-general.
5. Empowering the user - deploy your own software environment

Contents:

.. toctree::
   :maxdepth: 2

   faq
   bestpractices
   mpi/mpich_abi
   install/centos6
   install/cle6
   install/manualinstall
   wlm/slurm



Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`

