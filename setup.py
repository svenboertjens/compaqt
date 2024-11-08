from setuptools import setup, Extension

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
            'compaqt/py_fallback/',
        ],
    ),
]

setup(
    name="compaqt",
    version="1.0.0",
    
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
            'py_fallback/*.py'
        ]
    },
    include_package_data=True,
    
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
    license='BSD-3-Clause'
)