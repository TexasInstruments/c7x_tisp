"""
This module characterizes the user class of a kernel.

It contains functions and attributes to perform high-level tasks and infrastructure management necessary for python-based xlib kernel development.

"""

import os
import shutil
from common import KernelFileIO


class KernelUser(KernelFileIO):

    # constructor
    def __init__(self, libName, libRootDir, kernelName):

        super().__init__(libName, libRootDir, kernelName)

    # buildup each of the kernel subdirectories
    def kernel_buildup(
        self, licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
    ):
        """
        Builds up the kernel subdirectories.

        Args:
            licenseFileName (str): The path to the license file.
            kernelPrivateHeaderYaml (str): The path to the kernel private header YAML file.
            kernelHeaderYaml (str): The path to the kernel header YAML file.
            testCaseCsv (str): The path to the test case csv file.

        Returns:
            None
        """

        self.kernel_source_buildup(
            licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
        )

        self.kernel_example_buildup(
            licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
        )

        self.kernel_test_buildup(
            licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
        )

    # create the source, test, and example directories for the kernel
    def kernel_create(self):
        """
        Creates the necessary subdirectories for the kernel.

        This function creates the src, test, and examples directories for the kernel.
        It checks if the directories already exist and only creates them if they do not.

        Parameters:
            None

        Returns:
            None
        """

        # define srcDir
        srcDir = self.libRootDir + "/src/" + self.kernelDirName

        print(srcDir)
        # only create if it doesn't exist
        if not os.path.exists(srcDir):
            os.mkdir(srcDir)

        print("Created src directory.\n")

        # define testDir
        testDir = self.libRootDir + "/test/" + self.kernelDirName
        testIdatDir = (
            self.libRootDir
            + "/test/"
            + self.libName.upper()
            + "_idat_gen/"
            + self.kernelDirName
        )

        # only create if it doesn't exist
        if not os.path.exists(testDir):
            os.mkdir(testDir)
        if not os.path.exists(testIdatDir):
            os.mkdir(testIdatDir)

        print("Created test directory.\n")

        # define exampleDir
        exampleDir = self.libRootDir + "/examples/" + self.kernelDirName

        # only create if it doesn't exist
        if not os.path.exists(exampleDir):
            os.mkdir(exampleDir)

        print("Created examples directory.\n")

    # tear down the infrastructure of kernel
    def kernel_teardown(self):
        """
        Remove the kernel subdirectories.

        This function removes the src, test, and examples directories for the kernel.
        It checks if the directories exist and only removes them if they do.

        Parameters:
            None

        Returns:
            None
        """

        # define srcDir
        srcDir = self.libRootDir + "/src/" + self.kernelDirName

        # only remove if it exists
        if os.path.exists(srcDir):
            shutil.rmtree(srcDir)

        print("Removed src directory.\n")

        # define testDir
        testDir = self.libRootDir + "/test/" + self.kernelDirName
        testIdatDir = (
            self.libRootDir
            + "/test/"
            + self.libName.upper()
            + "_idat_gen/"
            + self.kernelDirName
        )

        # only remove if it exists
        if os.path.exists(testDir):
            shutil.rmtree(testDir)
        if os.path.exists(testIdatDir):
            shutil.rmtree(testIdatDir)

        print("Removed test directory.\n")

        # define exampleDir
        exampleDir = self.libRootDir + "/examples/" + self.kernelDirName

        # only remove if it exists
        if os.path.exists(exampleDir):
            shutil.rmtree(exampleDir)

        print("Removed examples directory.\n")

    # write source directory
    def kernel_source_buildup(
        self, licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
    ):
        """
        Builds up the kernel source files.

        Args:
            licenseFileName (str): The path to the license file.
            kernelPrivateHeaderYaml (str): The path to the kernel private header YAML file.
            kernelHeaderYaml (str): The path to the kernel header YAML file.

        Returns:
            None
        """

        # write header file
        self.write_kernel_header(kernelHeaderYaml, licenseFileName)

        # write private header file
        self.write_kernel_private_header(kernelPrivateHeaderYaml, licenseFileName)

        # write natc source file
        self.write_kernel_natc_sourcefile(kernelPrivateHeaderYaml, licenseFileName)

        # write optimized implementation
        self.write_kernel_optimized_sourcefile(kernelPrivateHeaderYaml, licenseFileName)

        # write CMakeLists.txt in src
        self.write_kernel_cmake("src")

        # write top level source file
        self.write_kernel_top_level_sourcefile(kernelHeaderYaml, licenseFileName)

    # write example directory
    def kernel_example_buildup(
        self, licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
    ):
        """
        Builds up the kernel example files.

        Args:
            licenseFileName (str): The path to the license file.
            kernelPrivateHeaderYaml (str): The path to the kernel private header YAML file.
            kernelHeaderYaml (str): The path to the kernel header YAML file.

        Returns:
            None
        """

        # write CMakeLists.txt in examples
        self.write_kernel_cmake("examples")

        # write kernel example source file
        self.write_kernel_example_sourcefile(kernelHeaderYaml, licenseFileName)

    # write test directory
    def kernel_test_buildup(
        self, licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
    ):
        """
        Builds up the kernel test files.

        Args:
            licenseFileName (str): The path to the license file.
            kernelPrivateHeaderYaml (str): The path to the kernel private header YAML file.
            kernelHeaderYaml (str): The path to the kernel header YAML file.

        Returns:
            None
        """

        # write CMakeLists.txt in test
        self.write_kernel_cmake("test")

    def process_test_case(self, testCaseCsv, licenseFileName, output_dir=None):
        """
        Process a single test case and write the results to a header file.

        Args:
            test_case: CSV containing test case parameters
            output_dir: Directory to write the output files

        Returns:
            None
        """

        # TODO: Implement function in main.py
        pass
