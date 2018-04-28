# Importing Images (beta)

While Shifter is primarily designed to run images built using Docker, it also
supports directly importing pre-prepared images.  Since some sites may not
wish to support this or limit its usage, it requires extra configuration.

## Security Considerations

Before enabling this feature, the administrator should understand the
increased risks of allowing direct imports of images perpared outside
of Shifter.  The Shifter gateway should not be capable
of creating images that contain security attributes.  This helps minimize
the risk of images introducing additional security risks.  If you enable
the importing of images, you should take extra precautions in how images
are prepared and who you trust to import them.  Images should ideally
be prepared as a unprivileged user and the -no-xattrs flag should be
passed to the mksquashfs command to mitigate the risks of security attributes
being included in the image.  When using the workload manager integration 
(e.g. SLURM plugin), it is especially critical that direct 
import users block xattrs since the mounts may be visible
to processes running outside the runtime.

## Approach

Importing images works by copying a pre-prepared image into the shared
location, generating a pseudo-hash for the image, and adding an entry in
the Shifter image list.

## Enabling support

To enable support, add an "ImportUsers" parameters into the top section of
the imagemanager.json file.  This can be a list of user names (in JSON list
format) or the keyword "all".  For example:

~~~~
...
"ExpandDirectory": "/tmp/images/expand/",
"ImportUsers":"all"
"Locations": {
...
~~~~

The fasthash script needs to be installed as `fasthash` in a location on
the search path for the image gateway for local mode or in the search path
on the remote system for remote mode.  This script generates a pseudo-hash
for the image based on the contents of the image.


## Usage

The user issuing the import most have the squashed image in a location that is
accessible by the shifter user (e.g. the account used to run the gateway).

The command line tools do not currently support import.  So a curl command
must be issued.  Here is an example of an import command to import the squashfs image
located at /tmp/image.squashfs and call it
load_test:v1 in Shifter.

~~~~bash
curl -d '{"filepath":"/tmp/myimage.squashfs","format":"squashfs"}' \
  -H "authentication: $(munge -n)" \
  http://<imagegw>:5000/api/doimport/mycluster/custom/load_test:v1/
~~~~

Once an image is imported, it is run with Shifter like other images.
The only difference is the type is "custom" instead of the default "docker".

```bash
shifter --image=custom:load_test:v1 /bin/app
```
