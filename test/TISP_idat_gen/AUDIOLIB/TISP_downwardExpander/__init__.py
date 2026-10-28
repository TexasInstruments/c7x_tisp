"""
TISP_downwardExpander kernel module initialization.
This module exposes the class API for the TISP_downwardExpander kernel.
"""

# Import the class API
from .downwardExpander import TISP_downwardExpander

# Define what should be imported when using "from TISP_downwardExpander import *"
__all__ = ["TISP_downwardExpander"]
