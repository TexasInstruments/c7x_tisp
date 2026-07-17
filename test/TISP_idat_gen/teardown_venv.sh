#!/bin/bash
# Script to deactivate and teardown virtual environment after every use

# deactivate virtual environment
deactivate

# remove environment related directories and files
rm -rf venv
rm -rf audiolib_codegen.egg-info
