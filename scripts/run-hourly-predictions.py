#!/usr/bin/env python

# Run hourly predictions from now up until the point we have
# data for.

import config, site

# Add our local packages to the path
site.addsitedir(config.CUSF_PYTHON_PATH)

import logging, glob, os, time, datetime, uuid, subprocess, demjson, shutil

logging.basicConfig(level = logging.INFO)

script_root = os.path.dirname(os.path.abspath(__file__))

def relpath(target, base=os.curdir):
    """
    Return a relative path to the target from either the current dir or an optional base dir.
    Base can be a directory specified either as absolute or relative to current dir.
    """

    if not os.path.exists(target):
        raise OSError, 'Target does not exist: '+target

    if not os.path.isdir(base):
        raise OSError, 'Base is not a directory or does not exist: '+base

    base_list = (os.path.abspath(base)).split(os.sep)
    target_list = (os.path.abspath(target)).split(os.sep)

    # On the windows platform the target may be on a completely different drive from the base.
    if os.name in ['nt','dos','os2'] and base_list[0] <> target_list[0]:
        raise OSError, 'Target is on a different drive to base. Target: '+target_list[0].upper()+', base: '+base_list[0].upper()

    # Starting from the filepath root, work out how much of the filepath is
    # shared by base and target.
    for i in range(min(len(base_list), len(target_list))):
        if base_list[i] <> target_list[i]: break
    else:
        # If we broke out of the loop, i is pointing to the first differing path elements.
        # If we didn't break out of the loop, i is pointing to identical path elements.
        # Increment i so that in all cases it points to the first differing path elements.
        i+=1

    rel_list = [os.pardir] * (len(base_list)-i) + target_list[i:]
    return os.path.join(*rel_list)

def last_line_of(filename):
    file = open(filename, 'r')
    last_line = ''
    for line in file:
        last_line = line
    return last_line.strip()

def run_prediction(prediction_time, predictions_dir, scenario_template):
    # Create a UUID for the prediction
    pred_uuid = uuid.uuid4()

    # Create a directory for the predicition result.
    pred_root = os.path.join(predictions_dir, str(pred_uuid))
    os.mkdir(pred_root)

    scenario = demjson.decode(scenario_template)
    scenario['launch-time'] = {
            'year': prediction_time.year,
            'month': prediction_time.month,
            'day': prediction_time.day,
            'hour': prediction_time.hour,
            'minute': prediction_time.minute,
            'second': prediction_time.second,
        }

    logging.debug('Using scenario:')
    logging.debug(scenario)

    # Convert the scenario to an INI
    scenarioINI = []
    for cattitle, catcontents in scenario.iteritems():
        scenarioINI.append('[%s]' % cattitle)
        for key, value in catcontents.iteritems():
            scenarioINI.append('%s = %s' % (key, value))
    scenarioINI = '\n'.join(scenarioINI)

    logging.debug('Scenario INI:')
    logging.debug(scenarioINI)

    # Write scenario
    scenario_filename = os.path.join(pred_root, 'scenario.ini')
    scenario_file = open(scenario_filename, 'w')
    scenario_file.write(scenarioINI + '\n')
    scenario_file.close()

    scenario_json_filename = os.path.join(pred_root, 'scenario.json')
    scenario_json_file = open(scenario_json_filename, 'w')
    scenario_json_file.write(demjson.encode(scenario))
    scenario_json_file.close()

    # Where is the prediction app?
    pred_app = config.LAND_PRED_APP

    logging.info('Launching prediction application...')

    # Try to wire everything up
    output_filename = os.path.join(pred_root, 'output.csv')
    output_file = open(output_filename, 'w')
    logging_filename = os.path.join(pred_root, 'log.txt')
    logging_file = open(logging_filename, 'w')
    pred_process = subprocess.Popen( \
        (pred_app, '-v', '-i', config.LAND_PRED_DATA, scenario_filename),
        stdout=output_file, stderr=logging_file)
    pred_process.wait()
    if pred_process.returncode:
        raise RuntimeError('Prediction process %s returned error code: %s.' % (pred_uuid, pred_process.returncode))
    output_file.close()
    logging_file.close()

    # Find the last line of the output
    last_output = last_line_of(output_filename)
    logging.info('Final line of output: %s' % last_output)

    (final_timestamp, latitude, longitude, alt) = map(lambda x: float(x), last_output.split(','))
    final_timestamp = int(final_timestamp)

    logging.info('Parsed as ts=%s, lat=%s, lon=%s, alt=%s' %
        (final_timestamp, latitude, longitude, alt))
    final_time = datetime.datetime.utcfromtimestamp(final_timestamp)

    manifest_entry = {
        'landing-location': {
            'latitude': latitude,
            'longitude': longitude,
            'altitude': alt,
        },
        'landing-time': {
            'year': final_time.year,
            'month': final_time.month,
            'day': final_time.day,
            'hour': final_time.hour,
            'minute': final_time.minute,
            'second': final_time.second,
        },
        'launch-time': {
            'year': prediction_time.year,
            'month': prediction_time.month,
            'day': prediction_time.day,
            'hour': prediction_time.hour,
            'minute': prediction_time.minute,
            'second': prediction_time.second,
        },
    }

    return (str(pred_uuid), manifest_entry)

def next_non_comment_line(file):
    line = file.readline()
    while line[0] == '#':
        line = file.readline()
    return line

def get_latest_timestamp():
    logging.info('Scanning data directory %s' % config.LAND_PRED_DATA)
    data_files = glob.glob(os.path.join(config.LAND_PRED_DATA, '*.dat'))
    timestamps = []
    for filename in data_files:
        logging.debug('  - found file: %s' % filename)
        file = open(filename, 'r')
        header = next_non_comment_line(file).strip().split(',')
        timestamps.append(int(header[4]))
    return max(timestamps)

def main():
    start_stamp = int(time.time())
    end_stamp = get_latest_timestamp()

    start_time = datetime.datetime.utcfromtimestamp(start_stamp)
    end_time = datetime.datetime.utcfromtimestamp(end_stamp)

    logging.info('Running hourly predictions from %s to %s.' % (start_time.ctime(), end_time.ctime()))

    one_hour = datetime.timedelta(hours = 1)

    # Round prediction times down to hour boundaries
    predict_time = start_time.replace(minute = 0, second = 0, microsecond = 0)

    # Load the scenario template
    scenario_template = open(os.path.join(config.HOURLY_PREDICTIONS, 'scenario-template.json')).read()

    # Create a directory to save the data in, removing any previously existing one.
    pred_root = os.path.join(config.HOURLY_PREDICTIONS, 'new')
    if(os.path.isdir(pred_root)):
        shutil.rmtree(pred_root)
    os.mkdir(pred_root)

    # Where do we store the manifest file?
    manifest_filename = os.path.join(pred_root, 'manifest.json')
    manifest = {}

    manifest['scenario-template'] = demjson.decode(scenario_template)
    manifest['predictions'] = { }

    while predict_time < end_time:
        if predict_time >= start_time:
            logging.info('Run prediction for %s.' % predict_time.ctime())
            (uuid, entry) = run_prediction(predict_time, pred_root, scenario_template)

            # Record in manifest
            manifest['predictions'][uuid] = entry
            open(manifest_filename, 'w').write(demjson.encode(manifest))

            logging.info('Prediction finished. Sleeping to save CPU...')
            
            # Sleep so we don't use massive amounts of CPU
            # time.sleep(2)

        predict_time += one_hour

if(__name__ == '__main__'):
    main()

# vim:sw=4:ts=4:et:autoindent
