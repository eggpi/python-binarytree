from distutils.core import setup, Extension
setup(name = "binarytree", version = "0.1", ext_modules=[Extension("binarytree", ["binarytree.c"])])
