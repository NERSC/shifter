Authentication to the image gateway
===================================

The image gateway requires authentication for all interactions.  The purpose 
of the authentication in most cases to ensure the request came from the target
platform (e.g., mycluster), and from a known user id on that system.  To that
end, the HTTP header must include an "authentication" field.

The value of that field must be a munge-encoded message signed with the same
munge credential as is configured for the target platform on the imagegw 
server system.  For basic requests, it may be sufficient to simply munge encode
an empty message (e.g., "").

The image gateway may, from time to time perform privileged operations as a
result of the API calls (e.g., pull a privileged image from DockerHub).  In
that case the originating request must include a more extensive
"authentication" header which includes the munge encrypted credentials
(username and password) for the target platform (at least).

Format of the "authentication" HTTP header
------------------------------------------
* _Non-privileged requests:_ munge encoded empty string
* _Privileged requests:_ munge encoded JSON document including one or more credentials for the remote image resources

JSON document format:
*********************

.. code-block:: json

   {
       "authorized_locations": {
           "default":"username:password",
           "otherloc":"username:password"
       }
   }

Specifications for authorized locations
---------------------------------------
The credentials provided to the image gateway are for the remote locations.
The identifier in the "authorized_locations" hash must match the locations in
the image_manager.json configuration.  The only exception to this is that it
may be difficult for the client to determine which location a particular 
request may be routed to, owing to that, and the fact that the client can not
know what the "default" remote location for the image manager is, a special
"default" location may be specified which the image manager can use for 
interacting with the default location (if the default loation is selected) so
long as a credential for a more specific name for the default loation is not
provided.

e.g., if the "default" location for the imagegw is registry-1.docker.io, and
a credential is provided by the client for "registry-1.docker.io", then that
credential must be used by the imagegw when interacting with
registry-1.docker.io; if, however, only a "default" credential is provided,
and "registry-1.docker.io" happens to be the default location, then the
"default" credential may be used when interacting with registry-1.docker.io.

The client may provide just the needed credential or many different credentials
and the image manager is responsible to parse the data structure and determine
which credential is most appropriate given the request.  This is allowed
because it may be challenging for the client to determine which location will
be selected by the imagegw, since the configuration of the imagegw may not
be accessible by the client.
