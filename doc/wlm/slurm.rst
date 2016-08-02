Shifter Integration with SLURM
==============================

Shifter offers tight integration with SLURM by way of the SPANK plugin system.
SLURM 15.08.2 or better is recommended for basic functionality, and 16.05.3 or
better with the extern step integration.

Enabling SLURM integration has the following benefits:

* simple user interface for specifying images and volume maps
* scalable startup of parallel applications
* optional sshd started in node-exclusive allocations to enable complex
  workflows (processes attached to custom memory cgroup or extern step job
  container)
* optional Cray-CCM-emulation mode to enable ssh support in the CLE 5.2 DSL
  environment under "native" SLURM
* optional extern step post-prolog image configuration (useful for tie-breaking
  parallel prolog operations)

Integration with SLURM causes the :code:`setupRoot` executable to be run in a per-
node prolog at job allocation time.  Conversely, at job-end a per-node epilog
runs the :code:`unsetupRoot` executable to deconstruct the UDI environment. setupRoot
generates a container image environment in the same Linux namespace as the
slurmd, meaning that the same environment can be re-used over-and-over again
by multiple sruns, as well as allowing multiple tasks to be simultaneoulsy
launched within that container environment.  Use of the setupRoot environment
also restricts the quantity of loop devices consumed by a job to just those
needed to setup the environment (typically one for a basic environment).

Without `setupRoot` to prepare the environment, the shifter executable can
do this, but these are all done in separate, private namespaces which increases
setup time and consumes more loop devices.  If the user specifies `--image` or
`--volume` options for the shifter executable that differ from the job-
submitted values, a new shifter instance will be instantiated.  This enables
the capability of running multiple containers within a single job, at the cost
of increased startup time and perhaps a small amount of overhead.

Integration with SLURM also increases scalability, as well as consistency, by
caching the specific identity of the image to be used at job submission time.
This increases scalability by transmitting the image ID to all nodes, thus
limiting interactions between shifter and the shifter image gateway to when
the job is initially submitted, and allowing all job setup actions to occur
without further interation with the image gateway.  Caching image identity
increases job consistency by ensuring that the version of an image that was on
the system at the time the job was submitted will be used when then job is
allocated an run, even if newer versions are pulled later.

Configuring SLURM Integration
-----------------------------
1. Build shifter with SLURM support `./configure --with-slurm=/slurm/prefix`
   or `rpmbuild -tb shifter-$VERSION.tar.gz --define "with_slurm /slurm/prefix"`
2. Locate shifter_slurm.so, will be in the shifter prefix/lib/shifter/
   (or within lib64) directory.
3. Add `required /path/to/shifter_slurm.so` to your slurm plugstack.conf
4. Modify plugstack.conf with other options as desired (see documentation)
5. Ensure slurm.conf has `PrologFlags=alloc` or `PrologFlags=alloc,contain`
   The `alloc` flag ensures the user-requested image will be configured on all
   nodes at job start (allocation) time.  The `contain` flag is used to setup
   the "extern" step job container.

Shifter/SLURM configuration options
-----------------------------------
Additional configuration options can be added in plugstack.conf after the
shifter_slurm.so path.  These options will control the behavior of the plugin.

* *shifter_config* - by default the configured location for udiRoot.conf wil be
  used, however, if there is a specific version that should be used by the 
  SLURM plugin, adding `shifter_config=/path/to/custom/udiRoot.conf` will
  achieve that.
* *extern_setup* - optionally specify a script/executable to run in the setup
  of the extern step, which occurs after the prolog runs.  This can be used to
  force specific modifications to the WLM-created container image. Any operation
  performed in this script must be very quick as ordering of
  `prolog -> extern_setup -> job start` is only guaranteed on the batch script
  node; all others may possibly have a possible race with the first srun if
  the batch script node runs first (thus this option is not recommended).
  e.g., `extern_setup=/path/to/script`
* *extern_cgroup* - Flag 0/1 (default 0 == off) to put the optional sshd and,
  therefore all of its eventual children, into the extern cgroup.  This is
  recommended if using the sshd and slurm 16.05.3 or newer., e.g.,
  `extern_cgroup=1`
* *memory_cgroup* - Path to where the memory cgroup is mounted. This is
  a recommended setting in all cases. The plugin will create a new cgroup
  tree under the memory cgroup called "shifter", and within that will follow
  the SLURM standard (uid_<uid>/job_<jobid>).  e.g.,
  `memory_cgroup=/sys/fs/cgroup/memory`
* *enable_ccm* - Enable the CCM emulation mode for Cray systems.  Requires
  enable_sshd as well. e.g., `enable_ccm=1`
* *enable_sshd* - Enable the optional sshd that is run within the container.
  This sshd is the statically linked sshd that is built with shifter.  If
  enabled it is run as root within the container, but limits as much as
  possible, interactions with the User Defined Image by using a statically
  linked-in libc (musl) and SSL environment (libressl), and strictly using
  files (/etc/passwd and /etc/group) which are explicitly placed by shifter
  for authorization.  No privileged logins are permitted.

Using Shifter with SLURM
------------------------
Basic Job Submission and Usage
++++++++++++++++++++++++++++++
The shifter_slurm.so plugin will add `--image`, `--volume`, and, if enabled,
`--ccm` options to sbatch, salloc, and srun.  Typical usage is that a user
would submit a job like::

   cluster$ sbatch script.sh

Where script.sh might look like::

   #!/bin/bash
   #SBATCH --image=docker:yourRepo/yourImage:latest
   
   srun shifter mpiExecutable.sh

See the MPI-with-shifter documentation for more information on how to
configure shifter to make MPI "just work" in shifter.

*Remember* ideal performance is realized if the `--image` and ALL `--volume`
options are supplied in the batch submission script.  If these options are
supplied to the shifter command it will cause new shifter instances to be 
generated at runtime, instead of using the prepared `setupRoot` environment.

Non-MPI and serial applications
+++++++++++++++++++++++++++++++
To start a single instance of your threaded or serial application, there are
two formats for the script you can use depending on your needs.  In this case
we don't need access to srun, thus it is possible to directly execute the
script within shifter if that is desirable.

Note that all paths mentioned in `--volume` arguments need to exist prior to
job submission.  The container image will be setup, including the volume mounts
prior to execution of the batch script.

*Option 1*: Explicitly launch applications in the image environment while
keeping logic flow in the external (cluster) environment::

   #!/bin/bash
   #SBATCH --image=docker:yourRepo/yourImage:latest
   #SBATCH --volume=/scratch/sd/you/exp1/data:/input
   #SBATCH --volume=/scratch/sd/you/exp1/results:/output
   #SBATCH -c 64

   ## -c 64 in this example, assuming system has 64 hyperthreads (haswell),
   ## because we want the batch script, and thus all the commands it runs to
   ## get access to all the hardware

   cp /scratch/sd/you/baseData /scratch/sd/you/exp1/data
   export OMP_NUM_THREADS=32
   shifter threadedExecutable /input /output

   ## do other work with /scratch/sd/you/exp1/results, post-processing

*Option 2*: Execute script in shifter container with no direct access to the
external environment.  Easier to write more complex workflows, but the 
container must have everything needed::

   #!/usr/bin/shifter /bin/bash
   #SBATCH --image=docker:yourRepo/yourImage:latest
   #SBATCH --volume=/scratch/sd/you/exp1/data:/input
   #SBATCH --volume=/scratch/sd/you/exp1/results:/output
   #SBATCH -c 64

   export OMP_NUM_THREADS=32
   threadedExecutable /input /output

   python myComplexPostProcessingScript.py /output

Complex Workflows with Multiple Nodes and No MPI, or non-site integrated MPI
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
You can enable the sshd capability by adding the `enable_sshd=1` option in
plugstack.conf on the shifter_slurm.so line.  This will start a specially
constructed sshd on port 204 on each node.  This sshd will only all the user to
login, and only using an ssh key constructed (automatically) for the explicit 
use of shifter.  All the manipulations to change the default ssh port from 22 
to 204 as well as provide the key are automatically injected into the image
container's /etc/ssh/ssh_config file to ease using the sshd.

Once in the container environment the script can discover the other nodes in
the allocation by examining the contents of `/var/hostslist`, which is in a 
PBS_NODES-style format.

This could allow an mpirun/mpiexec built into the image to be used as well by
using the `/var/nodeslist` and an ssh-based launcher.

If the user can access the external environment sshd, one could avoid turning
on the shifter sshd, and just use the standard `scontrol show hostname
$SLURM_NODELIST` to discover the nodes, then do something like: `ssh <hostname>
shifter yourExecutable` to launch the remote process.

Note that the shifter sshd is only enabled if the job allocation has exclusive
access to the nodes.  Shared allocations will not run `setupRoot`, and
therefore not start the sshd.

Using Shifter to emulate the Cray Cluster Compatibility Mode (CCM) in native slurm
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
The CCM (`--ccm`) capability is a special use-case of shifter to automatically
start and allow the user access to the sshd that shifter can start.  This mode
is distinct because it can automatically put the user script/session into the
shifter environment prior to task start.  This is typically avoided to prevent
SLURM from operating with privilege in the user defined environment.  However,
it is permissible in the unique case of CCM, because CCM targets _only_ the
already existing external environment, not a user-specified one.  I.e., CCM
mode makes a shifter container out of the /dsl environment, starts an sshd in 
it, then launches the job in that containerized revision of the DSL
environment.

To enable `--ccm`, you'll need both `enable_ccm=1` and `enable_sshd=1` in
plugstack.conf.  In addition you'll need to set `allowLocalChroot=1` in
udiRoot.conf.  This is because CCM effectively works by doing::

   shifter --image=local:/  # but with setupRoot so the sshd can be setup

Frequently Asked Questions
--------------------------
Why not just start the job in the container environment?
++++++++++++++++++++++++++++++++++++++++++++++++++++++++
This is technically feasible, however we do not enable it by default for a
number of reasons; though there has been much discussion of it in the past and
may be more in the future.  For example `--ccm` does this for the special case
of a locally constructed image `/`.

Why not do it?
1. We would need to chroot into the container in the task_init_privileged hook,
   which carries a great deal of privilege and is executed far too early in the
   job setup process.  A number of privileged operations would happen in the
   user specified environment, and we felt the risk was too high.

2. It is highly useful to have access to the external environment.  This allows
   you to perform sruns to start parallel applications, move data the site may
   not have necessarily exported into the shifter environment, access commands
   or other resources not trivially imported into a generic UDI.

3. We did try to force slurm into `/opt/slurm` of the container to allow srun
   and job submission to work within the container environment, but owing to
   the way SLURM interacts with so many of the local system libraries via
   dynamic linking, there were too many edge cases where direct interaction
   with SLURM from within a generic UDI was not working quite right.  Also
   there may be some security concerns with such an approach.
