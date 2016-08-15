Updating Shifter
================

Version 15.12.0 to 16.08.1
--------------------------
udiRoot.conf
============

   * siteFs format changed from space separated to semicolon separated.  Now
     requires that both "to" and "from" locations be specified.  Recommend
     using same in most cases::

        siteFs=/home:/home;\
               /var/opt/cray/spool:/var/opt/cray/spool:rec

     siteFs now also supports recursive bind mounts (as well as a few other
     less common mount options).
