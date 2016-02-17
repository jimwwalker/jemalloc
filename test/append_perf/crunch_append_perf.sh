#!/bin/bash

# This script needs to be copied out of the git repo as it checksout old SHAs
echo "WARNING RUN ME FROM /tmp (or somewhere outside of git) AS I FORCEFULLY CLEAN THE GIT REPO"
JEMALLOC_REPO_PATH=$1
PERFTEST=$2
JE_MALLOC_CONF=$3
AUTOGEN_SETTINGS=$4
OUTPUT_FILE=$5

#Get a list of SHAs from git. git log --pretty=oneline c002a5c..4.0.4 | cut -d' ' -f1 > ~/shas
SHA_FILE=$6

export JE_MALLOC_CONF
export LD_LIBRARY_PATH=${JEMALLOC_REPO_PATH}/lib

for SHA in $(cat $SHA_FILE)
do
   cd $JEMALLOC_REPO_PATH
   
   git  clean -xfd
   git checkout $SHA
   ./autogen.sh $AUTOGEN_SETTINGS
   make -j4
   
   # run 3 times.
   echo "$SHA, $($PERFTEST)" >> $OUTPUT_FILE
   echo "$SHA, $($PERFTEST)" >> $OUTPUT_FILE
   echo "$SHA, $($PERFTEST)" >> $OUTPUT_FILE
done



