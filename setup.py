#!/usr/bin/env python

from distutils.core import setup

setup(name='shifter-imagegw',
      version='16.04.0pre1',
      description='shifter image manager',
      author='Shane Canon',
      author_email='scanon@lbl.gov',
      url='https://github.com/NERSC/shifter/',
      packages=['shifter_imagegw'],
      package_dir={'shifter_imagegw': 'src/shifter_imagegw'},
      data_files=[('libexec/shifter_imagegw', ['src/imagegwapi.py','src/imagecli.py'])],
)
