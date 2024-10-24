from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext

import platform
import sys
import os

macros = []

if sys.byteorder != "little" and not os.environ.get("SET_LITTLE_ENDIAN"):
    macros.append(("IS_LITTLE_ENDIAN", 0))

if os.environ.get("SET_STRICT_ALIGNMENT") or platform.machine().strip().strip('_').lower().startswith("arm"):
    macros.append(("STRICT_ALIGNMENT", 1))

ext_modules = [
    Extension(
        'compaqt.compaqt',
        sources=[
            'compaqt/compaqt.c',
            'compaqt/metadata.c',
            
            'compaqt/main/serialization.c',
            'compaqt/main/regular.c',
            'compaqt/main/stream.c',
            'compaqt/main/validation.c',
            
            'compaqt/settings/allocations.c',
        ],
        include_dirs=[
            'compaqt/',
            'compaqt/main/',
            'compaqt/settings/',
        ],
        define_macros=macros
    ),
]

setup(
    name="compaqt",
    version="0.4.5",
    
    author="Sven Boertjens",
    author_email="boertjens.sven@gmail.com",
    
    description="A compact serializer aiming for flexibility and performance",
    
    long_description=open('README.md', 'r').read(),
    long_description_content_type="text/markdown",
    
    url="https://github.com/svenboertjens/compaqt",
    
    packages=['compaqt'],
    install_requires=[],
    
    ext_modules=ext_modules,
    package_data={
        'compaqt': [
            '*.pyi',
            '*.h',
            'main/*.h',
            'settings/*.h',
        ]
    },
    include_package_data=True,
    
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: BSD License",
        "Operating System :: OS Independent",
    ],
    license='BSD-3-Clause'
)