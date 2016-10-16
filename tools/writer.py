import argparse
import serial

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Serial port writer')
    parser.add_argument('device', type=str, nargs='?')
    parser.add_argument('baudrate', type=int, nargs='?', default=9600)
    args = parser.parse_args()

    ser = serial.Serial(args.device, args.baudrate)

    try:
        while True:
            print '%s>' % args.device,
            try:
                ser.write(raw_input())
            except serial.serialutil.SerialException:
                print 'Error: write failure'
    except KeyboardInterrupt:
        print 'Quitting...'

    ser.close()

