#!/bin/bash
#
# Script to be run in a cronjob which grabs a load of GFS data around the UK.
#
# Original version by: Rich Wareham <rjw57@cam.ac.uk>

# The root where the various utilities are based.
ROOT=/societies/cuspaceflight/

# The get data script itself
GETDATA=${ROOT}/git/cusf-landing-prediction/pydap/get_wind_data.py

# Where to run the script
WORKINGDIR=${ROOT}/landing-prediction-data/

# Where local Python packages are. Note that we use export here to make sure
# that this variable is passed onto Python scripts.
export PYTHONPATH=${ROOT}/python-packages

# Move into the working directory.
cd ${WORKINGDIR}

# Check the existance of various things we need.
if [ ! -x ${GETDATA} ]; then
	echo "$0: The $GETDATA script does not exist or is not executable." 
	exit 1
fi

GFSDIR=${WORKINGDIR}/gfs
if [ ! -d ${GFSDIR} ]; then
	echo "$0: The directory ${GFSDIR} needs to exist and be writable."
	exit 1
fi

LOGDIR=${WORKINGDIR}/logs
if [ ! -d ${LOGDIR} ]; then
	echo "$0: The directory ${LOGDIR} needs to exist and be writable."
	exit 1
fi

LOGFILE=${LOGDIR}/fetchdatalog-`date +%Y.%m.%d-%H:%M:%S`

# Run the data grabber from now to 24 hours in future
${GETDATA} --lat=52 --lon=0 -v -f 24 2>${LOGFILE}
