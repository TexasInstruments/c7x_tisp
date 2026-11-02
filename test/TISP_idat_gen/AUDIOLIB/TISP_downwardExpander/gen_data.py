"""
Script to generate test data for TISP_downwardExpander
"""

import sys
import os
from main import main

# Run the main function from main.py
if __name__ == "__main__":
    # Add 'gen_data' argument to generate test data
    sys.argv.append("gen_data")
    main()
