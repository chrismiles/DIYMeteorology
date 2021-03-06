#!/usr/bin/env python
# encoding: utf-8

'''DIYMeteorologyReceiver.py - read meteorological data from serial port device.
'''

__author__ = 'Chris Miles'
__copyright__ = '(c) Chris Miles 2014-2015. All rights reserved.'
__license__ = 'MIT http://opensource.org/licenses/mit-license.php'
__version__ = '0.0.1'


import csv
import httplib
import optparse
import os
import serial
import sys


config = None


def publish_temperature_to_data_sparkfun(channel, temperature, humidity):
    # http://data.sparkfun.com/input/[publicKey]?private_key=[privateKey]&humidity=[value]&temperature=[value]

    public_key = None
    private_key = None

    channel_cfg = config['channels'][channel]
    if channel_cfg:
        public_key = channel_cfg['sf_public']
        private_key = channel_cfg['sf_private']

    if public_key and private_key:
        host = "data.sparkfun.com"
        path = "/input/%s?private_key=%s&humidity=%f&temperature=%f" %(public_key, private_key, humidity, temperature)
        print "http://%s%s" %(host, path)

        conn = httplib.HTTPConnection(host)
        conn.request("GET", path)
        r1 = conn.getresponse()
        print(r1.status, r1.reason)
        conn.close()

    else:
        print "WARNING: no public or private key set for data.sparkfun temperature channel %d" %(channel,)


def parse_fields_THGR122NX(fields):
    channel = int(fields[0])
    temperature = float(fields[1])
    humidity = float(fields[2])

    return (channel, temperature, humidity)


def parse_data_line(line):
    csv_reader = csv.reader( (line,) )
    for row in csv_reader:
        print "row:",row #DEBUG
        if len(row) < 3:
            print "WARNING: data line ignored due to containing not enough fields: %s" %(row,)
            continue

        if row[0] != 'D':
            print "WARNING: data line ignored due to not beginning with 'D' field: %s" %(row,)
            continue

        deviceType = row[1]
        if deviceType == 'THGR122NX' or deviceType == 'RTGR328N':
            (channel, temperature, humidity) = parse_fields_THGR122NX(row[2:])
            print "%s: channel=%d temperature=%f˚C humidity=%f%%" %(deviceType, channel, temperature, humidity) #DEBUG

            try:
                publish_temperature_to_data_sparkfun(channel, temperature, humidity)
            except:
                print "!!! Exception caught while publishing to data.sparkfun: %s" %(sys.exc_info(),)


def decode_line(line):
    if line.startswith("D,"):
        parse_data_line(line.strip())


def serial_data(port, baudrate):
    ser = serial.Serial(port, baudrate)
    while True:
        yield ser.readline()
    ser.close()


def read_from_serial(port, baudrate):
    """

    :rtype : None
    """
    print "Reading from serial port '%s' (%d baud)" %(port, baudrate)
    for line in serial_data(port, baudrate):
        print line.strip() #DEBUG
        try:
            decode_line(line)
        except:
            print "!!! Exception caught while reading line: %s\n%s" %(line,sys.exc_info()[0])


def load_config(filename):
    config_globals = {}
    config_locals = {}
    execfile(filename, config_globals, config_locals)
    print "config_locals: ", config_locals
    return config_locals


def main(argv=None):
    if argv is None:
        argv = sys.argv

    # define usage and version messages
    usage_msg = "usage: %s [options] <config file>" % argv[0]
    version_msg = """%s %s""" % (os.path.basename(argv[0]), __version__)
    description = """Read meteorological data from device via serial port."""

    # get a parser object and define our options
    parser = optparse.OptionParser(usage=usage_msg, version=version_msg, description=description)

    # Switches
    # parser.add_option('-v', '--verbose', dest='verbose',
    #     action='store_true', default=False,
    #     help="verbose output")
    # parser.add_option('-d', '--debug', dest='debug',
    #     action='store_true', default=False,
    #     help="debugging output (very verbose)")
    # parser.add_option('-q', '--quiet', dest='quiet',
    #     action='store_true', default=False,
    #     help="suppress output (excluding errors)")

    # Options expecting values
    parser.add_option('-p', '--serialport',
                      dest='serialport',
                      metavar="SERIAL_PORT",
                      default="/dev/tty.usbmodem1421",
                      help="Specify serial port device for connecting with device (e.g. /dev/tty.usbmodem3d11).")
    parser.add_option('-b', '--serialbaud',
                      dest='serialbaud',
                      metavar="SERIAL_BAUD",
                      default=115200,
                      help="Specify serial baud rate for connecting with device (e.g. 115200).")

    # Parse options & arguments
    (options, args) = parser.parse_args(argv[1:])

    if len(args) < 1:
        parser.error("Not enough arguments. You must specify a config file.")

    global config
    config = load_config(args[0])

    port = options.serialport  # '/dev/tty.usbmodem1421'
    baud = int(options.serialbaud)  # 115200

    read_from_serial(port, baud)


if __name__ == "__main__":
    sys.exit(main())
