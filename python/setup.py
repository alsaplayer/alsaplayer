#!/usr/bin/env python

from distutils.core import setup, Extension

setup(name='alsaplayer',
      version='0.1',
      description='Python extension for alsaplayer',
      author='Austin Bingham',
      author_email='austin.bingham@gmail.com',
      url='http://www.alsaplayer.org',
      ext_modules=[Extension('alsaplayer',
                             ['control.cc'],
                             include_dirs=[],
                             library_dirs=[],
                             libraries=['alsaplayer', 'boost_python'])
                   ]
     )
