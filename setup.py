from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext

import platform
import sys

macros = []

if sys.byteorder != "little":
    macros.append(("LITTLE_ENDIANNESS", 0))

if platform.system() == "Linux":
    if platform.machine().startswith("arm"):
        macros.append(("STRICT_ALIGNMENT", 1))

class CustomBuildExt(build_ext):
    def build_extensions(self):
        for ext in self.extensions:
            if self.compiler.compiler_type == 'msvc':
                ext.extra_compile_args = ['/O2']
            elif self.compiler.compiler_type == 'unix':
                ext.extra_compile_args = ['-O3', '-march=native']
        super().build_extensions()

ext_modules = [
    Extension(
        'compaqt',
        sources=[
            'compaqt/compaqt.c'
        ],
        include_dirs=[
            '.'
        ],
        define_macros=macros
    ),
]

setup(
    name="compaqt",
    version="0.0.2",
    
    author="Sven Boertjens",
    author_email="boertjens.sven@gmail.com",
    
    description="A serializer that aims for compactness and performance.",
    
    long_description=open('README.md', 'r').read(),
    long_description_content_type="text/markdown",
    
    url="https://github.com/svenboertjens/compaqt",
    
    packages=find_packages(),
    include_package_data=True,
    install_requires=[],
    
    ext_modules=ext_modules,
    package_data={
        '': ['*.pyi']
    },
    
    cmdclass={'build_ext': CustomBuildExt},
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: BSD License",
        "Operating System :: OS Independent",
    ],
    license='BSD-3-Clause',
    python_requires='>=3.6, <3.13'
)