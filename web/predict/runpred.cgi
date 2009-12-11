#!/usr/bin/env python 

import cgi, cgitb, os, sys, subprocess, site
cgitb.enable()

# Locate the script directory and, hence, the prediction app
SCRIPT_ROOT = os.path.abspath(os.path.dirname(__file__))
CONFIG_PATH = os.path.abspath(os.path.join(SCRIPT_ROOT, '../../scripts'))
sys.path.append(CONFIG_PATH)

# Import the location of the prediction apps, etc.
import config

# Add our local packages to the path
site.addsitedir(config.CUSF_PYTHON_PATH)

import demjson

def form_to_ini(form):
    """Take the form submitted and convert it to an INI file."""
    ini_dict = { }

    for k in form.keys():
        v = form[k].value
        sec_key = k.split(':')
        if len(sec_key) != 2:
            continue
        if not sec_key[0] in ini_dict:
            ini_dict[sec_key[0]] = { sec_key[1]: v }
        else:
            ini_dict[sec_key[0]][sec_key[1]] = v 

    ini = ''
    for (k,v) in ini_dict.iteritems():
        ini = ini + '[%s]\n' % k
        for (k2,v2) in v.iteritems():
            ini = ini + '%s = %s\n' % (k2,v2)
    
    return ini

def main():
    form = cgi.FieldStorage()
    ini = form_to_ini(form)

    # Try to wire everything up
    pred_process = subprocess.Popen( \
        (config.LAND_PRED_APP, '-v', '-i', config.LAND_PRED_DATA),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, stdin=subprocess.PIPE)
    (output, log) = pred_process.communicate(ini)

    parsed_output = []
    for output_line in output.split('\n'):
        if len(output_line) == 0:
            continue

        output_fields = output_line.rstrip().split(',')
        if len(output_fields) < 2:
            continue

        parsed_output.append(output_fields)

    output_dict = {
        'log': filter(lambda x: x.rstrip(), log.split('\n')),
        'output': parsed_output,
    }
    output_json = demjson.encode(output_dict)

    print('Content-Type: application/json\n')
    print(output_json)

if __name__ == '__main__':
    main()

# vim:sw=4:ts=4:autoindent:et
