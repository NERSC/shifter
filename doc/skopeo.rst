External Mode
=============

Since the beginning Shifter has used a native python code to pull and unpack
Docker images.  This has some drawbacks such as keeping up to date with changes
in the specs and performance issues unpacking certain large images.  External
mode allows Shifter to use standard external tools from the Open Container community
to perform these actions.

Dependencies
------------

To use external mode you need three external tools: Skopeo, Umoci, and oci-image-tool.

These can be obtained from:

- https://github.com/containers/skopeo
- https://github.com/openSUSE/umoci
- https://github.com/opencontainers/image-tools

Some distributions have packages for some of these tools.  The tools
should be installed and available on the PATH when the Image Gateway
is started.


Configuration
-------------

To configure the gateway to use external mode, simply set `use_external`
to true in the appropriate platform section of the imagemanager.json
file.

For example:

~~~~
...
    "Platforms": {
        "mycluster": {
            "admins": ["root"],
            "use_external": true
        }
    }
...
~~~~

