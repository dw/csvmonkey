
from setuptools import Extension
from setuptools import setup

setup(
    name = 'py-csvninja',
    version = '0.0.1',
    packages = ['csvninja'],
    classifiers = [],
    ext_package = 'csvninja',
    ext_modules = [
        Extension(
            name='csvninja',
            sources=['csvninja.cpp'],
        )
    ],
    zip_safe = False,
)
