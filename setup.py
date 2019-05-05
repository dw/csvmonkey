
from __future__ import print_function
import os
import sys

from setuptools import Extension
from setuptools import setup

sys.path.insert(0,
    os.path.join(
        os.path.dirname(__file__),
        'third_party',
    )
)

import cpuid


def has_sse42():
    cpu = cpuid.CPUID()
    regs = cpu(1)
    return bool((1 << 20) & regs[2])


extra_compile_args = []
extra_compile_args += ['-std=c++11']
extra_compile_args += ['-Iinclude']
extra_compile_args += ['-O3']
extra_compile_args += ['-w']

# cc1plus: warning: command line option '-Wstrict-prototypes' is valid for
# C/ObjC but not for C++
extra_compile_args += ['-Wno-strict-prototypes']

#extra_compile_args += ['-DUSE_SPIRIT']
#extra_compile_args += ['-I/home/dmw/src/boost_1_64_0']
#extra_compile_args += ['-fprofile-generate', '-lgcov']
#extra_compile_args += ['-DCSVMONKEY_DEBUG']


if has_sse42():
    extra_compile_args += ['-msse4.2']
else:
    print("Warning: CPU lacks SSE4.2, compiling with fallback",
          file=sys.stderr)


setup(
    name='csvmonkey',
    author='David Wilson',
    author_email='dw+csvmonkey@botanicus.net',
    version='0.0.5',
    classifiers=[],
    url='https://github.com/dw/csvmonkey/',
    ext_modules = [
        Extension(
            name='csvmonkey',
            sources=['cpython/csvmonkey.cpp'],
            undef_macros=['NDEBUG'],
            extra_compile_args=extra_compile_args,
        )
    ],
    zip_safe = False,
)
