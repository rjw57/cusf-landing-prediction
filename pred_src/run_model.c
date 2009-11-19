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
#include <stdlib.h>
#include <assert.h>

#include "wind/wind_file.h"
#include "util/random.h"
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
    double              loglik;
};

// get the wind values in the u and v directions at a point in space and time from the dataset data
// we interpolate lat, lng, alt and time. The GRIB data only contains pressure levels so we first
// determine which pressure levels straddle to our desired altitude and then interpolate between them
static int get_wind(
        wind_file_t* past, wind_file_t* future,
        float lat, float lng, float alt, long int timestamp, float* wind_v, float* wind_u, float *wind_var);
// note: get_wind will likely call load_data and load a different tile into data, so just be careful that data could be pointing
// somewhere else after running get_wind

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
_advance_one_timestep(wind_file_t* past,
                      wind_file_t* future,
                      unsigned long delta_t,
                      unsigned long timestamp, unsigned long initial_timestamp,
                      unsigned int n_states, model_state_t* states,
                      float rmserror)
{
    unsigned int i;

    for(i=0; i<n_states; ++i)
    {
        float ddlat, ddlng;
        float wind_v, wind_u, wind_var;
        float u_samp, v_samp, u_lik = 0.f, v_lik = 0.f;
        model_state_t* state = &(states[i]);

        if(!altitude_model_get_altitude(state->alt_model, 
                                        timestamp - initial_timestamp, &state->alt))
        {
            state->alt = 0.f;
            continue;
        }

        if(!get_wind(past, future, 
                     state->lat, state->lng, state->alt, timestamp, 
                     &wind_v, &wind_u, &wind_var)) 
        {
                fprintf(stderr, "ERROR: error getting wind data\n");
                return 0;
        }

        _get_frame(state->lat, state->lng, state->alt, &ddlat, &ddlng);

        // NOTE: it this really the right thing to be doing? - think about what
        // happens near the poles

        wind_var += rmserror * rmserror;

        assert(wind_var >= 0.f);

        //fprintf(stderr, "U: %f +/- %f, V: %f +/- %f\n",
        //        wind_u, sqrtf(wind_u_var),
        //        wind_v, sqrtf(wind_v_var));

#if 0
        u_samp = random_sample_normal(wind_u, wind_var, &u_lik);
        v_samp = random_sample_normal(wind_v, wind_var, &v_lik);
#else
        u_samp = wind_u;
        v_samp = wind_v;
#endif

        state->lat += v_samp * delta_t / ddlat;
        state->lng += u_samp * delta_t / ddlng;

        state->loglik += (double)(u_lik + v_lik);
    }

    return 1;
}

/*
static int _state_compare_rev(const void* a, const void *b)
{
    model_state_t* sa = (model_state_t*)a;
    model_state_t* sb = (model_state_t*)b;

    // this returns a value s.t. the states will be sorted so that
    // the maximum likelihood state is at position 0.
    return sb->loglik - sa->loglik;
}
*/

/* Run the models from the specified timestamp up until the timestamp of future */
/* Return the timestamp we should start from next time. */
static long int _run_model_internal(
        wind_file_t* past, wind_file_t* future,
        long int initial_timestamp,  /* For altitude model */
        long int start_timestamp,
        long int stop_timestamp,
        altitude_model_t* alt_model, float rmswinderror,
        model_state_t* states, unsigned int n_states)
{
    long int timestamp = start_timestamp;
    
    int log_counter = 0; // only write position to output files every LOG_DECIMATE timesteps
    
    while(timestamp <= stop_timestamp)
    {
        _advance_one_timestep(past, future, TIMESTEP, timestamp, initial_timestamp, 
                n_states, states, rmswinderror);

        // write a particular state out
        if (log_counter == LOG_DECIMATE) {
            write_position(states[0].lat, states[0].lng, states[0].alt, timestamp);
            log_counter = 0;
        }

        log_counter++;
        timestamp += TIMESTEP;
    }

    /*
    int i;
    for(i=0; i<n_states; ++i) 
    {
        model_state_t* state = &(states[i]);
        write_position(state->lat, state->lng, state->alt, timestamp);
    }
    */

    fprintf(stderr, "INFO: Stop model.\n");


    return timestamp;
}

int run_model(wind_file_cache_t* cache, altitude_model_t* alt_model,
              float initial_lat, float initial_lng, float initial_alt,
              long int initial_timestamp, float rmswinderror) 
{
    wind_file_cache_entry_t *past_entry = NULL, *future_entry = NULL;
    wind_file_t *past_file = NULL, *future_file = NULL;

    model_state_t* states;
    const unsigned int n_states = 300;
    unsigned int i, all_at_zero, run_count;
    long int timestamp = initial_timestamp;

    // Initialise all the states 
    states = (model_state_t*) malloc( sizeof(model_state_t) * n_states );
    for(i=0; i<n_states; ++i) 
    {
        model_state_t* state = &(states[i]);

        state->alt = initial_alt;
        state->lat = initial_lat;
        state->lng = initial_lng;
        state->alt_model = alt_model;
        state->loglik = 0.f;
    }

    all_at_zero = 0;
    run_count = 0;
    while(!all_at_zero && (run_count < 5)) {
        wind_file_cache_entry_t *new_past_entry = NULL, *new_future_entry = NULL;

        // look for two wind files which matches this latitude and longitude
        // but are in past and future respectively
        wind_file_cache_find_entry(cache, initial_lat, initial_lng, timestamp,
                &new_past_entry, &new_future_entry);

        if(!new_future_entry || !new_past_entry) {
            fprintf(stderr, "ERROR: Failed to find wind cache entries in range.\n");
            return 0;
        }

        if(past_entry)
            wind_file_cache_entry_release_file(past_entry);

        // Optimisation: if new past is old future, don't release, effectively swap.
        if((new_past_entry != future_entry) && future_entry) 
            wind_file_cache_entry_release_file(future_entry);

        // Set the new past and future entries/files.
        past_entry = new_past_entry;
        future_entry = new_future_entry;
        past_file = wind_file_cache_entry_file(past_entry);
        future_file = wind_file_cache_entry_file(future_entry);

        // Run inside this time window
        timestamp = _run_model_internal(past_file, future_file,
                initial_timestamp, 
                timestamp, wind_file_cache_entry_timestamp(future_entry),
                alt_model, rmswinderror,
                states, n_states);

        // See if we're all at zero yet
        all_at_zero = 1;
        for(i=0; all_at_zero && (i<n_states); ++i) 
        {
            model_state_t* state = &(states[i]);
            if(state->alt > 10.f) { /* 10m == 0 :) */
                all_at_zero = 0;
            }
        }

        ++run_count;
    }

    wind_file_cache_entry_release_file(past_entry);
    wind_file_cache_entry_release_file(future_entry);

    free(states);

    return 1;
}

int get_wind(
        wind_file_t* past,
        wind_file_t* future,
        float lat, float lng, float alt, long int timestamp,
        float* wind_v, float* wind_u, float *wind_var) 
{
    int i;
    float lambda, wu_l, wv_l, wu_h, wv_h;
    float wuvar_l, wvvar_l, wuvar_h, wvvar_h;
    unsigned int earlier_ts, later_ts;

    earlier_ts = wind_file_get_timestamp(past);
    later_ts = wind_file_get_timestamp(future);

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

    wind_file_get_wind(past, lat, lng, alt, &wu_l, &wv_l, &wuvar_l, &wvvar_l);
    wind_file_get_wind(future, lat, lng, alt, &wu_h, &wv_h, &wuvar_h, &wvvar_h);

    *wind_u = lambda * wu_h + (1.f-lambda) * wu_l;
    *wind_v = lambda * wv_h + (1.f-lambda) * wv_l;

    // flatten the u and v variances into a single mean variance for the
    // magnitude.
    *wind_var = 0.5f * (wuvar_h + wuvar_l + wvvar_h + wvvar_l);

    return 1;
}

// vim:sw=4:ts=4:et:cindent
