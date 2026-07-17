"""
This module characterizes the Kernel class (inherited from Kernel User class).

It contains functions and attributes to define a kernel and its execution algorithm

"""

from common import KernelUser


class Kernel(KernelUser):

    def __init__(self, libName, libRootDir, kernelName):

        super().__init__(libName, libRootDir, kernelName)

    # Kernel execution algorithm to be implemented by derived classes
    def exec(self):

        pass
