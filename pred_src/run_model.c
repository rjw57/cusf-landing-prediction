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
#include "run_model.h"
#include "pred.h"
#include "altitude.h"

extern int verbosity;

int run_model(float initial_lat, float initial_lng, float initial_alt, long int initial_timestamp) {

    float alt = initial_alt;
    float lat = initial_lat; 
    float lng = initial_lng;
    long int timestamp = initial_timestamp;
    
    int log_counter = 0; // only write position to output files every LOG_DECIMATE timesteps
    
    float wind_v, wind_u;
    
    wind_data* w_data = NULL;
    
	while(get_altitude(timestamp - initial_timestamp, &alt)) {
	    
	    // wrap lng values if they exceed the defined range - here we use -180 < lng <= 180
        while (lng > 180) lng -= 360;
        while (lng <= -180) lng += 360;
        // NOTE: what to do if the lat exceeds +/-90 is wierd, need to think about what to do in that case
	    
		if(!get_wind(lat, lng, alt, timestamp, &wind_v, &wind_u, &w_data)) {
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
	
	// clean up any left over unfreed memory that get_wind was using
    wind_data_cleanup(w_data);

    return 1;
}


int get_wind(float lat, float lng, float alt, long int timestamp, float* wind_v, float* wind_u, wind_data** data) {
    
    static int current_tile_lat, current_tile_lng;
    static long int current_tile_start_time;
    int tile_lat, tile_lng;
    float lat_index_fraction, lng_index_fraction, time_fraction;
    int lat_index, lng_index;
    
    float interp_lng1, interp_lng2, interp_time1, interp_time2;
    float interp_pressure_above, interp_pressure_below;
    int i, pressure_level_below, pressure_level_above;    
    float alt_i, pressure_level_below_diff, pressure_level_above_diff, pressure_level_fraction;
    
    // work with fixed size tiles so round our coords down to the nearest TILE_SIZE to
    // get tile lat and lng
    tile_lat = (((int)lat)/TILE_SIZE) * TILE_SIZE;
    tile_lng = (((int)lng)/TILE_SIZE) * TILE_SIZE;
    
    if (tile_lat != current_tile_lat || 
        tile_lng != current_tile_lng || 
        timestamp < current_tile_start_time ||
        timestamp > current_tile_start_time+GRIB_TIME_PERIOD || !data) 
    {
        // we have flown off the edge of the current tile so we need to load in a new one
        if (verbosity)
            fprintf(stderr, "loading wind data tile, lat: %d, lng: %d, timestamp: %d\n", tile_lat, tile_lng, timestamp);
            
        if (!load_data(tile_lat, tile_lng, timestamp, data, &current_tile_start_time)) {
            fprintf(stderr, "ERROR: couldn't load requested wind data tile\n");
            return 0;
        }
        if (verbosity)
            fprintf(stderr, "loaded wind data tile!\n");
        
        current_tile_lat = tile_lat;
        current_tile_lng = tile_lng;
    }
    
    // we (now) have the required tile loaded so its all groovy, now find indexes in the array
    // corresponding to the lat and lng
    
    // NOTE: the indicies run from the "bottom-left" corner of the grid, i.e. increasing
    // lat_index corresponds to increasing latitude
    lat_index = (int)((lat - tile_lat)/GRIB_RESOLUTION);
     // fractional part of index, i.e. how far are we from the next index, for interpolation
    lat_index_fraction = ((lat - tile_lat) - lat_index*GRIB_RESOLUTION)/GRIB_RESOLUTION;
         
    lng_index = (int)((lng - tile_lng)/GRIB_RESOLUTION);
    lng_index_fraction = ((lng - tile_lng) - lng_index*GRIB_RESOLUTION)/GRIB_RESOLUTION;
    
    time_fraction = (float)(timestamp - current_tile_start_time)/GRIB_TIME_PERIOD;
    
    // now we need to which pressure levels in the GRIB data lie either side of our desired altitude
    // in the region where the balloon is, so work out altitudes interpolated w.r.t. lat, lng and time
    
    pressure_level_below_diff = pressure_level_above_diff = 1e6; // some large number, higher than we will ever get
    pressure_level_below = pressure_level_above = -1;
    for (i=0; i<NUM_PRESSURE_LEVELS; i++) {
        // Base time
            // First interpolate across latitude at base longitude
            interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][i][0].alt + lat_index_fraction * (**data)[lat_index+1][lng_index][i][0].alt;
            // Next interp across latitude at second longitude
            interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][i][0].alt + lat_index_fraction * (**data)[lat_index+1][lng_index+1][i][0].alt;
            // Now interp across longitude using these values
            interp_time1 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;
            
        // Second time
            interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][i][1].alt + lat_index_fraction * (**data)[lat_index+1][lng_index][i][1].alt;
            interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][i][1].alt + lat_index_fraction * (**data)[lat_index+1][lng_index+1][i][1].alt;
            interp_time2 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;
            
        // Now interp across time to get the altitude of the ith pressure level
        alt_i = (1 - time_fraction)*interp_time1 + time_fraction*interp_time2;
        
        // Now check if this altitude comes closer than the previous ones to our desired altitude
        // keeping track of those above and below separately for interpolation later
        if (alt_i < alt) {
            if ((alt - alt_i) < pressure_level_below_diff) {
                pressure_level_below_diff = alt - alt_i;
                pressure_level_below = i;
            }
        } else {
            if ((alt_i - alt) < pressure_level_above_diff) {
                pressure_level_above_diff = alt_i - alt;
                pressure_level_above = i;
            }
        }
    }
    
    // its possible if our altitude is higher or lower than anything we have got. in this case just use the same pressure level for both
    if (pressure_level_below == -1) {
        pressure_level_below = pressure_level_above;
        pressure_level_below_diff = 0;
    }
    if (pressure_level_above == -1) {
        pressure_level_above = pressure_level_below;
        pressure_level_above_diff = 0;
    }    
    
    // The last thing we need is the pressure level index fraction, i.e. how far between the two levels we are
    pressure_level_fraction = pressure_level_below_diff / (pressure_level_below_diff + pressure_level_above_diff);

    // Now find iterpolated values of wind speed in u and v directions
    // This is about to get a bit intense so you might want to make a cup of tea.
    
    // v direction, pressure level below
        // base time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_below][0].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_below][0].wind_v;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_below][0].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_below][0].wind_v;
        interp_time1 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;
        
        // second time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_below][1].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_below][1].wind_v;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_below][1].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_below][1].wind_v;
        interp_time2 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;

    	interp_pressure_below = (1 - time_fraction)*interp_time1 + time_fraction*interp_time2;
    
    // v direction, pressure level above
        // base time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_above][0].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_above][0].wind_v;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_above][0].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_above][0].wind_v;
        interp_time1 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;
        
        // second time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_above][1].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_above][1].wind_v;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_above][1].wind_v + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_above][1].wind_v;
        interp_time2 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;

    	interp_pressure_above = (1 - time_fraction)*interp_time1 + time_fraction*interp_time2;
    	
    // calculate final wind value in v direction
    *wind_v = (1 - pressure_level_fraction)*interp_pressure_below + pressure_level_fraction*interp_pressure_above;
    
    // u direction, pressure level below
        // base time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_below][0].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_below][0].wind_u;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_below][0].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_below][0].wind_u;
        interp_time1 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;
        
        // second time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_below][1].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_below][1].wind_u;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_below][1].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_below][1].wind_u;
        interp_time2 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;

    	interp_pressure_below = (1 - time_fraction)*interp_time1 + time_fraction*interp_time2;
    
    // u direction, pressure level above
        // base time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_above][0].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_above][0].wind_u;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_above][0].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_above][0].wind_u;
        interp_time1 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;
        
        // second time
        interp_lng1 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index][pressure_level_above][1].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index][pressure_level_above][1].wind_u;
        interp_lng2 = (1 - lat_index_fraction) * (**data)[lat_index][lng_index+1][pressure_level_above][1].wind_u + \
            lat_index_fraction * (**data)[lat_index+1][lng_index+1][pressure_level_above][1].wind_u;
        interp_time2 = (1 - lng_index_fraction)*interp_lng1 + lng_index_fraction*interp_lng2;

    	interp_pressure_above = (1 - time_fraction)*interp_time1 + time_fraction*interp_time2;
    	
    // calculate final wind value in v direction
    *wind_u = (1 - pressure_level_fraction)*interp_pressure_below + pressure_level_fraction*interp_pressure_above;

    return 1;
}

