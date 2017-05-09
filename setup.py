
from setuptools import Extension
from setuptools import setup

extra_compile_args = []
extra_compile_args += ['-msse4.2']
extra_compile_args += ['-std=c++11']
extra_compile_args += ['-O3']
extra_compile_args += ['-w']
#extra_compile_args += ['-DUSE_SPIRIT']
extra_compile_args += ['-I/home/dmw/src/boost_1_64_0']
#extra_compile_args += ['-fprofile-generate', '-lgcov']
# extra_compile_args += ['-UNDEBUG']
extra_compile_args += ['-DNDEBUG']

setup(
    name = 'py-csvninja',
    version = '0.0.1',
    #packages = ['csvninja'],
    classifiers = [],
    #ext_package = 'csvninja',
    ext_modules = [
        Extension(
            name='csvninja',
            sources=['csvninja.cpp'],
            extra_compile_args=extra_compile_args,
        )
    ],
    zip_safe = False,
)
