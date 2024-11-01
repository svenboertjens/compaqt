from setuptools import setup, Extension

import sys
import os

macros = []

if sys.byteorder != "little" and os.environ.get("SET_LITTLE_ENDIAN") != '1':
    macros.append(("IS_LITTLE_ENDIAN", 0)) # 1 by default

ext_modules = [
    Extension(
        'compaqt.compaqt',
        sources=[
            'compaqt/compaqt.c',
            'compaqt/metadata.c',
            'compaqt/exceptions.c',
            
            'compaqt/main/serialization.c',
            'compaqt/main/regular.c',
            'compaqt/main/stream.c',
            'compaqt/main/validation.c',
            
            'compaqt/types/custom.c',
            
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
    version="0.5.1",
    
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
            'types/*.h',
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