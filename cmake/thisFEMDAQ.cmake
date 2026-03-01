# Write thisFEMDAQ.sh to INSTALL directory

# We identify the thisroot.sh script for the corresponding ROOT version
execute_process(
  COMMAND root-config --prefix
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  OUTPUT_VARIABLE ROOT_PATH)
string(REGEX REPLACE "\n$" "" ROOT_PATH "${ROOT_PATH}")
set(thisROOT "${ROOT_PATH}/bin/thisroot.sh")

# install thisREST script, sh VERSION
install(
  CODE "
file( WRITE \${CMAKE_INSTALL_PREFIX}/thisFEMDAQ.sh
\"\#!/bin/bash

\# check active shell by checking for existence of _VERSION variable
if [[ -n \\\"\\\${BASH_VERSION}\\\" ]] ; then
    thisdir=\\\$(cd \\\$(dirname \\\${BASH_ARGV[0]}) ; pwd)
else
    echo \\\"Invalid shell! Only bash is supported!\\\"
    return 1
fi

\# if thisroot.sh script is found we load the same ROOT version as used in compilation
if [[ -f \\\"${thisROOT}\\\" ]]; then
    source ${thisROOT}
fi

if [ \\\$FEMDAQ_PATH ] ; then
echo switching to FEMDAQ installed in \\\${thisdir}
_PATH=`echo \\\$PATH | sed -e \\\"s\#\\\${FEMDAQ_PATH}/bin:\#\#g\\\"`
_LD_LIBRARY_PATH=`echo \\\$LD_LIBRARY_PATH | sed -e \\\"s\#\\\${FEMDAQ_PATH}/lib:\#\#g\\\"`
else
_PATH=\\\$PATH
_LD_LIBRARY_PATH=\\\$LD_LIBRARY_PATH
fi

export FEMDAQ_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}
export FEMDAQ_PATH=\\\${thisdir}
export FEMDAQ_INCLUDE_PATH=\\\${thisdir}/inc
export PATH=\\\$FEMDAQ_PATH/bin:\\\$_PATH
export LD_LIBRARY_PATH=\\\$FEMDAQ_PATH/lib:\\\$_LD_LIBRARY_PATH
export LIBRARY_PATH=\\\$LIBRARY_PATH:\\\$FEMDAQ_PATH/lib

alias FEMDAQRoot=\\\"FEMDaqRoot -l\\\"

\"
)
        ")
