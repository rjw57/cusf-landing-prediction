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

#ifndef __WIND_DATA_H__
#define __WIND_DATA_H__

#define TILE_SIZE 6         // size of square tiles of GRIB data in degrees
// note, tiles are labeled by their southern and westerly most corner, i.e. round lat and lng down
// to nearest multiple of tile size to get the label of the tile that contains that point

#define GRIB_RESOLUTION 0.5         // number or degrees between data points in GRIB file
#define GRIB_POINTS_PER_DEGREE 2    // this must be set to 1/GRIB_RESOLUTION, just to keep compiler happy when working with arrays
#define GRIB_TIME_PERIOD (3600*3)   // length of GRIB time period in seconds
#define NUM_PRESSURE_LEVELS 26      // number of pressure levels in GRIB file
#define GRIB_POINTS (TILE_SIZE*GRIB_POINTS_PER_DEGREE+1)
// we have a +1 here as we need to include the data points on all the edges of our tile for interpolation,
// without it you would just have the lat = 0 and lng = 0 edges, not the far ones.

// this struct holds one data point from a GRIB dataset
typedef struct {
    float alt;
    float wind_u;
    float wind_v;
} wind_data_entry;

// for convenience typedef the wind data array to a convenient name
// NOTE: the format is:
//      wind_data[lat_index][lng_index][pressure_level][time]
// time can be either 0 or 1 as we load in two datasets at a time so we can interpolate between times
typedef wind_data_entry wind_data[GRIB_POINTS][GRIB_POINTS][NUM_PRESSURE_LEVELS][2];
// we have a +1 on the lat and lng as we need to include the data points on all the edges of our tile for interpolation

// load a tile of wind data and modify data to point to the dataset
int load_data(int lat, int lng, long int timestamp, wind_data** data, long int* current_tile_timestamp);

// look through data_dir and try and find a GRIB file for lat, lng which covers the time timestamp.
// the return value is the data file or zero if no file could be opened.
FILE* find_grib_file(int lat, int lng, long int timestamp);

// call before exiting to clean up any unfreed memory etc
int wind_data_cleanup(wind_data* w_data);

#endif // __WIND_DATA_H__

