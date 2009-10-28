#!/usr/bin/env python

# Modules from the Python standard library.
import datetime, math, sys, logging, calendar, optparse

# We use Pydap from http://pydap.org/.
import pydap.exceptions, pydap.client, pydap.lib

# Use a client-side cache
pydap.lib.CACHE = 'gfs-cache/'

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
        metavar='TIME',
        default=calendar.timegm(datetime.datetime.utcnow().timetuple()))
    parser.add_option('-o', '--output', dest='output',
        help='file to write output to or \'-\' for stdout \t[default: stdout]',
        metavar='FILE',
        default='-')
    parser.add_option('-v', '--verbose', action='store_true', dest='verbose',
        help='be verbose \t[default: %default]', default=False)
    parser.add_option('-p', '--past', dest='past',
        help='number of hours in past to start saving data \t[default: %default]',
        metavar='HOURS',
        default='3')
    parser.add_option('-f', '--future', dest='future',
        help='number of hours in future to stop saving data \t[default: %default]',
        metavar='HOURS',
        default='6')

    (options, args) = parser.parse_args()

    if options.verbose:
        log.setLevel(logging.INFO)

    if options.output == '-':
        output_file = sys.stdout
    else:
        output_file = open(options.output, 'w')

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

    log.debug('      Latitude: %s -> %s' % (min(dataset.lat), max(dataset.lat)))
    log.debug('     Longitude: %s -> %s' % (min(dataset.lon), max(dataset.lon)))

    write_file(output_file, dataset, ('hgtprs', 'ugrdprs', 'vgrdprs'), \
        -2, 2, 50, 54, # FIXME: Don't hardcode these!
        time_to_find - datetime.timedelta(hours=int(options.past)),
        time_to_find + datetime.timedelta(hours=int(options.future)))

def write_file(output, data, vars, minlat, maxlat, minlon, maxlon, mintime, maxtime):
    for var in vars:
        grid = data[var]
        log.info('Processing variable \'%s\' with shape %s...' % (var, grid.shape))

        # Check the dimensions are what we expect.
        assert(grid.dimensions == ('time', 'lev', 'lat', 'lon'))

        times= map(timestamp_to_datetime, grid.maps['time'])

        for timeidx, time in enumerate(times):
            # If this is outside of our time window, skip the record.
            if (time < mintime) | (time > maxtime):
                continue

            timestamp = datetime_to_posix(time)
            log.info('   Downloading data for %s...' % time.ctime())
            selection = grid[timeidx,:,\
                (grid.lat >= minlat) & (grid.lat <= maxlat),\
                (grid.lon >= minlon) & (grid.lon <= maxlon)]
            log.debug('   Downloaded data has shape %s...', selection.shape)
            log.debug('   Writing data...')
            output.write('CHUNK START\n')
            output.write('NAME %s\n' % var)
            output.write('DATALINECOUNT %s\n' % reduce(lambda x, y: x * y, selection.shape))
            output.write('DATAFIELDS %s\n' % ','.join(('TIME','PRESSURE','LATITUDE','LONGITUDE','VALUE')))
            for pressureidx, pressure in enumerate(selection.maps['lev']):
                for latidx, latitude in enumerate(selection.maps['lat']):
                    for lonidx, longitude in enumerate(selection.maps['lon']):
                        record = ( timestamp, pressure, \
                                   latitude, longitude, \
                                   selection.array[pressureidx,latidx,lonidx] )
                        output.write('DATA %s\n' % ','.join(map(str,record)))
            output.write('CHUNK END\n')

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
