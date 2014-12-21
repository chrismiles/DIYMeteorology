#!/usr/bin/env python
# encoding: utf-8

'''DIYMeteorologyReceiver.py - read meteorological data from serial port device.
'''

__author__ = 'Chris Miles'
__copyright__ = '(c) Chris Miles 2014. All rights reserved.'
__license__ = 'MIT http://opensource.org/licenses/mit-license.php'
__version__ = '0.0.1'


import csv
import optparse
import os
import serial
import sys


def parse_fields_THGR122NX(fields):
    channel = int(fields[0])
    temperature = float(fields[1])
    humidity = float(fields[2])

    print "THGR122NX: channel=%d temperature=%f˚C humidity=%f%%" %(channel, temperature, humidity) #DEBUG


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

        if row[1] == 'THGR122NX':
            parse_fields_THGR122NX(row[2:])


def decode_line(line):
    if line.startswith("D,"):
        parse_data_line(line.strip())

def serial_data(port, baudrate):
    ser = serial.Serial(port, baudrate)
    while True:
        yield ser.readline()
    ser.close()


def read_from_serial(port, baudrate):
    for line in serial_data(port, baudrate):
        print line.strip() #DEBUG
        decode_line(line)


def main(argv=None):
    if argv is None:
        argv = sys.argv

    # define usage and version messages
    usageMsg = "usage: %s [options] <serial port>" % argv[0]
    versionMsg = """%s %s""" % (os.path.basename(argv[0]), __version__)
    description = """Read meteorological data from device via serial port."""

    # get a parser object and define our options
    parser = optparse.OptionParser(usage=usageMsg, version=versionMsg, description=description)

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
    parser.add_option('-b', '--serialbaud', dest='serialbaud',
        metavar="SERIAL_BAUD", default=115200,
        help="Specify serial baud rate for connecting with device.")

    # Parse options & arguments
    (options, args) = parser.parse_args(argv[1:])


    port = '/dev/tty.usbmodem1421'
    baud = 115200

    read_from_serial('/dev/tty.usbmodem1421', 115200)


if __name__ == "__main__":
    sys.exit(main())
