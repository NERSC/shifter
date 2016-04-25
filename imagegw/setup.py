#!/usr/bin/env python

from distutils.core import setup

setup(name='shifter-imagegw',
      version='16.04.0pre1',
      description='shifter image manager',
      author='Shane Canon',
      author_email='scanon@lbl.gov',
      url='https://github.com/NERSC/shifter/',
      packages=['shifter-imagegw'],
      package_dir={'shifter-imagegw': 'src'},
)
