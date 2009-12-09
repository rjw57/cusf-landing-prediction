#!/bin/bash
#
# Script to be run in a cronjob which grabs a load of GFS data around the UK.
#
# Original version by: Rich Wareham <rjw57@cam.ac.uk>

# The root where the various utilities are based.
ROOT=/societies/cuspaceflight/

# The script itself
HOURLYSCRIPT=${ROOT}/git/cusf-landing-prediction/scripts/run-hourly-predictions.py

# Where to run the script
WORKINGDIR=${ROOT}/git/cusf-landing-prediction/scripts/

# Output
OUTPUTDIR=${ROOT}/public_html/hourly-predictions/
if [ ! -d ${OUTPUTDIR} ]; then
	echo "$0: The directory ${OUTPUTDIR} needs to exist and be writable."
	exit 1
fi

LOGDIR=${OUTPUTDIR}/logs/
if [ ! -d ${LOGDIR} ]; then
	echo "$0: The directory ${LOGDIR} needs to exist and be writable."
	exit 1
fi

DATADIR=${OUTPUTDIR}/data/
TMPDIR=${OUTPUTDIR}/data.old/
NEWDIR=${OUTPUTDIR}/new/

# Where local Python packages are. Note that we use export here to make sure
# that this variable is passed onto Python scripts.
export PYTHONPATH=${ROOT}/python-packages

# Move into the working directory.
cd ${WORKINGDIR}

# Check the existance of various things we need.
if [ ! -x ${HOURLYSCRIPT} ]; then
	echo "$0: The $HOURLYSCRIPT script does not exist or is not executable." 
	exit 1
fi

LOGFILE=${LOGDIR}/hourlypredictlog-`date +%Y.%m.%d-%H:%M:%S`

# Run the data grabber from now to 160 hours in future
${HOURLYSCRIPT} 2>${LOGFILE}

if [ -d ${DATADIR} ]; then
	mv ${DATADIR} ${TMPDIR}
fi
mv ${NEWDIR} ${DATADIR}
rm -rf ${TMPDIR}
