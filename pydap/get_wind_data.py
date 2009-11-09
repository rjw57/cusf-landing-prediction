#!/usr/bin/env python

# Modules from the Python standard library.
import datetime, math, sys, os, logging, calendar, optparse

# We use Pydap from http://pydap.org/.
import pydap.exceptions, pydap.client, pydap.lib

# Attempt to cache the downloaded data.
pydap.lib.CACHE = os.path.dirname(__file__) + '/pydap-cache/'

# Output logger format
log = logging.getLogger('main')
console = logging.StreamHandler()
console.setFormatter(logging.Formatter('%(levelname)s: %(message)s'))
log.addHandler(console)

def main():
    """
    The main program routine.
    """

    # Set up our command line options
    parser = optparse.OptionParser()
    parser.add_option('-t', '--timestamp', dest='timestamp',
        help='search for dataset covering the POSIX timestamp TIME \t[default: now]', 
        metavar='TIME', type='int',
        default=calendar.timegm(datetime.datetime.utcnow().timetuple()))
    parser.add_option('-o', '--output', dest='output',
        help='file to write output to with \'%(VAR)\' replaced with the the value of VAR [default: %default]',
        metavar='FILE',
        default='gfs/gfs_%(time)_%(lat)_%(lon)_%(latdelta)_%(londelta).dat')
    parser.add_option('-v', '--verbose', action='count', dest='verbose',
        help='be verbose. The more times this is specified the more verbose.', default=False)
    parser.add_option('-p', '--past', dest='past',
        help='window of time to save data is at most HOURS hours in past [default: %default]',
        metavar='HOURS',
        type='int', default=3)
    parser.add_option('-f', '--future', dest='future',
        help='window of time to save data is at most HOURS hours in future [default: %default]',
        metavar='HOURS',
        type='int', default=9)

    group = optparse.OptionGroup(parser, "Location specifiers",
        "Use these options to specify a particular window of data to download.")
    group.add_option('--lat', dest='lat',
        help='window centre latitude in range (-90,90) degrees north [default: %default]',
        metavar='DEGREES',
        type='float', default=52)
    group.add_option('--lon', dest='lon',
        help='window centre longitude in degrees east [default: %default]',
        metavar='DEGREES',
        type='float', default=0)
    group.add_option('--latdelta', dest='latdelta',
        help='window radius in latitude in degrees [default: %default]',
        metavar='DEGREES',
        type='float', default=6)
    group.add_option('--londelta', dest='londelta',
        help='window radius in longitude in degrees [default: %default]',
        metavar='DEGREES',
        type='float', default=6)
    parser.add_option_group(group)

    (options, args) = parser.parse_args()

    # Check the latitude is in the right range.
    if (options.lat < -90) | (options.lat > 90):
        log.error('Latitude %s is outside of the range (-90,90).')
        sys.exit(1)

    # Check the delta sizes are valid.
    if (options.latdelta <= 0.5) | (options.londelta <= 0.5):
        log.error('Latitiude and longitude deltas must be at least 0.5 degrees.')
        sys.exit(1)

    if options.londelta > 180:
        log.error('Longitude window sizes greater than 180 degrees are meaningless.')
        sys.exit(1)

    # We need to wrap the longitude into the right range.
    options.lon = canonicalise_longitude(options.lon)

    # How verbose are we being?
    if options.verbose > 0:
        log.setLevel(logging.INFO)
    if options.verbose > 1:
        log.setLevel(logging.DEBUG)

    log.debug('Using cache directory: %s' % pydap.lib.CACHE)

    timestamp_to_find = options.timestamp
    time_to_find = datetime.datetime.utcfromtimestamp(timestamp_to_find)

    log.info('Looking for latest dataset which covers %s' % time_to_find.ctime())
    try:
        dataset = dataset_for_time(time_to_find)
    except: 
        print('Could not locate a dataset for the requested time.')
        sys.exit(1)

    dataset_times = map(timestamp_to_datetime, dataset.time)
    dataset_timestamps = map(datetime_to_posix, dataset_times)

    log.info('Found appropriate dataset:')
    log.info('    Start time: %s (POSIX %s)' % \
        (dataset_times[0].ctime(), dataset_timestamps[0]))
    log.info('      End time: %s (POSIX %s)' % \
        (dataset_times[-1].ctime(), dataset_timestamps[-1]))

    log.info('      Latitude: %s -> %s' % (min(dataset.lat), max(dataset.lat)))
    log.info('     Longitude: %s -> %s' % (min(dataset.lon), max(dataset.lon)))

    window = (options.lat, options.latdelta, options.lon, options.londelta)

    write_file(options.output, dataset, \
        window, \
        time_to_find - datetime.timedelta(hours=options.past), \
        time_to_find + datetime.timedelta(hours=options.future))

def write_file(output_format, data, window, mintime, maxtime):
    log.info('Downloading data in window (lat, lon) = (%s +/- %s, %s +/- %s).' % window)
    downloaded_data = { }
    for var in ('hgtprs', 'ugrdprs', 'vgrdprs'):
        grid = data[var]
        log.info('Processing variable \'%s\' with shape %s...' % (var, grid.shape))

        # Check the dimensions are what we expect.
        assert(grid.dimensions == ('time', 'lev', 'lat', 'lon'))

        # Work out what times we want to download
        times = filter(lambda x: (x >= mintime) & (x <= maxtime),
            map(timestamp_to_datetime, grid.maps['time']))

        start_time = min(times)
        end_time = max(times)
        log.info('   Downloading from %s to %s.' % (start_time.ctime(), end_time.ctime()))

        # Download the data. Unfortunately, OpeNDAP only supports remote access of
        # contiguous regions. Since the longitude data wraps, we may require two 
        # windows. The 'right' way to fix this is to download a 'left' and 'right' window
        # and munge them together. In terms of download speed, however, the overhead of 
        # downloading is so great that it is quicker to download all the longitude 
        # data in our slice and do the munging afterwards.
        selection = grid[\
            times.index(start_time):(times.index(end_time)+1), \
            :, \
            (grid.lat >= (window[0]-window[1])) & (grid.lat <= (window[0]+window[1])), \
            : ]

        # Cache the downloaded data for later
        downloaded_data[var] = selection

        log.info('   Downloaded data has shape %s...', selection.shape)
        if len(selection.shape) != 4:
            log.error('    Expected 4-d data. Perhaps your time window is too short?')
            return

    # Check all the downloaded data has the same shape
    target_shape = downloaded_data['hgtprs']
    assert( all( map( lambda x: x == target_shape, 
        filter( lambda x: x.shape, downloaded_data.itervalues() ) ) ) )

    log.info('Writing output...')

    hgtprs = downloaded_data['hgtprs']
    ugrdprs = downloaded_data['ugrdprs']
    vgrdprs = downloaded_data['vgrdprs']

    # Filter the longitudes we're actually going to use.
    longitudes = filter(lambda x: longitude_distance(x[1], window[2]) <= window[3] ,
                        enumerate(hgtprs.maps['lon']))

    # Filter the latitudes we're actually going to use.
    latitudes = filter(lambda x: math.fabs(x[1] - window[0]) <= window[1] ,
                        enumerate(hgtprs.maps['lat']))

    log.debug('Using longitudes: %s' % (map(lambda x: x[1], longitudes),))

    # Write one file for each time index.
    for timeidx, time in enumerate(hgtprs.maps['time']):
        timestamp = datetime_to_posix(timestamp_to_datetime(time))

        output_filename = output_format
        output_filename = output_filename.replace('%(time)', str(timestamp))
        output_filename = output_filename.replace('%(lat)', str(window[0]))
        output_filename = output_filename.replace('%(latdelta)', str(window[1]))
        output_filename = output_filename.replace('%(lon)', str(window[2]))
        output_filename = output_filename.replace('%(londelta)', str(window[3]))

        log.info('   Writing \'%s\'...' % output_filename)
        output = open(output_filename, 'w')

        # Write the header.
        output.write('# window centre latitude, window latitude radius, window centre longitude, window longitude radius, POSIX timestamp\n')
        header = window + (timestamp,)
        output.write(','.join(map(str,header)) + '\n')

        # Write the axis count.
        output.write('# num_axes\n')
        output.write('3\n') # FIXME: HARDCODED!

        # Write each axis, a record showing the size and then one with the values.
        output.write('# axis 1: pressures\n')
        output.write(str(hgtprs.maps['lev'].shape[0]) + '\n')
        output.write(','.join(map(str,hgtprs.maps['lev'][:])) + '\n')
        output.write('# axis 2: latitudes\n')
        output.write(str(hgtprs.maps['lat'].shape[0]) + '\n')
        output.write(','.join(map(str,hgtprs.maps['lat'][:])) + '\n')
        output.write('# axis 3: longitudes\n')
        output.write(str(len(longitudes)) + '\n')
        output.write(','.join(map(lambda x: str(x[1]), longitudes)) + '\n')

        # Write the number of lines of data.
        output.write('# number of lines of data\n')
        output.write('%s\n' % (hgtprs.shape[1] * hgtprs.shape[2] * len(longitudes)))

        # Write the number of components in each data line.
        output.write('# data line component count\n')
        output.write('3\n') # FIXME: HARDCODED!

        # Write the data itself.
        output.write('# now the data in axis 3 major order\n')
        output.write('# data is: '
                     'geopotential height [gpm], u-component wind [m/s], '
                     'v-component wind [m/s]\n')
        for pressureidx, pressure in enumerate(hgtprs.maps['lev']):
            for latidx, latitude in enumerate(hgtprs.maps['lat']):
                for lonidx, longitude in longitudes:
                    record = ( hgtprs.array[timeidx,pressureidx,latidx,lonidx], \
                               ugrdprs.array[timeidx,pressureidx,latidx,lonidx], \
                               vgrdprs.array[timeidx,pressureidx,latidx,lonidx] )
                    output.write(','.join(map(str,record)) + '\n')

def canonicalise_longitude(lon):
    """
    The GFS model has all longitudes in the range 0.0 -> 359.5. Canonicalise
    a longitude so that it fits in this range and return it.
    """
    lon = math.fmod(lon, 360)
    if lon < 0.0:
        lon += 360.0
    assert((lon >= 0.0) & (lon < 360.0))
    return lon

def longitude_distance(lona, lonb):
    """
    Return the shortest distance in degrees between longitudes lona and lonb.
    """
    distances = ( \
        math.fabs(lona - lonb),  # Straightforward distance
        360 - math.fabs(lona - lonb), # Other way 'round.
    )
    return min(distances)

def datetime_to_posix(time):
    """
    Convert a datetime object to a POSIX timestamp.
    """
    return calendar.timegm(time.timetuple())

def timestamp_to_datetime(timestamp):
    """
    Convert a GFS fractional timestamp into a datetime object representing 
    that time.
    """
    # The GFS timestmp is a floating point number fo days from the epoch,
    # day '0' appears to be January 1st 1 AD.

    (fractional_day, integer_day) = math.modf(timestamp)

    # Unfortunately, the datetime module uses a different epoch.
    ordinal_day = int(integer_day - 1)

    # Convert the integer day to a time and add the fractional day.
    return datetime.datetime.fromordinal(ordinal_day) + \
        datetime.timedelta(days = fractional_day)

def possible_urls(time):
    """
    Given a datetime object representing a date and time, return a list of
    possible data URLs which could cover that period.

    The list is ordered from latest URL (i.e. most likely to be correct) to
    earliest.

    We assume that a particular data set covers a period of P days and
    hence the earliest data set corresponds to time T - P and the latest
    available corresponds to time T given target time T.
    """

    period = datetime.timedelta(days = 7.5)

    earliest = time - period
    latest = time

    url_format = 'http://nomads.ncep.noaa.gov:9090/dods/gfs_hd/gfs_hd%i%02i%02i/gfs_hd_%02iz'

    # Start from the latest, work to the earliest
    proposed = latest
    possible_urls = []
    while proposed >= earliest:
        for offset in ( 18, 12, 6, 0 ):
            possible_urls.append(url_format % \
                (proposed.year, proposed.month, proposed.day, offset))
        proposed -= datetime.timedelta(days = 1)
    
    return possible_urls

def dataset_for_time(time):
    """
    Given a datetime object, attempt to find the latest dataset which covers that 
    time and return pydap dataset object for it.
    """

    url_list = possible_urls(time)

    for url in url_list:
        try:
            log.debug('Trying dataset at %s.' % url)
            dataset = pydap.client.open_url(url)

            start_time = timestamp_to_datetime(dataset.time[0])
            end_time = timestamp_to_datetime(dataset.time[-1])

            if start_time <= time and end_time >= time:
                log.info('Found good dataset at %s.' % url)
                return dataset
        except pydap.exceptions.ServerError:
            # Skip server error.
            pass
    
    raise RuntimeError('Could not find appropriate dataset.')

# If this is being run from the interpreter, run the main function.
if __name__ == '__main__':
    main()

# vim:sw=4:ts=4:et:autoindent
