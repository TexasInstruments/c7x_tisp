"""
This module characterizes the file io class of a kernel.

It contains functions and attributes to perform low-level tasks and file io transactions necessary for python-based xlib kernel development.

"""

import os
import yaml
from subprocess import call
from sys import platform
import pathlib
import pandas as pd


class KernelFileIO:

    def __init__(self, libName, libRootDir, kernelName):

        # library name (string), e.g. audiolib
        self.libName = libName

        # kernel name (string), e.g. asrc
        self.kernelName = kernelName

        # define kernel directory name
        self.kernelDirName = libName.upper() + "_" + kernelName

        # set library root directory, e.g. /home/user/audiolib
        self.libRootDir = libRootDir

    # write top level source file
    def write_kernel_top_level_sourcefile(self, kernel_data, licenseFileName):
        """Generate the source file for the kernel"""

        with open(kernel_data, "r") as f:
            kernel_yaml = yaml.safe_load(f)

        library_name = self.libName.upper()
        kernel_name = kernel_yaml.get("kernel_name")

        kernel_prefix = f"{library_name}_{kernel_name}"

        functions = kernel_yaml.get("functions", [])

        output_dir = "src"
        output_file = os.path.join(
            f"{self.libRootDir}",
            output_dir,
            f"{self.kernelDirName}",
            f"{kernel_prefix}.cpp",
        )

        lines = []

        # Include headers
        lines.append(f'#include "{kernel_prefix}_priv.h"')
        lines.append(f"{self.libName.upper()}_bufParams.h")

        # Write function implementations
        for func in functions:
            args_str = ", ".join(
                [f"{arg['type']} {arg['name']}" for arg in func.get("arguments", [])]
            )
            lines.append(
                f"{func['return_type']} {kernel_prefix}_{func['name']}({args_str})"
            )
            lines.append("{")
            lines.append("    // TODO: Implement function here")

            # Add a default return value if the function returns something
            if func["return_type"] != "void":
                if "STATUS" in func["return_type"]:
                    lines.append("    return AUDIOLIB_SUCCESS;")
                elif func["return_type"] == "int32_t":
                    lines.append("    return 0;")
                else:
                    lines.append(f"    return ({func['return_type']})0;")

            lines.append("}\n")

        # Join all lines with newlines
        impl_content = "\n".join(lines)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_file, licenseFileName)
            with open(output_file, "a") as f:
                f.write(impl_content)
        else:
            with open(output_file, "w") as f:
                f.write(impl_content)

        print(f"Generated top level source file: {output_file}")

    # write example source file
    def write_kernel_example_sourcefile(self, kernel_data, licenseFileName):
        """Generate an example source file for the kernel"""

        # Load YAML configuration
        with open(kernel_data, "r") as f:
            kernel_yaml = yaml.safe_load(f)

        library_name = self.libName.upper()
        kernel_name = kernel_yaml.get("kernel_name")

        kernel_prefix = f"{library_name}_{kernel_name}"

        # Define path for example file
        output_dir = "examples"
        output_file = os.path.join(
            f"{self.libRootDir}",
            output_dir,
            f"{self.kernelDirName}",
            f"{kernel_prefix}_example.cpp",
        )

        # Create example file content with TODO comment
        lines = []

        # Include headers
        lines.append(f'#include "{self.libName}.h"')
        lines.append("#include <stdint.h>")
        lines.append("#include <stdio.h>")
        lines.append("#include <stdlib.h>")
        lines.append("")

        # Add main function with TODO
        lines.append("int main(void)")
        lines.append("{")
        lines.append("    // TODO: Implement example here")
        lines.append("    return 0;")
        lines.append("}")

        # Join all lines with newlines
        example_content = "\n".join(lines)

        # Write to file with license header
        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_file, licenseFileName)
            with open(output_file, "a") as f:
                f.write(
                    "\n/******************************************************************************/\n"
                )
                f.write(
                    "/*                                                                            */\n"
                )
                f.write(
                    "/* Includes                                                                   */\n"
                )
                f.write(
                    "/*                                                                            */\n"
                )
                f.write(
                    "/******************************************************************************/\n\n"
                )
                f.write(example_content)
        else:
            with open(output_file, "w") as f:
                f.write(example_content)

        print(f"Generated example source file: {output_file}")

    # write CMake file
    def write_kernel_cmake(self, location):
        """Generate CMakeLists.txt file for the kernel"""
        library_name = self.libName.upper()
        kernel_name = self.kernelName

        # Make sure location is valid
        valid_locations = {
            "src": f"{library_name}_create_object_lib_for_kernel()",
            "test": f"{library_name}_create_binary_for_kernel_testing()",
            "examples": f"{library_name}_create_binary_for_example_kernel()",
        }

        if location not in valid_locations:
            raise ValueError(f"Error: Invalid location: {location}")

        # Define path for CMakeLists.txt
        cmake_path = (
            f"{self.libRootDir}/{location}/{library_name}_{kernel_name}/CMakeLists.txt"
        )

        # Create CMakeLists.txt content
        content = "# Call util function to create rules for building object libs for this kernel\n"
        content += f"{valid_locations[location]}"

        # Write the file
        with open(cmake_path, "w") as f:
            f.write(content)

        print(f"Generated CMakeLists.txt: {cmake_path}")

    # write the kernel optimized source files
    def write_kernel_optimized_sourcefile(self, impl_yaml, licenseFileName):
        """
        Generate C++ implementation files for the C7x intrinsic versions of the kernel functions
        with blank implementations and TODO comments

        Args:
            impl_yaml: Path to the YAML file with kernel configuration
            licenseFileName: Path to the license file to include at the top
        """
        library_name = self.libName.upper()
        kernel_name = self.kernelName

        # Load YAML configuration
        yaml_data = {}
        if impl_yaml and os.path.exists(impl_yaml):
            with open(impl_yaml, "r") as f:
                yaml_data = yaml.safe_load(f)

        # Get supported data types from YAML
        data_types = yaml_data.get("supported_data_types", [])

        # Create a combined implementation file for intrinsic versions
        lines = []

        # Include headers
        lines.append(f'#include "../common/c71/{library_name}_inlines.h"')
        lines.append(f'#include "{library_name}_{kernel_name}_priv.h"')
        lines.append("")

        # Add defines
        lines.append(
            "/*******************************************************************************"
        )
        lines.append(" *")
        lines.append(" * DEFINES")
        lines.append(" *")
        lines.append(
            " ******************************************************************************/"
        )
        lines.append("")
        lines.append("#define SE_PARAM_BASE (0x0000)")
        lines.append("#define SE_SE0_PARAM_OFFSET (SE_PARAM_BASE)")
        lines.append(
            "#define SE_SA0_PARAM_OFFSET (SE_SE0_PARAM_OFFSET + SE_PARAM_SIZE)"
        )
        lines.append("")

        #################################
        # Generate init_ci implementation
        #################################

        # Find the function details in the YAML
        init_ci_details = None
        for func in yaml_data.get("private_functions", []):
            if func["name"] == "init_ci":
                init_ci_details = func
                break

        if init_ci_details:
            template_type = init_ci_details.get("template", "dataType")
            return_type = init_ci_details.get("return_type", "void")

            # Build function signature
            args_list = []
            for arg in init_ci_details.get("arguments", []):
                args_list.append(f"{arg['type']} {arg['name']}")
            args_str = ", ".join(args_list)

            func_sig = f"template <typename {template_type}>\n{return_type}\n{library_name}_{kernel_name}_init_ci({args_str})"

            lines.append(func_sig)
            lines.append("{")

            # Add TODO comment instead of implementation
            lines.append(
                "   // TODO: Implement the optimized initialization for this kernel"
            )

            # Add return statement for non-void functions
            if return_type != "void":
                lines.append(f"   return {library_name}_SUCCESS;")

            lines.append("}")
            lines.append("")

            # Add template instantiations for init_ci
            for data_type in data_types:
                lines.append(
                    f"template {return_type} {library_name}_{kernel_name}_init_ci<{data_type}>({args_str});"
                )

            lines.append("")

        #################################
        # Generate exec_ci implementation
        #################################

        # Find the function details in the YAML
        exec_ci_details = None
        for func in yaml_data.get("private_functions", []):
            if func["name"] == "exec_ci":
                exec_ci_details = func
                break

        if exec_ci_details:
            template_type = exec_ci_details.get("template", "dataType")
            return_type = exec_ci_details.get("return_type", "void")

            # Build function signature
            args_list = []
            for arg in exec_ci_details.get("arguments", []):
                args_list.append(f"{arg['type']} {arg['name']}")
            args_str = ", ".join(args_list)

            func_sig = f"template <typename {template_type}>\n{return_type}\n{library_name}_{kernel_name}_exec_ci({args_str})"

            lines.append(func_sig)
            lines.append("{")

            # Add TODO comment instead of implementation
            lines.append(
                "   // TODO: Implement the optimized execution for this kernel"
            )

            # Add return statement for non-void functions
            if return_type != "void":
                lines.append(f"   return {library_name}_SUCCESS;")

            lines.append("}")
            lines.append("")

            # Add template instantiations for exec_ci
            for data_type in data_types:
                lines.append(f"template {return_type}")
                lines.append(
                    f"{library_name}_{kernel_name}_exec_ci<{data_type}>({args_str});"
                )
                lines.append("")

        # Join all lines with newlines
        impl_content = "\n".join(lines)

        # Write the combined C7x implementation file
        output_path = f"{self.libRootDir}/src/{library_name}_{kernel_name}/{library_name}_{kernel_name}_ci.cpp"
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_path, licenseFileName)
            with open(output_path, "a") as f:
                f.write(impl_content)
        else:
            with open(output_path, "w") as f:
                f.write(impl_content)

        print(f"Generated C7x optimized source file: {output_path}")

    # write natural c exec source file
    def write_kernel_natc_sourcefile(self, impl_yaml, licenseFileName):
        """
        Generate a C++ implementation file for a specific kernel function with template instantiations

        Args:
            impl_yaml: Path to the YAML file with kernel configuration
            licenseFileName: Path to the license file to include at the top
            function_name: Name of the function to implement (e.g., "exec_cn")
        """
        library_name = self.libName.upper()
        kernel_name = self.kernelName

        # Load YAML configuration
        yaml_data = {}
        if impl_yaml and os.path.exists(impl_yaml):
            with open(impl_yaml, "r") as f:
                yaml_data = yaml.safe_load(f)

        # Find the function details in the YAML
        function_details = None
        function_name = "exec_cn"

        for func in yaml_data.get("private_functions", []):
            if func["name"] == function_name:
                function_details = func
                break

        if not function_details:
            print(f"Error: Function {function_name} not found in YAML configuration")
            return

        # Create implementation file content
        lines = []

        # Include headers
        lines.append(f'#include "{library_name}_{kernel_name}_priv.h"')
        lines.append("")

        # Function implementation
        template_type = function_details.get("template", "dataType")
        return_type = function_details.get("return_type", "void")

        # Build function signature
        args_list = []
        for arg in function_details.get("arguments", []):
            args_list.append(f"{arg['type']} {arg['name']}")
        args_str = ", ".join(args_list)

        func_sig = f"template <typename {template_type}>\n{return_type}\n{library_name}_{kernel_name}_{function_name}({args_str})"

        lines.append(func_sig)
        lines.append("{")

        # Add TODO comment instead of implementation
        if function_name == "exec_cn":
            lines.append(f"   // TODO: Implement natural C for this kernel")
        elif function_name == "exec_ci":
            lines.append(f"   // TODO: Implement C7x optimized version for this kernel")
        elif function_name == "init_ci":
            lines.append(f"   // TODO: Implement C7x initialization for this kernel")
        else:
            lines.append(f"   // TODO: Implement {function_name} for this kernel")

        # Add return statement for non-void functions
        if return_type != "void":
            lines.append(f"   return {library_name}_SUCCESS;")

        lines.append("}")
        lines.append("")

        # Get supported data types from YAML
        data_types = yaml_data.get("supported_data_types", [])

        # Add template instantiations
        lines.append("// explicit instantiation for the different data type versions")
        for data_type in data_types:
            instantiation = f"template {return_type}\n{library_name}_{kernel_name}_{function_name}<{data_type}>({args_str});"
            lines.append(instantiation)

        # Join all lines with newlines
        impl_content = "\n".join(lines)

        # Write the implementation file
        output_path = f"{self.libRootDir}/src/{library_name}_{kernel_name}/{library_name}_{kernel_name}_cn.cpp"
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_path, licenseFileName)
            with open(output_path, "a") as f:
                f.write(impl_content)
        else:
            with open(output_path, "w") as f:
                f.write(impl_content)

        print(f"Generated natural C source file: {output_path}")

    # write kernel private header
    def write_kernel_private_header(self, priv_header_yaml, licenseFileName):
        """
        Generate a C private header file for the specified library and kernel with YAML configuration.
        """
        library_name = self.libName.upper()
        kernel_name = self.kernelName

        yaml_data = {}
        if priv_header_yaml and os.path.exists(priv_header_yaml):
            with open(priv_header_yaml, "r") as f:
                yaml_data = yaml.safe_load(f)

        # Define include guard without IXX_IXX_OXX
        include_guard = library_name + "_" + kernel_name.upper() + "_PRIV_H_"

        # Create header file content line by line to ensure no indentation
        lines = []

        # Header guards and includes
        lines.append("#ifndef " + include_guard)
        lines.append("#define " + include_guard)
        lines.append("")
        lines.append('#include "../common/' + library_name + '_utility.h"')
        lines.append('#include "' + library_name + "_" + kernel_name + '.h"')
        lines.append("")

        # File documentation
        lines.append("/**")
        lines.append(" * \\file")
        lines.append(
            " * \\brief Header file for kernel's internal use. For the kernel's interface,"
        )
        lines.append(" * please see @ref " + library_name + "_" + kernel_name)
        lines.append(" */")
        lines.append("")

        # Macros
        for macro in yaml_data.get("macros", []):
            if macro.get("description"):
                lines.append("/**")
                lines.append(" * @brief " + macro.get("description", ""))
                if macro.get("details"):
                    lines.append(" *")
                    for detail in macro.get("details", []):
                        lines.append(" *        " + detail)
                lines.append(" *")
                lines.append(" */")
                lines.append("")

            lines.append("#define " + macro["name"] + " " + macro["value"])

        # Function pointer typedefs
        for func_ptr in yaml_data.get("function_pointers", []):
            if func_ptr.get("description"):
                lines.append("/**")
                lines.append(" *  @brief " + func_ptr.get("description", ""))
                if func_ptr.get("details"):
                    lines.append(" *")
                    for detail in func_ptr.get("details", []):
                        lines.append(" *         " + detail)
                lines.append(" */")

            # Build function pointer typedef
            args_str = ", ".join(
                [
                    arg["type"] + " " + arg["name"]
                    for arg in func_ptr.get("arguments", [])
                ]
            )
            lines.append(
                "typedef "
                + func_ptr.get("return_type", "void")
                + " (*"
                + func_ptr["name"]
                + ")("
                + args_str
                + ");"
            )
            lines.append("")

        # Private functions
        for func in yaml_data.get("private_functions", []):
            # Create function documentation
            lines.append("/**")
            lines.append(" *  @brief " + func.get("description", ""))

            if func.get("details"):
                lines.append(" *")
                lines.append(" * @details " + func.get("details", [])[0])
                for detail in func.get("details", [])[1:]:
                    lines.append(" *          " + detail)

            # Process arguments
            lines.append(" *")
            for arg in func.get("arguments", []):
                io_type = arg.get("io_type", "in")
                lines.append(
                    " *  @param ["
                    + io_type
                    + "]  "
                    + arg["name"].ljust(10)
                    + ": "
                    + arg["description"]
                )

            # Return description
            if func.get("return_description"):
                lines.append(" *")
                lines.append(" *  @return      " + func.get("return_description"))

            # Performance considerations
            if func.get("performance_considerations"):
                lines.append(" *")
                lines.append(" *  @par Performance Considerations:")
                lines.append(" *    For best performance,")
                for consideration in func.get("performance_considerations", []):
                    lines.append(" *    - " + consideration)

            lines.append(" *")
            lines.append(" */")

            # Function signature
            args_str = ", ".join(
                [arg["type"] + " " + arg["name"] for arg in func.get("arguments", [])]
            )

            # Check if template is needed
            if func.get("template"):
                lines.append("template <typename " + func.get("template") + ">")
                lines.append(
                    "extern "
                    + func.get("return_type", "void")
                    + "\n"
                    + library_name
                    + "_"
                    + kernel_name
                    + "_"
                    + func["name"]
                    + "("
                    + args_str
                    + ");"
                )
            else:
                lines.append(
                    func.get("return_type", "void")
                    + "\n"
                    + library_name
                    + "_"
                    + kernel_name
                    + "_"
                    + func["name"]
                    + "("
                    + args_str
                    + ");"
                )

            lines.append("")

        # Private structures
        for struct in yaml_data.get("private_structures", []):
            lines.append("/**")
            lines.append(" * @brief " + struct.get("description", ""))
            lines.append(" */")
            lines.append("typedef struct {")

            for field in struct.get("fields", []):
                if field.get("description"):
                    lines.append(
                        "   /** @brief " + field.get("description", "").ljust(60) + "*/"
                    )
                lines.append("   " + field["type"] + " " + field["name"] + ";")

            lines.append(
                "} " + library_name + "_" + kernel_name + "_" + struct["name"] + ";"
            )
            lines.append("")

        # Close header
        lines.append("#endif /* " + include_guard + " */")
        lines.append("")
        lines.append(
            "/* ======================================================================== */"
        )
        lines.append(
            "/*  End of file:  "
            + library_name
            + "_"
            + kernel_name
            + "_priv.h          */"
        )
        lines.append(
            "/* ======================================================================== */"
        )

        # Join all lines with newlines
        header_content = "\n".join(lines)

        # Write the header file
        output_path = f"{self.libRootDir}/src/{library_name}_{kernel_name}/{library_name}_{kernel_name}_priv.h"
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_path, licenseFileName)
            with open(output_path, "a") as f:
                f.write(header_content)
        else:
            with open(output_path, "w") as f:
                f.write(header_content)

        print(f"Generated private header file: {output_path}")

    # write top level kernel header files
    def write_kernel_header(self, header_yaml, licenseFileName):
        """
        Generate a C header file for the specified library and function name with YAML configuration.
        """
        library_name = self.libName.upper()
        kernel_name = self.kernelName

        yaml_data = {}
        if header_yaml and os.path.exists(header_yaml):
            with open(header_yaml, "r") as f:
                yaml_data = yaml.safe_load(f)

        # Generate custom types
        types_content = []
        for type_def in yaml_data.get("types", []):
            doc = "/**\n * @brief " + type_def.get("description", "") + "\n */\n"
            typedef = "typedef struct {\n"
            for field in type_def.get("fields", []):
                typedef += (
                    "   /** @brief " + field.get("description", "").ljust(70) + "*/\n"
                )
                typedef += "   " + field["type"] + " " + field["name"] + ";\n"
            # Just use the type name directly without prefixing the kernel name again
            typedef += (
                "} " + library_name + "_" + kernel_name + "_" + type_def["name"] + ";"
            )
            types_content.append(doc + typedef)

        all_types = "\n\n".join(types_content)

        # Process functions
        functions_content = []
        for func in yaml_data.get("functions", []):
            # Create function documentation
            doc = "/**\n"
            doc += " *  @brief       " + func.get("description", "") + "\n"

            if func.get("details"):
                doc += " *\n *  @details     "
                doc += "\n *               ".join(func.get("details", []))
                doc += "\n"
            elif "details_reference" in func:
                doc += " *\n *  @details     Please refer to details under\n"
                doc += (
                    " *               @ref "
                    + library_name
                    + "_"
                    + kernel_name
                    + "_"
                    + func.get("details_reference")
                    + "\n"
                )

            # Process arguments
            doc += " *\n"
            for arg in func.get("arguments", []):
                io_type = arg.get("io_type", "in")
                doc += (
                    " *  @param ["
                    + io_type
                    + "]  "
                    + arg["name"].ljust(10)
                    + ": "
                    + arg["description"]
                    + "\n"
                )

            # Return description
            if func.get("return_description"):
                doc += " *\n"
                doc += " *  @return      " + func.get("return_description") + "\n"

            # Assumptions
            if func.get("assumptions"):
                doc += " *\n"
                doc += " *  @par Assumptions:\n"
                for assumption in func.get("assumptions", []):
                    doc += " *    - " + assumption + "\n"

            # Performance considerations
            if func.get("performance_considerations"):
                doc += " *\n"
                doc += " *  @par Performance Considerations:\n"
                doc += " *    For best performance,\n"
                for consideration in func.get("performance_considerations", []):
                    doc += " *    - " + consideration + "\n"

            # Remarks
            if func.get("remarks"):
                doc += " *\n"
                doc += " *  @remarks     "
                doc += "\n *               ".join(func.get("remarks", []))
                doc += "\n"

            doc += " */\n"

            # Function signature
            args_str = ", ".join(
                [f"{arg['type']} {arg['name']}" for arg in func.get("arguments", [])]
            )
            func_sig = (
                func.get("return_type", "void")
                + "\n"
                + library_name
                + "_"
                + kernel_name
                + "_"
                + func.get("name")
                + "("
                + args_str
                + ");"
            )

            functions_content.append(doc + func_sig)

        # Join all function declarations
        all_functions = "\n\n".join(functions_content)

        # Define include guard without IXX_IXX_OXX
        include_guard = library_name + "_" + kernel_name.upper() + "_H_"

        # Create header file content line by line to ensure no indentation
        lines = []

        # Header guards and includes
        lines.append("#ifndef " + include_guard)
        lines.append("#define " + include_guard)
        lines.append("")
        lines.append('#include "../common/' + library_name + '_types.h"')
        lines.append("")
        lines.append("#ifdef __cplusplus")
        lines.append('extern "C" {')
        lines.append("#endif")
        lines.append("")

        # Module documentation
        lines.append("/**")
        lines.append(
            " * @defgroup "
            + library_name
            + "_"
            + kernel_name
            + " "
            + library_name
            + "_"
            + kernel_name
        )
        lines.append(
            " * @brief " + yaml_data.get("description", "Kernel for processing data")
        )
        lines.append(" *")
        lines.append(" * @details")

        # Add details
        if yaml_data.get("details"):
            for i, detail in enumerate(yaml_data.get("details")):
                if i == 0:
                    lines.append(" *          " + detail)
                else:
                    lines.append(" *          - " + detail)

        lines.append(" *")
        lines.append(" * @ingroup  " + library_name)
        lines.append(" */")
        lines.append("")
        lines.append("/**@{*/")
        lines.append("")

        # Add types if any
        if all_types:
            lines.append(all_types)
            lines.append("")

        # Add functions
        lines.append(all_functions)
        lines.append("")
        lines.append("/**@}*/")
        lines.append("")
        lines.append("#ifdef __cplusplus")
        lines.append("}")
        lines.append("#endif")
        lines.append("")
        lines.append("#endif /* " + include_guard + " */")
        lines.append("")
        lines.append(
            "/* ======================================================================== */"
        )
        lines.append(
            "/*  End of file:  "
            + library_name
            + "_"
            + kernel_name
            + ".h                   */"
        )
        lines.append(
            "/* ======================================================================== */"
        )

        # Join all lines with newlines
        header_content = "\n".join(lines)

        # Write the header file
        output_path = f"{self.libRootDir}/src/{library_name}_{kernel_name}/{library_name}_{kernel_name}.h"
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_path, licenseFileName)
            with open(output_path, "a") as f:
                f.write(header_content)
        else:
            with open(output_path, "w") as f:
                f.write(header_content)

        print(f"Generated header file: {output_path}")

    # write ti prequel to file
    def write_ti_license(self, fileName, licenseFileName):

        try:
            with open(licenseFileName, "r") as source:
                content = source.read()

                with open(fileName, "w") as destination:
                    destination.write(content)

                    # print(f"Successfully copied content from {licenseFileName} to {fileName}")
                    return True

        except Exception as e:
            print(f"Error copying file: {e}")
            return False

    # write attributes to static test buffers
    def write_attributes(self, f, attributes, stringToWrite):
        """Write attributes to a file with conditional code for Windows vs other platforms.

        Args:
            f: File object to write to
            attributes: Attributes string to use for non-Windows platforms
            stringToWrite: String to write in both cases
        """
        f.write("#ifdef WIN32\n")
        f.write(stringToWrite)
        f.write("#else\n")
        f.write(attributes + stringToWrite)
        f.write("#endif\n")
        f.write("{\n")

    # write static test buffers into header files
    def write_arrays_to_header(
        self,
        data_arrays,
        array_names,
        data_types,
        output_path,
        section_attributes=None,
        format_strs=None,
        indent=True,
        indent_params=None,
    ):
        """Write multiple data arrays to a header file with customizable formatting.

        Args:
            data_arrays: List of data arrays to write
            array_names: List of names for the arrays
            data_types: List of data types for the arrays (e.g., "int32_t")
            output_path: Full path to the output header file
            section_attributes: List of section attributes for each array (default: None)
            format_strs: List of format strings for each array (default: ["{:>2}"] * len(data_arrays))
            indent: Whether to run the indent command on the output file (default: True)
            indent_params: List of parameters for the indent command (default: ["-nut", "-i3", "-l100"])
        """
        # Validate inputs
        if not (len(data_arrays) == len(array_names) == len(data_types)):
            raise ValueError(
                "data_arrays, array_names, and data_types must have the same length"
            )

        # Set defaults
        if format_strs is None:
            format_strs = ["{:>2}"] * len(data_arrays)

        if section_attributes is None:
            section_attributes = [""] * len(data_arrays)

        if indent_params is None:
            indent_params = ["-nut", "-i3", "-l100"]

        # Create directory if it doesn't exist
        output_dir = os.path.dirname(output_path)
        if output_dir and not os.path.exists(output_dir):
            pathlib.Path(output_dir).mkdir(parents=True, exist_ok=True)

        # Write the data arrays to the header file
        with open(output_path, "w") as f:
            for i, (data, name, dtype, attr, fmt) in enumerate(
                zip(
                    data_arrays,
                    array_names,
                    data_types,
                    section_attributes,
                    format_strs,
                )
            ):
                if attr:
                    self.write_attributes(f, attr, dtype + " " + name + "[]=\n")
                else:
                    f.write(dtype + " " + name + "[]=\n{\n")

                length = len(data)
                for j in range(length):
                    print(fmt.format(data[j]), file=f, end=", ")
                    # Add newline every 10 items for readability
                    if (j + 1) % 10 == 0 and j < length - 1:
                        print("", file=f)

                f.write("\n};\n\n")

        # Run indent command if requested
        if indent and (platform == "linux" or platform == "linux2"):
            indent_cmd = ["indent"] + indent_params + [output_path]
            call(indent_cmd)
            call(["rm", output_path + "~"])  # remove backup file

    def read_test_cases(self, csv_file):
        """
        Read test cases from a CSV file and generate random parameters.

        Args:
            csv_file: Path to the CSV file

        Returns:
            A dataframe containing information of all test cases
        """
        testCases_df = []

        testCases_df = pd.read_csv(csv_file)

        print(f"Read {len(testCases_df)} test cases from {csv_file}")
        return testCases_df

    def write_idat_header(
        self, testParamStruct, licenseFileName, outputDir, libHeader=None
    ):

        structName = f"{self.kernelDirName}_testParams_t"
        fileName = f"{self.kernelDirName}_idat.h"
        output_path = f"{outputDir}/{fileName}"

        lines = []

        lines.append(f"#ifndef {self.kernelDirName.upper()}_IDAT_H")
        lines.append(f"#define {self.kernelDirName.upper()}_IDAT_H\n")

        # header macros
        if libHeader == None:
            lines.append(f"#include <{self.libName.lower()}.h>\n")
        else:
            lines.append(f"#include <{libHeader.lower()}.h>\n")

        lines.append(f'#include "../common/{self.libName.upper()}_test.h"\n')

        lines.append("#ifdef __cplusplus")
        lines.append('extern "C" {')
        lines.append("#endif")

        lines.append("typedef struct {\n")

        for testParamKey in testParamStruct:

            lines.append(f"     {testParamStruct[testParamKey]} {testParamKey};")

        lines.append("\n}" + f" {structName};\n")

        lines.append(
            f"void {self.kernelDirName}_getTestParams({structName} **params, int32_t *numTests);\n"
        )

        lines.append("#ifdef __cplusplus")
        lines.append("}")
        lines.append("#endif")

        lines.append(f"#endif /* define {self.kernelDirName.upper()}_IDAT_H */\n")

        lines.append(
            "/* ======================================================================== */"
        )
        lines.append(
            f"/*  End of file:  {fileName}                                               */"
        )
        lines.append(
            "/* ======================================================================== */"
        )

        lines.append("\n")

        idatContent = "\n".join(lines)

        # Write the header file
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_path, licenseFileName)
            with open(output_path, "a") as f:
                f.write(idatContent)
        else:
            with open(output_path, "w") as f:
                f.write(idatContent)

        print(f"Generated idat header file: {output_path}")

    def write_idat_source(self, testParamStruct, licenseFileName, outputDir):

        fileName = f"{self.kernelDirName}_idat.c"
        output_path = f"{outputDir}/{fileName}"

        lines = []

        lines.append(f'#include "{self.kernelDirName}_idat.h"\n')

        lines.append("/* This file is autogenerated */")
        lines.append("/* Please do not edit */\n")

        for testParam in testParamStruct:

            lines.append(
                f'#if (defined(ALL_TEST_CASES) || (TEST_CASE == {testParam["testId"]}))'
            )
            lines.append(f'#include "test_data/staticRefCase{testParam["testId"]}.h"')
            lines.append("#endif\n")

        lines.append(f"static {self.kernelDirName}_testParams_t testParams[] = " + "{")

        for testParam in testParamStruct:

            lines.append(
                f'#if (defined(ALL_TEST_CASES) || (TEST_CASE == {testParam["testId"]}))'
            )
            lines.append("{")

            for testEntry in testParam.values():

                lines.append(f"     {testEntry},")

            lines.append("},")
            lines.append("#endif\n")

        lines.append("\n};")

        lines.append("/* Sends the test parameter structure and number of tests */")
        lines.append(
            f"void {self.kernelDirName}_getTestParams ({self.kernelDirName}_testParams_t ** params, int32_t *numTests)"
        )
        lines.append("{")
        lines.append("      *params = testParams;")
        lines.append(
            f"      *numTests = sizeof (testParams) / sizeof ({self.kernelDirName}_testParams_t);"
        )
        lines.append("}")
        lines.append("\n")
        idatContent = "\n".join(lines)

        # Write the header file
        os.makedirs(os.path.dirname(output_path), exist_ok=True)

        if licenseFileName and os.path.exists(licenseFileName):
            self.write_ti_license(output_path, licenseFileName)
            with open(output_path, "a") as f:
                f.write(idatContent)
        else:
            with open(output_path, "w") as f:
                f.write(idatContent)

        print(f"Generated idat source file: {output_path}")
