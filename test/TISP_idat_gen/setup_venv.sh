#!/bin/bash
# Setup instructions for audiolib-codegen project using only pyproject.toml

# 1. Create virtual environment
python3 -m venv venv

# 2. Activate virtual environment
source venv/bin/activate

# 3. Install the project and its dependencies directly from pyproject.toml
# When you have network connectivity, run:
pip install -e .

# 4. To include development dependencies, run:
# pip install -e ".[dev]"

# 5. Verify installation
python3 -c "import yaml, dotenv; print('Dependencies successfully installed!')"

# Note: This approach uses only the pyproject.toml file for dependency management
# and doesn't rely on separate requirements files.
