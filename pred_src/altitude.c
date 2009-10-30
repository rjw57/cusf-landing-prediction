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

#include <stdio.h>
#include <math.h>

#include "pred.h"
#include "altitude.h"
#include "run_model.h"

typedef struct altitude_model_s altitude_model_t;
struct altitude_model_s
{
    float   burst_altitude;
    float   ascent_rate;
    float   drag_coeff;
    int     descent_mode;

    float   initial_alt;
    int     burst_time;
};

static altitude_model_t _alt_singleton;

int init_altitude(int dec_mode, float burst_alt, float asc_rate, float drag_co) {
    // this function doesn't do anything much at the moment
    // but will prove useful in the future
    
    _alt_singleton.burst_altitude = burst_alt;
    _alt_singleton.ascent_rate = asc_rate;
    _alt_singleton.drag_coeff = drag_co;
    _alt_singleton.descent_mode = dec_mode;
    return 1;
}

int get_altitude(int time_into_flight, float* alt) {
    
    altitude_model_t* self = &(_alt_singleton);
    
    // TODO: this section needs some work to make it more flexible
    
    // time == 0 so setup initial altitude stuff
    if (time_into_flight == 0) {
        self->initial_alt = *alt;
        self->burst_time = (self->burst_altitude - self->initial_alt) / self->ascent_rate;
    }
    
    // If we are not doing a descending mode sim then start going up
    // at out ascent rate. The ascent rate is constant to a good approximation.
    if (self->descent_mode == DESCENT_MODE_NORMAL) 
        if (time_into_flight <= self->burst_time) {
            *alt = self->initial_alt + time_into_flight*self->ascent_rate;
            return 1;
        }
    
    // Descent - just assume its at terminal velocity (which varies with altitude)
    // this is a pretty darn good approximation for high-ish drag e.g. under parachute
    // still converges to T.V. quickly (i.e. less than a minute) for low drag.
    // terminal velocity = -drag_coeff/sqrt(get_density(*alt));
    *alt += TIMESTEP * -self->drag_coeff/sqrt(get_density(*alt));
    
    /*
    // Rob's method - is this just freefall until we reach terminal velocity?
	vertical_speed = -drag_coeff/sqrt(get_density(*alt));
	// TODO: inaccurate if still accelerating
	if (vertical_speed < previous_vertical_speed - G*TIMESTEP)    // still accelerating
		vertical_speed = previous_vertical_speed - G*TIMESTEP;
	if(vertical_speed > previous_vertical_speed + G*TIMESTEP)     // still accelerating
		vertical_speed = previous_vertical_speed + G*TIMESTEP;
	previous_vertical_speed = vertical_speed;
	*alt += TIMESTEP * vertical_speed;
	*/
    
    if (*alt <= 0)
        return 0;

    return 1;
    
}

float get_density(float altitude) {
    
    float temp, pressure;
    
    if (altitude > 25000) {
        temp = -131.21 + 0.00299 * altitude;
        pressure = 2.488*pow((temp+273.1)/216.6,-11.388);
    }
    if (altitude <=25000 && altitude > 11000) {
        temp = -56.46;
        pressure = 22.65 * exp(1.73-0.000157*altitude);
    }
    if (altitude <=11000) {
        temp = 15.04 - 0.00649 * altitude;
        pressure = 101.29 * pow((temp + 273.1)/288.08,5.256);
    }
    
    return pressure/(0.2869*(temp+273.1));
}
