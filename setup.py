import os
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext

force_py_fallback = os.environ.get('COMPAQT_PY_IMPL', '0') == '1'

ext_modules = []

if not force_py_fallback:
    ext_modules.append(
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
        )
    )

class BuildExt(build_ext):
    def run(self):
        try:
            super().run()
        except:
            ext_modules.clear()

setup(
    name="compaqt",
    version="1.0.1",
    
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
    
    cmdclass={
        'build_ext': BuildExt,
    },
)
