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

#define RADIUS_OF_EARTH 6371009.f

typedef struct model_state_s model_state_t;
struct model_state_s
{
    float               lat;
    float               lng;
    float               alt;
    altitude_model_t   *alt_model;
};

// Get the distance (in metres) of one degree of latitude and one degree of
// longitude. This varys with height (not much grant you).
static void
_get_frame(float lat, float lng, float alt, 
        float *d_dlat, float *d_dlng)
{
    float theta, r;

    theta = 2.f * M_PI * (90.f - lat) / 360.f;
    r = RADIUS_OF_EARTH + alt;

    // See the differentiation section of
    // http://en.wikipedia.org/wiki/Spherical_coordinate_system

    // d/dv = d/dlat = -d/dtheta
    *d_dlat = (2.f * M_PI) * r / 360.f;

    // d/du = d/dlong = d/dphi
    *d_dlng = (2.f * M_PI) * r * sinf(theta) / 360.f;
}

static int 
_advance_one_timestep(wind_file_cache_t* cache, 
                      unsigned long delta_t,
                      unsigned long timestamp, unsigned long initial_timestamp,
                      unsigned int n_states, model_state_t* states)
{
    unsigned int i;

    for(i=0; i<n_states; ++i)
    {
        float ddlat, ddlng;
        float wind_v, wind_u;
        model_state_t* state = &(states[i]);

        if(!altitude_model_get_altitude(state->alt_model, 
                                        timestamp - initial_timestamp, &state->alt))
            return 0;

		if(!get_wind(cache, state->lat, state->lng, state->alt, timestamp, &wind_v, &wind_u)) {
            fprintf(stderr, "ERROR: error getting wind data\n");
			return 0;
		}

        _get_frame(state->lat, state->lng, state->alt, &ddlat, &ddlng);

        // NOTE: it this really the right thing to be doing? - think about what
        // happens near the poles

        state->lat += wind_v * delta_t / ddlat;
        state->lng += wind_u * delta_t / ddlng;
    }

    return 1;
}

int run_model(wind_file_cache_t* cache, altitude_model_t* alt_model,
              float initial_lat, float initial_lng, float initial_alt,
              long int initial_timestamp) 
{
    model_state_t state;

    state.alt = initial_alt;
    state.lat = initial_lat;
    state.lng = initial_lng;
    state.alt_model = alt_model;

    long int timestamp = initial_timestamp;
    
    int log_counter = 0; // only write position to output files every LOG_DECIMATE timesteps
    
	while(_advance_one_timestep(cache, TIMESTEP, timestamp, initial_timestamp, 1, &state))
    {
		if (log_counter == LOG_DECIMATE) {
            write_position(state.lat, state.lng, state.alt, timestamp);
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
            // Check to see if any of the other loaded enties are useful first
            int j;
            for(j=0; j<2; ++j) 
            {
                if(loaded_cache_entries[j] == found_entries[i]) 
                {
                    if(loaded_files[i]) 
                    {
                        wind_file_free(loaded_files[i]);
                    }
                    loaded_files[i] = loaded_files[j];
                    loaded_files[j] = NULL;
                    loaded_cache_entries[i] = loaded_cache_entries[j];
                    loaded_cache_entries[j] = NULL;
                }
            }

            // If we could still not find a loaded file, actually load one.
            if(loaded_cache_entries[i] != found_entries[i])
            {
                loaded_cache_entries[i] = found_entries[i];
                loaded_files[i] = wind_file_new(
                        wind_file_cache_entry_file_path(loaded_cache_entries[i]));
            }
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

