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

#include "ini/iniparser.h"
#include "util/gopt.h"
#include "wind/wind_file_cache.h"

#include "run_model.h"
#include "pred.h"
#include "altitude.h"

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
    dictionary*        scenario = NULL;
    
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
        printf("Usage: %s [options] [scenario file]\n", argv[0]);
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
        printf("The scenario file is an INI-like file giving the launch scenario. If it is\n");
        printf("omitted, the scenario is read from standard input.\n");
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

    // read in flight parameters
    if(argc > 2) {
        fprintf(stderr, "ERROR: too many parameters, run %s -h for usage information\n", argv[0]);
        exit(1);
    }

    if(argc == 1) {
        fprintf(stderr, "ERROR: scenarios from stdin not yet supported, sorry.\n");
        exit(1);
    }

    scenario = iniparser_load(argv[1]);
    if(!scenario) {
        fprintf(stderr, "ERROR: could not parse scanario file.\n");
        exit(1);
    }

    if(verbosity > 1) {
        fprintf(stderr, "INFO: Parsed scenario file:\n");
        iniparser_dump_ini(scenario, stderr);
    }

    // The observant amongst you will notice that there are default values for
    // *all* keys. This information should not be spread around too well.
    // Unfortunately, this means we lack some error checking.

    initial_lat = iniparser_getdouble(scenario, "launch-site:latitude", 0.0);
    initial_lng = iniparser_getdouble(scenario, "launch-site:longitude", 0.0);
    initial_alt = iniparser_getdouble(scenario, "launch-site:altitude", 0.0);

    ascent_rate = iniparser_getdouble(scenario, "altitude-model:ascent-rate", 1.0);

    // The 1.1045 comes from a magic constant buried in
    // ~cuspaceflight/public_html/predict/index.php.
    drag_coeff = iniparser_getdouble(scenario, "altitude-model:descent-rate", 1.0) * 1.1045;

    burst_alt = iniparser_getdouble(scenario, "altitude-model:burst-altitude", 1.0);

    if(verbosity > 0) {
        fprintf(stderr, "INFO: Scenario loaded:\n");
        fprintf(stderr, "    - Initial latitude  : %lf deg N\n", initial_lat);
        fprintf(stderr, "    - Initial longitude : %lf deg E\n", initial_lng);
        fprintf(stderr, "    - Initial altitude  : %lf m above sea level\n", initial_alt);
        fprintf(stderr, "    - Drag coeff.       : %lf\n", drag_coeff);
        if(!descent_mode) {
            fprintf(stderr, "    - Ascent rate       : %lf m/s\n", ascent_rate);
            fprintf(stderr, "    - Burst alt.        : %lf m\n", burst_alt);
        }
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

    // selease the scenario
    iniparser_freedict(scenario);

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

