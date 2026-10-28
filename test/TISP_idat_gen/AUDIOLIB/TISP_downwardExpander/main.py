"""
Driver script for TISP_downwardExpander that sets up the kernel infrastructure
"""

import os
import traceback
import sys

sys.path.append("../../")
# Import the TISP_downwardExpander class from the expander module
from downwardExpander import TISP_downwardExpander


# Main function implementation
def main():

    try:

        # Check number of command line arguments
        if len(sys.argv) < 2:

            raise Exception("No command-line arguments given!")

        # TODO: define the kernel characteristics in yaml files
        kernelHeaderYaml = os.path.join(
            os.path.dirname(__file__), "generate_header.yaml"
        )
        kernelPrivateHeaderYaml = os.path.join(
            os.path.dirname(__file__), "generate_private_header.yaml"
        )

        # define test cases
        testCaseCsv = os.path.join(os.path.dirname(__file__), "test_cases_list.csv")

        # set up license file
        licenseFileName = os.path.join(
            os.path.dirname(__file__), "..", "common", "ti_prequel_2025.txt"
        )

        # TODO: set library root
        libRootDir = os.path.join(os.path.dirname(__file__), "../../../../")

        # init constructor with only libRootDir, blockSize, and numChannels parameters
        k0 = TISP_downwardExpander(libRootDir, blockSize=None, numChannels=None)

        if "init" in sys.argv:
            # NOTE: should only be used to initially set up the kernel in the library
            k0.kernel_create()

            # populate kernel infrastructure with boilerplate code
            k0.kernel_buildup(
                licenseFileName, kernelPrivateHeaderYaml, kernelHeaderYaml
            )

        if "gen_data" in sys.argv:

            # process test cases and generate idat files
            outputIdatDir = k0.libRootDir + "/test/AUDIOLIB/" + k0.kernelDirName
            k0.process_test_case(
                testCasesCsv=testCaseCsv,
                licenseFileName=licenseFileName,
                output_dir=outputIdatDir,
                gen_idat_header=True,
            )

        if "delete" in sys.argv:

            # NOTE: should only be used to remove kernel existence entirely
            k0.kernel_teardown()

    except Exception as e:
        print(f"Error: {e}")
        traceback.print_exc()


# define entry point
if __name__ == "__main__":

    # run application
    main()
