#!/usr/bin/env python

# Scan the list of downloaded GFS data and print the timestamps each file corresponds to.

import config, logging, glob, os

# logging.basicConfig(level = logging.INFO)

def next_non_comment_line(file):
    line = file.readline()
    while line[0] == '#':
        line = file.readline()
    return line

def main():
    logging.info('Scanning data directory %s' % config.LAND_PRED_DATA)
    data_files = glob.glob(os.path.join(config.LAND_PRED_DATA, '*.dat'))
    for filename in data_files:
        logging.info('File: %s' % filename)

        file = open(filename, 'r')
        header = next_non_comment_line(file).strip().split(',')
        timestamp = header[4]
        print '%s %s' % (timestamp, filename)

if(__name__ == '__main__'):
    main()

# vim:sw=4:ts=4:et:autoindent
