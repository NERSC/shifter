Shifter Frequently Asked Questions
==================================

Note that this document is a work in progress, if you don't see your question
or answer, please contact shifter-hpc@googlegroups.com

Do the Image Manager and Image Worker processes need to run with root privilege?
--------------------------------------------------------------------------------
No.  The image manager doesn't really do any real work on its own, and the
image worker uses only user-space tools to construct images (in the default
configuration).  A trusted machine should be used to run the imagegw to ensure
that images are securely imported for your site.  In particular, the python
and squashfs tools installations should ideally be integrated as part of the
system or be installed in trusted locations.

Can Shifter import Docker images with unicode filenames embedded?
-----------------------------------------------------------------
Yes.  If you are seeing errors similar to the following in the image gateway log
then you'll need to include the provide sitecustomize.py in your python setup
or just point the gateway's PYTHONPATH to include it::

    [2016-08-01 09:41:20,664: ERROR/MainProcess] Task shifter_imagegw.imageworker.dopull[XXXXXXX-omitted-XXXXXXX] raised unexpected: UnicodeDecodeError('ascii', '/path/is/omitted/some\xc3\xa9_unicode', 35, 36, 'ordinal not in range(128)')
    Traceback (most recent call last):
      File "/usr/lib64/python2.6/site-packages/shifter_imagegw/imageworker.py", line 304, in dopull
          if not pull_image(request,updater=us):
            File "/usr/lib64/python2.6/site-packages/shifter_imagegw/imageworker.py", line 161, in pull_image
                dh.extractDockerLayers(expandedpath, dh.get_eldest(), cachedir=cdir)
      File "/usr/lib64/python2.6/site-packages/shifter_imagegw/dockerv2.py", line 524, in extractDockerLayers
          tfp.extractall(path=basePath,members=members)
      File "/usr/lib64/python2.6/tarfile.py", line 2036, in extractall
          self.extract(tarinfo, path)
      File "/usr/lib64/python2.6/tarfile.py", line 2073, in extract
          self._extract_member(tarinfo, os.path.join(path, tarinfo.name))
      File "/usr/lib64/python2.6/posixpath.py", line 70, in join
          path += '/' + b
          UnicodeDecodeError: 'ascii' codec can't decode byte 0xc3 in position 35: ordinal not in range(128)

To fix this, either set PYTHONPATH to include

* /usr/libexec/shifter (RedHat variants)
* /usr/lib/shifter (SLES variants)

In order to get the shifter sitecustomize.py into your PYTHONPATH.

If that isn't appropriate for your site, then you can examine the contents of
the sitecustomize.py and prepare your own that does similar.
