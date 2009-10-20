#!/usr/bin/env python

import numpy

TILE_SIZE = 6   # this must match the value in pred - wind_data.h
GRIB_RESOLUTION = 0.5

def download_grib(lat, lng, cycle_num, time_into_forecast, model_run_date, output_filename=None):
    """
    Download a GRIB tile from the NOMADS server. cycle_num is the time of the run during that day,
    i.e. 0, 6, 12 or 18. time_into_forecast must be a multiple of 3. model_run_date should be a string formatted "YYYYMMDD".
    Returns zero on success, and 404 when the GRIB is not available. Any other return code represents an error.
    """
    import urllib2
    
    if (cycle_num % 6) != 0 or cycle_num > 18:
        raise ValueError("download_grib: cycle_num must be 0, 6, 12 or 18")
        
    if (time_into_forecast % 3) != 0 or time_into_forecast > 180:
        raise ValueError("download_grib: time_into_forecast must be a multiple of 3 and < 180")
    
    model_run_string = "%s%.2d" % (model_run_date, cycle_num)
    
    if not output_filename:
        output_filename = "%s_%.2d_%d_%d.grb" % (model_run_string, time_into_forecast, lat, lng)
    
    url = "http://nomads.ncep.noaa.gov/cgi-bin/filter_gfs_hd.pl?file=gfs.t%.2dz.mastergrb2f%.2d&lev_1000_mb=on&"\
          "lev_100_mb=on&lev_10_mb=on&lev_150_mb=on&lev_200_mb=on&lev_20_mb=on&lev_250_mb=on&lev_300_mb=on&"\
          "lev_30_mb=on&lev_350_mb=on&lev_400_mb=on&lev_450_mb=on&lev_500_mb=on&lev_50_mb=on&lev_550_mb=on&"\
          "lev_600_mb=on&lev_650_mb=on&lev_700_mb=on&lev_70_mb=on&lev_750_mb=on&lev_800_mb=on&lev_850_mb=on&"\
          "lev_900_mb=on&lev_925_mb=on&lev_950_mb=on&lev_975_mb=on&var_HGT=on&var_UGRD=on&var_VGRD=on&subregion=&"\
          "leftlon=%d&rightlon=%d&toplat=%d&bottomlat=%d&dir=%%2Fgfs.%s%%2Fmaster" % \
          (cycle_num, time_into_forecast, lng, lng+TILE_SIZE, lat+TILE_SIZE, lat, model_run_string);

    try:
        remote_file = urllib2.urlopen(url)
    except urllib2.HTTPError, e:
        if e.code == 404:
            raise UserWarning("download_grib: GRIB file not available (HTTP returned 404)")
        else:
            raise IOError("download_grib: an error occured connecting to the remote GRIB file, HTTP error code: %s" % e.code)
    
    local_file = open(output_filename, 'wb')
    local_file.write(remote_file.read())
    
    local_file.close()
    remote_file.close()
    
    return output_filename

entry_type_lookup = {'HGT' : 0, 'UGRD' : 1, 'VGRD' : 2}

def read_wgrib2_txt(filename):
    wgrib2_csv = open(filename, "r")
    wind_data = dict()
    tile_lat = tile_lng = 0
    
    for line in wgrib2_csv.readlines():
        line = line.split(",")
        (entry_type, pressure_level, lng, lat, value) = line[2:]
        (lat, lng, value) = map(float, (lat, lng, value))
        pressure_level = int(pressure_level[1:-3])
        entry_type = entry_type[1:-1]
        
        if not wind_data.has_key(pressure_level):
            wind_data[pressure_level] = numpy.zeros((13,13,3))
            tile_lat = lat
            tile_lng = lng
            
        lat_index = int((lat - tile_lat)/GRIB_RESOLUTION)
        lng_index = int((lng - tile_lng)/GRIB_RESOLUTION)
        wind_data[pressure_level][lat_index][lng_index][entry_type_lookup[entry_type]] = value

    return wind_data
        
def write_wind_data_tile(wind_data, timestamp, lat, lng, filename):
    wind_data = {1:124, 2:2345}
    output_file = open(filename, 'w')
    # print header line
    output_file.write("6371229, 13, 13, %d, %d, %d, %d, %d" % (lat, lng, lat+TILE_SIZE, lng+TILE_SIZE, timestamp))
    # go through pressure levels
    for pressure, data in wind_data:
        for entry_type in range(3):
            output_file.write("%d, %d" % (entry_type-3, pressure))
            for lat_index, const_lat_row in enumerate(data):
                for lng_index, point in enumerate(const_lat_row):
                    output_file.write(", %.2g" % point[entry_type])
    output_file.close()
                
    
if __name__ == '__main__':
    import sys
    import os
    
    try:
        grib_filename = download_grib(48, 0, 6, 9, "20090607")
        os.system("./wgrib2 %s -csv %s" % (grib_filename, grib_filename + ".wgrib2_csv"))
        os.remove(grib_filename)
        data = read_wgrib2_txt(grib_filename + ".wgrib2_csv")
        write_wind_data_tile(data, 123456789, 48, 0, "testout.decoded_grib")
        os.remove(grib_filename + ".wgrib2_csv")
    except UserWarning, w:
        print w
    """
    if len(sys.argv) == 2:
        try:
            download(sys.argv[1])
        except IOError:
            print 'Filename not found.'
    else:
        import os
        print 'usage: %s http://server.com/path/to/filename' % os.path.basename(sys.argv[0])
    """
