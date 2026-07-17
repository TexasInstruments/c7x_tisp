"""
TISP_idat_gen package initialization.
This package contains Python models for various TISP libraries.
This file dynamically imports all library modules and their classes in the package directory.
"""

import os
import importlib
import inspect

# Initialize the __all__ list
__all__ = []

# Get the package directory
package_dir = os.path.dirname(__file__)

# List of library directories to import
library_dirs = ["AUDIOLIB", "DSPLIB", "MATHLIB", "superNode"]

# Dynamically import all library directories that are modules (contain __init__.py)
for lib_dir in library_dirs:
    lib_path = os.path.join(package_dir, lib_dir)
    # Check if it's a directory and has an __init__.py file
    if os.path.isdir(lib_path) and os.path.exists(
        os.path.join(lib_path, "__init__.py")
    ):
        # Import the module
        module_name = lib_dir
        try:
            # Import the module
            module = importlib.import_module(f".{module_name}", __name__)
            globals()[module_name] = module

            # Add the module name to __all__
            __all__.append(module_name)

            # Import all classes from the module and add them to globals and __all__
            if hasattr(module, "__all__"):
                for class_name in module.__all__:
                    # Get the class from the module
                    cls = getattr(module, class_name)
                    # Add it to globals so it can be imported directly from this package
                    globals()[class_name] = cls
                    # Add it to __all__
                    __all__.append(class_name)

        except ImportError as e:
            print(f"Warning: Could not import {module_name}: {e}")
