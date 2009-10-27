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
#include <stdlib.h>
#include <dirent.h>

#include "pred.h"
#include "altitude.h"
#include "wind_data.h"
#include "run_model.h"

extern char* data_dir;
extern int verbosity;

int load_data(int lat, int lng, long int timestamp, wind_data** w_data, long int* current_tile_timestamp) {
    
    FILE* data_file;
    float radius_of_earth, lat1, lat2, lng1, lng2;
    int u_size, v_size, time_period; 
    long int start_timestamp, first_start_timestamp;
    int i, j, count, row_type, pressure;
    float value;
    
    // if w_data is a valid pointer it probably points to a previously
    // malloc'd bit of memory holding old GRIB data, so best free it now

    if (*w_data)
        free(*w_data);

    // get ourselves some memory to put the data in
    wind_data* data = (wind_data*)malloc(sizeof(wind_data)); 
    if (!data) {
        fprintf(stderr, "ERROR: unable to allocate memory for wind data file\n");
        return 0;
    }

    // point the data pointer in the calling function to our new malloc'd memory area
    *w_data = data; 

    for (time_period=0; time_period<2; time_period++) {
        
        if (!(data_file = find_grib_file(lat, lng, timestamp + time_period*GRIB_TIME_PERIOD))) {
            fprintf(stderr, "ERROR: unable to find or open wind data file\n");
            return 0;
        }

        // read in first line of file containing information on the region covered by the data file
        // lat1,lng1 etc. contain the limits og the dataset
        count = fscanf(data_file, "%g,%d,%d,%g,%g,%g,%g,%ld\n", &radius_of_earth, &u_size, &v_size, &lat1, &lng1, &lat2, &lng2, &start_timestamp);
        if (count != 8) {
            fprintf(stderr, "ERROR: error parseing wind data file header\n");
            return 0;
        }
        
        if (verbosity>1)
            fprintf(stderr, "wind data file header:\n\tRadius of Earth: %g\n\tSize: %d,%d\n\tBounds: %g,%g - %g,%g\n\tStart timestamp: %ld\n", radius_of_earth, u_size, v_size, lat1, lng1, lat2, lng2, start_timestamp);
        
        // check the size is what we are set up for
        if (u_size != GRIB_POINTS || v_size != GRIB_POINTS) {
            fprintf(stderr, "ERROR: wind data file is not the expected size (%d,%d instead of %d,%d points)\n", u_size, v_size, GRIB_POINTS, GRIB_POINTS);
            return 0;
        }
    
        // check that the boundaries and size are consistent with the resolution we are expecting
        if ((lat2 - lat1)/(v_size-1) != GRIB_RESOLUTION || (lng2 - lng1)/(u_size-1) != GRIB_RESOLUTION) {
            fprintf(stderr, "ERROR: wind data file is not the expected resolution\n");
            return 0;
        }
        
        // check that the tile covers the requested area
        if (lat1 != lat || lng1 != lng) {
            fprintf(stderr, "ERROR: wind data file doesn't cover the requested area.\n");
            return 0;
        }
        
        // check that our required timestamp falls within the GRIB period and that the GRIB for the
        // second time period really does follow on from the first
        if (time_period == 0) {
            first_start_timestamp = start_timestamp; // we need this later so make a note of it
            if (timestamp < start_timestamp || timestamp > (start_timestamp + GRIB_TIME_PERIOD)) {
                fprintf(stderr, "ERROR: wind data file does not cover the expected time period\n");
                return 0;
            }
        } else
            if (start_timestamp - GRIB_TIME_PERIOD != first_start_timestamp) {
                fprintf(stderr, "ERROR: second wind data file does not follow on from the first\n");
                return 0;
            } else {
                // update start timestamp of current tile passed to calling function
                *current_tile_timestamp = first_start_timestamp;
            }
            
        // read in main body of data
    	for (j=0; j<3*NUM_PRESSURE_LEVELS; j++) {
    	    
    		// row types: -1 is UGRD (wind_u), -2 is VGRD (wind_v), -3 is altitude
            //count = fscanf(data_file, "%d", &row_type);
            //count = fscanf(data_file, ", %d", &pressure);
            row_type = pressure = 42;
            count = fscanf(data_file, "%d, %d, ",  &row_type, &pressure);
            if (count != 2) {
                fprintf(stderr, "ERROR: error parsing wind data file, couldn't determine entry type\n");
                return 0;
            }
        
    		for (i=0; i<GRIB_POINTS*GRIB_POINTS; i++) {
                count = fscanf(data_file, "%g, ", &value);
                if (count != 1) {
                    fprintf(stderr, "ERROR: error parsing wind data file\n");
                    return 0;
                }
    			switch (row_type) {
    			    case -3: // altitude
    			        (*data)[i/GRIB_POINTS][i % GRIB_POINTS][j / 3][time_period].alt = value;
                        break;
    			    case -2: // wind_v
    			        (*data)[i/GRIB_POINTS][i % GRIB_POINTS][j / 3][time_period].wind_v = value;
                        break;
    		        case -1: // wind_u
    			        (*data)[i/GRIB_POINTS][i % GRIB_POINTS][j / 3][time_period].wind_u  = value;
                        break;
                    default:
                        fprintf(stderr, "ERROR: wind data file entry with unknown type %d\n", row_type);
                        return 0;
    		    }
    		}
    	}
    	fclose(data_file);
    }
    
    return 1;
}

int wind_data_cleanup(wind_data* w_data) {
    // free previously allocated wind data memory
    if (w_data)
        free(w_data);
    return 1;
}


FILE* find_grib_file(int lat, int lng, long int timestamp) {
    DIR *dp;
    struct dirent *ep;
    
    int file_lat, file_lng, file_timestamp;
    char temp[100];
    
    // open the data directory
    dp = opendir(data_dir);
    if (!dp) {
        fprintf(stderr, "ERROR: %s: data directory not found\n", data_dir);
        return 0;
    }
    
    while (ep = readdir(dp)) {
        sscanf(ep->d_name, "%d_%d_%d.decoded_grib", &file_lat, &file_lng, &file_timestamp);
        if (file_lat == lat && file_lng == lng && file_timestamp < timestamp && (file_timestamp+GRIB_TIME_PERIOD) > timestamp) {
            if (verbosity)
                fprintf(stderr, "opening wind data file: %s%s\n", data_dir, ep->d_name);
            sprintf(temp, "%s%s", data_dir, ep->d_name);
            return fopen(temp, "r");
        }
    }
    return 0;
}

