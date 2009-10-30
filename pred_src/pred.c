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
#include <string.h>
#include <time.h>
#include <errno.h>

#include "gopt.h"
#include "run_model.h"
#include "pred.h"
#include "altitude.h"
#include "wind_files.h"

FILE* output;
FILE* kml_file;
const char* data_dir;
int verbosity;

int main(int argc, const char *argv[]) {
    
    const char* argument;
    
    long int initial_timestamp;
    float initial_lat, initial_lng, initial_alt;
    float burst_alt, ascent_rate, drag_coeff;
    int descent_mode;
    char* endptr;       // used to check for errors on strtod calls 
    
    wind_file_cache_t* file_cache;
    
    // configure command-line options parsing
    void *options = gopt_sort(&argc, argv, gopt_start(
        gopt_option('h', 0, gopt_shorts('h', '?'), gopt_longs("help")),
        gopt_option('z', 0, gopt_shorts(0), gopt_longs("version")),
        gopt_option('v', GOPT_REPEAT, gopt_shorts('v'), gopt_longs("verbose")),
        gopt_option('o', GOPT_ARG, gopt_shorts('o'), gopt_longs("output")),
        gopt_option('k', GOPT_ARG, gopt_shorts('k'), gopt_longs("kml")),
        gopt_option('t', GOPT_ARG, gopt_shorts('t'), gopt_longs("start_time")),
        gopt_option('i', GOPT_ARG, gopt_shorts('i'), gopt_longs("data_dir")),
        gopt_option('d', 0, gopt_shorts('d'), gopt_longs("descending"))
    ));

    if (gopt(options, 'h')) {
        // Help!
        // Print usage information
        printf("Usage: %s [options] initial_lat initial_lon initial_alt drag_coeff [burst_alt ascent_rate]\n", argv[0]);
        printf("Options:\n\n");
        printf(" -h --help               Display this information.\n");
        printf(" --version               Display version information.\n");
        printf(" -v --verbose            Display more information while running,\n");
        printf("                           Use -vv, -vvv etc. for even more verbose output.\n");
        printf(" -t --start_time <int>   Start time of model, defaults to current time.\n");
        printf("                           Should be a UNIX standard format timestamp.\n");
        printf(" -o --output <file>      Output file for CSV data, defaults to stdout.\n");
        printf(" -k --kml <file>         Output KML file.\n");
        printf(" -d --descending         We are in the descent phase of the flight, i.e. after\n");
        printf("                           burst or cutdown. burst_alt and ascent_rate ignored.\n");
        printf(" -i --data_dir <dir>     Input directory for wind data, defaults to current dir.\n\n");
      exit(0);
    }

    if (gopt(options, 'z')) {
      // Version information
      printf("Landing Prediction version: %s\nCopyright (c) CU Spaceflight 2009\n", VERSION);
      exit(0);
    }
    
    verbosity = gopt(options, 'v');
    
    if (gopt(options, 'd'))
        descent_mode = DESCENT_MODE_DESCENDING;
    else
        descent_mode = DESCENT_MODE_NORMAL;
    
    if (gopt_arg(options, 'o', &argument) && strcmp(argument, "-")) {
      output = fopen(argument, "wb");
      if (!output) {
        fprintf(stderr, "ERROR: %s: could not open CSV file for output\n", argument);
        exit(1);
      }
    }
    else
      output = stdout;
      
    if (gopt_arg(options, 'k', &argument) && strcmp(argument, "-")) {
      kml_file = fopen(argument, "wb");
      if (!kml_file) {
        fprintf(stderr, "ERROR: %s: could not open KML file for output\n", argument);
        exit(1);
      }
    }
    else
      kml_file = NULL;

    if (gopt_arg(options, 't', &argument) && strcmp(argument, "-")) {
      initial_timestamp = strtol(argument, &endptr, 0);
      if (endptr == argument) {
        fprintf(stderr, "ERROR: %s: invalid start timestamp\n", argument);
        exit(1);
      }
    } else {
      initial_timestamp = time(NULL);
    }
    
    if (!(gopt_arg(options, 'i', &data_dir) && strcmp(data_dir, "-")))
      data_dir = "./";

    // release gopt data, 
    gopt_free(options);

    // populate wind data file cache
    file_cache = wind_file_cache_new(data_dir);
    
    // write KML header
    if (kml_file)
        start_kml();

    // read in flight parameters, 6 usually, only 4 in descent mode
    if (descent_mode) {
        if ((argc-1) < 4) {
            fprintf(stderr, "ERROR: too few parameters (descent mode expects 4), run %s -h for usage information\n", argv[0]);
            exit(1);
        }
    } else {
        if ((argc-1) < 6) {
            fprintf(stderr, "ERROR: too few parameters (normal mode expects 6), run %s -h for usage information\n", argv[0]);
            exit(1);
        } else {
            burst_alt = strtod(argv[5], &endptr);
            if (endptr == argv[5] || burst_alt <= 0) {
                fprintf(stderr, "ERROR: %s: invalid burst altitude\n", argv[5]);
                exit(1);
            }
            ascent_rate = strtod(argv[6], &endptr);
            if (endptr == argv[6] || ascent_rate <= 0) {
                fprintf(stderr, "ERROR: %s: invalid ascent rate\n", argv[6]);
                exit(1);
            }
        }
    }
    initial_lat = strtod(argv[1], &endptr);
    if (endptr == argv[1]) {
        fprintf(stderr, "ERROR: %s: invalid initial latitude\n", argv[1]);
        exit(1);
    }
    initial_lng = strtod(argv[2], &endptr);
    if (endptr == argv[2]) {
        fprintf(stderr, "ERROR: %s: invalid initial longitude\n", argv[2]);
        exit(1);
    }
    initial_alt = strtod(argv[3], &endptr);
    if (endptr == argv[3] || initial_alt < 0) {
        fprintf(stderr, "ERROR: %s: invalid initial altitude\n", argv[3]);
        exit(1);
    }
    drag_coeff = strtod(argv[4], &endptr);
    if (endptr == argv[4] || drag_coeff <= 0) {
        fprintf(stderr, "ERROR: %s: invalid drag coefficient\n", argv[4]);
        exit(1);
    }
    
    {
        // do the actual stuff!!
        altitude_model_t* alt_model = altitude_model_new(descent_mode, burst_alt, 
                                                         ascent_rate, drag_coeff);
        if(!alt_model) {
                fprintf(stderr, "ERROR: error initialising altitude profile\n");
                exit(1);
        }

        if (!run_model(file_cache, alt_model, 
                       initial_lat, initial_lng, initial_alt, initial_timestamp)) {
                fprintf(stderr, "ERROR: error during model run!\n");
                exit(1);
        }

        altitude_model_free(alt_model);
    }
    
    // write footer to KML and close output files
    if (kml_file) {
        finish_kml();
        fclose(kml_file);
    }
    fclose(output);

    // release the file cache resources.
    wind_file_cache_free(file_cache);

    return 0;
}

void write_position(float lat, float lng, float alt, int timestamp) {
    if (kml_file) {
        fprintf(kml_file, "%g,%g,%g\n", lng, lat, alt);
        if (ferror(kml_file)) {
          fprintf(stderr, "ERROR: error writing to KML file\n");
          exit(1);
        }
    }
        
    fprintf(output, "%d,%g,%g,%g\n", timestamp, lat, lng, alt);
    if (ferror(output)) {
      fprintf(stderr, "ERROR: error writing to CSV file\n");
      exit(1);
    }
}

void start_kml() {
    FILE* kml_header;
    char c;
    
    kml_header = fopen("kml_header", "r");
    
    while (!feof(kml_header)) {
      c = fgetc(kml_header);
      if (ferror(kml_header)) {
        fprintf(stderr, "ERROR: error reading KML header file\n");
        exit(1);
      }
      if (!feof(kml_header)) fputc(c, kml_file);
      if (ferror(kml_file)) {
        fprintf(stderr, "ERROR: error writing header to KML file\n");
        exit(1);
      }
    }
    
    fclose(kml_header);
}

void finish_kml() {
    FILE* kml_footer;
    char c;
    
    kml_footer = fopen("kml_footer", "r");
    
    while (!feof(kml_footer)) {
      c = fgetc(kml_footer);
      if (ferror(kml_footer)) {
        fprintf(stderr, "ERROR: error reading KML footer file\n");
        exit(1);
      }
      if (!feof(kml_footer)) fputc(c, kml_file);
      if (ferror(kml_file)) {
        fprintf(stderr, "ERROR: error writing footer to KML file\n");
        exit(1);
      }
    }
    
    fclose(kml_footer);
}

