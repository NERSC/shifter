# ACLs and Private Registries

## Theory of operation

When dealing with private registries we need to handle both gathering
the users credentials to access the registry and manage access to the
downloaded images.  The general flow for the user is:

    shifterimg login default
    default username: <enter username>
    default password: <enter password>

Once the user has logged in that can pull an image and specify the list
of users who can access the image.

    shifter --user usera,userb,userc pull usera/privaterepo:latest

Once pulled, the image will only be listed for users who have access and
only those users will be able to use the image.

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
