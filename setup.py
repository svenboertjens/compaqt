import platform

if platform.architecture()[0] != '64bit':
    exit("Compaqt only works on 64-bit systems")

from setuptools import setup, Extension, find_packages

ext_modules = [
    Extension(
        'compaqt.compaqt',
        sources=[
            'compaqt/compaqt.c',
            
            'compaqt/globals/exceptions.c',
            
            'compaqt/main/serialization.c',
            'compaqt/main/regular.c',
            'compaqt/main/stream.c',
            'compaqt/main/validation.c',
            
            'compaqt/types/usertypes.c',
            'compaqt/types/strdata.c',
            'compaqt/types/cbytes.c',
            'compaqt/types/cstr.c',
            
            'compaqt/settings/allocations.c',
        ],
        include_dirs=[
            'compaqt/',
            'compaqt/main/',
            'compaqt/globals/',
            'compaqt/settings/',
            'compaqt/types/',
        ],
    )
]

setup(
    name="compaqt",
    version="1.1.0",
    
    author="Sven Boertjens",
    author_email="boertjens.sven@gmail.com",
    
    description="A compact serializer aiming for flexibility and performance",
    
    long_description=open('README.md', 'r').read(),
    long_description_content_type="text/markdown",
    
    url="https://github.com/svenboertjens/compaqt",
    
    packages=['compaqt'],
    install_requires=[],
    
    ext_modules=ext_modules,
    include_package_data=True,
    package_data={
        'compaqt': [
            'mail/*.h',
            'globals/*.h',
            'types/*.h',
            'settings/*.h',
            '*.pyi',
        ]
    },
    
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "License :: OSI Approved :: BSD License",
        "Operating System :: OS Independent",
    ],
    python_requires='>=3.7, <=3.13',
    license='BSD-3-Clause',
)
