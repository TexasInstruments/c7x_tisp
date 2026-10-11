#!/bin/bash


pushd () {
    command pushd "$@" > /dev/null
}

popd () {
    command popd "$@" > /dev/null
}

export THIS_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"

export REPO_WORK_DIR=${THIS_SCRIPT_DIR}/../

declare -a kernels_target+=($(find ${REPO_WORK_DIR}/test/TISP_idat_gen/DSPLIB/ -mindepth 1 -maxdepth 1 -type d ! \( -iname  "common" -o -iname "__pycache__" \) | grep -o -P '.{0}test/.*' | cut -c 20-))
kernels_target+=($(find ${REPO_WORK_DIR}/test/TISP_idat_gen/AUDIOLIB/ -mindepth 1 -maxdepth 1 -type d ! \( -iname "common" -o -iname "__pycache__" \) | grep -o -P '.{0}test/.*' | cut -c 20-))
kernels_target+=($(find ${REPO_WORK_DIR}/test/TISP_idat_gen/MATHLIB/ -mindepth 1 -maxdepth 1 -type d ! \( -iname "common" -o -iname "__pycache__" \) | grep -o -P '.{0}test/.*' | cut -c 20-))
kernels_target+=($(find ${REPO_WORK_DIR}/test/TISP_idat_gen/superNode/ -mindepth 2 -maxdepth 2 -type d ! \( -iname "common" -o -iname "__pycache__" \) | grep -o -P '.{0}test/.*' | cut -c 20-))
kernels_target+=($(find ${REPO_WORK_DIR}/test/TISP_idat_gen/misc/ -mindepth 1 -maxdepth 1 -type d ! \( -iname "common" -o -iname "__pycache__" \) | grep -o -P '.{0}test/.*' | cut -c 20-))

kernels_target+=" superNode/TISP_matMul_superNode"



echo ${kernels_target[@]}




# for i in "${kernels_target[@]}"; do
gen_test_case(){
    echo ${1}
    i=${1} # get kernel name
    mkdir -p ${THIS_SCRIPT_DIR}/../test/${i}/test_data
    echo ""
    echo "##############################################################"
    echo ""
    echo " Generating test case headers for ${i}"
    
    echo "##############################################################"
    echo ""
    
    pushd ${THIS_SCRIPT_DIR}/../test/TISP_idat_gen/${i}
    echo "The current dir for python test case generation:"
    echo $(pwd)
    python3 gen_data.py -a
    popd
    echo ""
}
# done

export -f gen_test_case

parallel --gnu -j0  gen_test_case ::: ${kernels_target[@]}


