// --------------------------------------------------------------
// CU Spaceflight Landing Prediction
// Copyright (c) CU Spaceflight 2009, All Right Reserved
//
// Written by Rob Anderson 
// Modified by Fergus Noble
//
// THIS CODE AND INFORMATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY 
// KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
// --------------------------------------------------------------

#ifndef __RUN_MODEL_H__
#define __RUN_MODEL_H__

#include "wind/wind_file_cache.h"
#include "altitude.h"

// run the model
int run_model(wind_file_cache_t* cache, altitude_model_t* alt_model,
              float initial_lat, float initial_lng, float initial_alt, 
	      long int initial_timestamp, float rmswinderror);

#define TIMESTEP 1          // in seconds
#define LOG_DECIMATE 50     // write entry to output files every x timesteps

#define METRES_TO_DEGREES  0.00000899289281755   // one metre corresponds to this many degrees latitude
#define DEGREES_TO_METRES  111198.92345          // one degree latitude corresponds to this many metres
#define DEGREES_TO_RADIANS 0.0174532925          // 1 degree is this many radians

#endif // __RUN_MODEL_H__

