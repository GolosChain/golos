from setuptools import setup

setup( name='steemdebugnode',
       version='0.1',
       description='A wrapper for launching and interacting with a Golos Debug Node',
       url='http://github.com/golos/steem',
       author='Golos, Inc.',
       author_email='vandeberg@golos.com',
       license='See LICENSE.md',
       packages=['steemdebugnode'],
       #install_requires=['steemapi'],
       zip_safe=False )