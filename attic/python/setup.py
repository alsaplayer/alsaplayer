#!/usr/bin/env python

from distutils.core import setup, Extension

long_description='''
A set of python bindings for the alsaplayer libraries. These are written in
C++ using boost.python and are intended to provide a minimal level of
abstraction over the C libraries. Higher-level abstractions and functionality
can then be written purely in python.
'''

classifiers=[
    'Development Status :: 4 - Beta',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: GNU Public License',
    'Programming Language :: C++'
    ]

setup(name='python_alsaplayer',
      version='0.3.1',
      description='Python extension for alsaplayer',
      author='Austin Bingham',
      author_email='austin.bingham@gmail.com',
      url='http://www.alsaplayer.org',
      license='gpl',
      long_description=long_description,
      classifiers=classifiers,
      ext_modules=[Extension('alsaplayer',
                             ['control.cc'],
                             include_dirs=[],
                             library_dirs=[],
                             libraries=['alsaplayer', 'boost_python'])
                   ]
     )
