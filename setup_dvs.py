from setuptools import setup
import glob
from torch.utils.cpp_extension import BuildExtension, CppExtension

setup(
    name="decodeDVS",
    ext_modules=[CppExtension("decodeDVS", ["decode_davis.cpp", "convert.cpp"], libraries=["lz4", "caer"]),],
    cmdclass={"build_ext": BuildExtension},
)