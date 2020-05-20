# jack-balancer
A JACK client that provides rudimentary volume and balance control via MIDI controls.

## Usage
jack-balancer listens for the following command line arguments:
```
-h             -  Show help
-v [level]     -  Set verbosity level (0: default, 1: verbose, 2: debug)
-c [channel]   -  Set MIDI channel (default: 0)
-vc [control]  -  Set control for volume (default: 7)
-bc [control]  -  Set control for balance (default: 8)
```

## Building
Requirements: A functional JACK installation, or at least `jack.h`
### Using the Makefile:
This project includes a makefile that automates the building and installation process.
```
make
sudo make install
```
By default, `jack-balancer` will be installed into `/usr/local/bin`, this can changed by setting `$PREFIX` to the desired directory when using `make install`.
### Building with GCC (standalone):
    g++ -Wall -o bin/jack-balancer jack-balancer.cpp -l jack
So far I have only used this application on Arch Linux with jack2, I probably cannot help you if something doesn't work on your machine. Sorry.

## TODO
- Be more verbose about connections etc.
- Fix 'Unique name assigned' output
- Format 'verbose' output better (i.e. always show the same amount of digits for floats)
- MIDI configuration
  - Properly cast input arguments to ints (also support hexadecimal values)
  - Make sure shorts are actually sufficient