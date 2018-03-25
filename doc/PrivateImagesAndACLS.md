# Private Registries and ACLs (17.04 or newer)

Private registries are useful for cases where a user has applications or source
code that need to be protected.  This may be because of competitive reasons
(e.g. an application that hasn't been published yet) or the image may have
licensed applications that can't be generally shared.  Shifter needs to
provide a way to fetch these images from a secure repository such as a private
DockerHub repo and also protect it locally so others users can't run the image.
This document explains how to use this feature.

## Usage

The repository must already be configured by the site admin to be supported.
Once it is configured use the shifterimg tool to store your credentials for
accessing the private repo.  You must provide the name of the registry as
configured by the site or use default.

    shifterimg login default
    default username: <enter username>
    default password: <enter password>

This credential will be stored as a file in your home directory similar to the way docker stores the credential.

You can now try pulling the image and include a list of users that should be allowed to use the pulled image.

    shifter --user alice,bob,charlie pull usera/privaterepo:latest

This would allow users alice, bob and charlie to use the image.  ACLs can be changed to re-pulling the image but with a different list of users.  **If the image is pulled with no ACLs specified, then it will be publicly visible.**

## Design and Theory of operation

When dealing with private registries we need to handle both gathering
the users credentials to access the registry and manage access to the
downloaded images.  The general flow for the user is shown above.

## Under the hood

### Credentials:

shifterimg login will save a munge encrypted string username:password.
This will be sent along with the request in the auth header.

imagegw will decrypt this and store it in the session structure.


### Allowed UIDs and GIDs:

shifterimg pull will send a allowed_uids and allowed_gids in the data part of the
POST

imagegw will parse the allowed_uids/gids and convert them to a list of integers.
imagegw will store these lists in the mongo record.
imagegw will write these values as comma, sepearated text string in the meta
        file of the image.

imagegw will overwrite the values if an images is pulled.

## Edge Cases

TODO: Explain some of the various edge cases and how those are
handled.

   * Public images
   * Private Images without ACLs
   * etc
