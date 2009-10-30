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

#include <math.h>
#include <stdio.h>

#include "wind_data.h"
#include "wind_file.h"
#include "run_model.h"
#include "pred.h"
#include "altitude.h"

extern int verbosity;

int run_model(wind_file_cache_t* cache, float initial_lat, float initial_lng, float initial_alt, long int initial_timestamp) {

    float alt = initial_alt;
    float lat = initial_lat; 
    float lng = initial_lng;
    long int timestamp = initial_timestamp;
    
    int log_counter = 0; // only write position to output files every LOG_DECIMATE timesteps
    
    float wind_v, wind_u;
    
	while(get_altitude(timestamp - initial_timestamp, &alt)) {
	    
	    // wrap lng values if they exceed the defined range - here we use -180 < lng <= 180
        while (lng > 180) lng -= 360;
        while (lng <= -180) lng += 360;
        // NOTE: what to do if the lat exceeds +/-90 is wierd, need to think about what to do in that case
	    
		if(!get_wind(cache, lat, lng, alt, timestamp, &wind_v, &wind_u)) {
            fprintf(stderr, "ERROR: error getting wind data\n");
			return 0;
		}

		// NOTE: it this really the right thing to be doing? - think about what happens near the poles
		lat += wind_v * TIMESTEP * METRES_TO_DEGREES;   // 1 degree of latitude always corresponds to same distance
		// depending on latitude 1 degree of longitude is equivalent to a different distance in metres
		// assuming earth is spherical as the data from the grib does
        lng += wind_u * TIMESTEP * METRES_TO_DEGREES / fabs(cos(DEGREES_TO_RADIANS * lat));

		if (log_counter == LOG_DECIMATE) {
            write_position(lat, lng, alt, timestamp);
            log_counter = 0;
		}
        
        log_counter++;
        timestamp += TIMESTEP;

	}

    return 1;
}


int get_wind(wind_file_cache_t* cache, float lat, float lng, float alt, long int timestamp, float* wind_v, float* wind_u) {
    int i;
    float lambda, wu_l, wv_l, wu_h, wv_h;
    wind_file_cache_entry_t* found_entries[] = { NULL, NULL };
    unsigned int earlier_ts, later_ts;

    static wind_file_cache_entry_t* loaded_cache_entries[] = { NULL, NULL };
    static wind_file_t* loaded_files[] = { NULL, NULL };

    // look for a wind file which matches this latitude and longitude...
    wind_file_cache_find_entry(cache, lat, lng, timestamp, 
            &(found_entries[0]), &(found_entries[1]));

    if(!found_entries[0] || !found_entries[1]) {
        fprintf(stderr, "ERROR: Could not locate appropriate wind data tile for time.\n");
        return 0;
    }

    if(!wind_file_cache_entry_contains_point(found_entries[0], lat, lng) || 
            !wind_file_cache_entry_contains_point(found_entries[1], lat, lng))
    {
        fprintf(stderr, "ERROR: Could not locate appropriate wind data tile for location.\n");
        return 0;
    }

    // Load any files we might need
    for(i=0; i<2; ++i)
    {
        if((loaded_cache_entries[i] != found_entries[i]) || !loaded_files[i])
        {
            if(loaded_files[i]) 
            {
                wind_file_free(loaded_files[i]);
            }
            loaded_cache_entries[i] = found_entries[i];
            loaded_files[i] = wind_file_new(
                    wind_file_cache_entry_file_path(loaded_cache_entries[i]));
        }
    }

    earlier_ts = wind_file_cache_entry_timestamp(loaded_cache_entries[0]);
    later_ts = wind_file_cache_entry_timestamp(loaded_cache_entries[1]);

    if(earlier_ts == later_ts)
    {
        fprintf(stderr, "WARN: Do not have two data files around current time. "
                        "Expect the results to be wrong!\n");
    }

    if(earlier_ts != later_ts)
        lambda = ((float)timestamp - (float)earlier_ts) /
            ((float)later_ts - (float)earlier_ts);
    else
        lambda = 0.5f;

    wind_file_get_wind(loaded_files[0], lat, lng, alt, &wu_l, &wv_l);
    wind_file_get_wind(loaded_files[1], lat, lng, alt, &wu_h, &wv_h);

    *wind_u = lambda * wu_h + (1.f-lambda) * wu_l;
    *wind_v = lambda * wv_h + (1.f-lambda) * wv_l;

    return 1;
}

