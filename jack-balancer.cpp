/**JACK Balancer - A tool that lets you balance two JACK channels through a MIDI controller.
 * 
 * Compile with
 * g++ -Wall -o bin/jack-balancer jack-balancer.cpp -l jack
 * 
 * TODO:
 * - Be more verbose about connections etc.
 * - Format 'verbose' output better (i.e. always show the same amount of digits for floats)
 * - MIDI configuration
 *   - Properly cast input arguments to ints (also support hexadecimal values)
 *   - Make sure shorts are actually sufficient
 */

#include <iostream>
#include <cstring>
#include <cmath>

#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>

#include <jack/jack.h>
#include <jack/midiport.h>



#define CONTROLSTEPS 128

unsigned short VERBLEVEL = 0;	//0: default, 1: verbose, 2: debug
unsigned short CHANNEL = 0;
unsigned short VOLCONTROL = 7;
unsigned short BALCONTROL = 8;
double GAIN = 1.0;				// Default input amplification/attenuation
char* CLIENTNAME = (char*)malloc(64*sizeof(char));	//JACK client names can at most be 63 characters long

double factorL = -1.0;			// Initiated as -1.0 so first time execution of process() can be detected
double factorR = -1.0;


jack_port_t *output_port1, *output_port2;
jack_port_t *input_port1, *input_port2;
jack_port_t *midi_port;
jack_client_t *client;

// Precalculated factors and their indicies:
double volumeFcts[CONTROLSTEPS];
double balanceFctsL[CONTROLSTEPS];
double balanceFctsR[CONTROLSTEPS];
int balanceStep = CONTROLSTEPS / 2;
int volumeStep = CONTROLSTEPS;



// Handle signals
static void signal_handler(int sig) {
	jack_client_close(client);
	std::cerr << "signal received, exiting..." << std::endl;
	exit(0);
}


/**
 * JACK interface functions:
 */

int process (jack_nframes_t nframes, void *arg) {
	bool controlChange = false;

	// Handle MIDI events
	void* midiBuffer = jack_port_get_buffer (midi_port, nframes);
	assert(midiBuffer);
	jack_nframes_t nMidiEvents = jack_midi_get_event_count (midiBuffer);
	for (uint32_t i = 0; i < nMidiEvents; i++) {
		jack_midi_event_t midiEvent;

		if(jack_midi_event_get (&midiEvent, midiBuffer, i))
			continue;
		
		uint8_t buffer[4096];
		memcpy(buffer, midiEvent.buffer, midiEvent.size);

		uint8_t type = buffer[0] & 0xf0;
		uint8_t channel = buffer[0] & 0xf;

		if (type == 0xb0 && channel == CHANNEL) {
			uint16_t controlNr = buffer[1];
			uint16_t value = buffer[2];

			if (controlNr == VOLCONTROL) {
				volumeStep = value;

				controlChange = true;
			} else if (controlNr == BALCONTROL) {
				balanceStep = value;

				controlChange = true;
			}

			if (VERBLEVEL == 2)
				printf ("Control event: %01x %01x  %02x %02x\n", type, channel, controlNr, value);
		}

	}

	// Handle audio
	if (controlChange || factorL == -1.0) {
		factorL = volumeFcts[volumeStep] * balanceFctsL[balanceStep] * GAIN;
		factorR = volumeFcts[volumeStep] * balanceFctsR[balanceStep] * GAIN;
		if (VERBLEVEL >= 1) {
			std::cout << "Vol: " << volumeStep;
			std::cout << ", PanStep: " << balanceStep;
			std::cout << " => " << factorL << "/" << factorR << std::endl;
		}
	}


	jack_default_audio_sample_t *out1, *out2, *in1, *in2;
	out1 = (jack_default_audio_sample_t*)jack_port_get_buffer (output_port1, nframes);
	out2 = (jack_default_audio_sample_t*)jack_port_get_buffer (output_port2, nframes);
	in1 = (jack_default_audio_sample_t*)jack_port_get_buffer (input_port1, nframes);
	in2 = (jack_default_audio_sample_t*)jack_port_get_buffer (input_port2, nframes);

	for (uint32_t i = 0; i < nframes; i++) {
		out1[i] = factorL * in1[i];
		out2[i] = factorR * in2[i];
	}

	return 0;
}

void jack_shutdown (void *arg) {
	exit (1);
}


/**
 * Main application function
 */

int main (int argc, char *argv[]) {
	CLIENTNAME[63] = 0x0;
	memccpy(CLIENTNAME, "jack-balancer", 0, 64);

	//Read in values from command line:
	for (int a = 1; a < argc; a++) {
		std::string arg = argv[a];
		//auto *assignTarget = nullptr;
		void *assignTarget;
		std::string targetName;
		if (arg == "-h" || arg == "--help" || arg == "-help") {
			std::cout << "Control balance and volume of two JACK channels." << std::endl;
			std::cout << "\nAvailable arguments:" << std::endl;
			std::cout << "  -h             -  Show help" << std::endl;
			std::cout << "  -v [level]     -  Set verbosity level (0: default, 1: verbose, 2: debug)" << std::endl;
			std::cout << "  -c [channel]   -  Set MIDI channel (default: 0)" << std::endl;
			std::cout << "  -vc [control]  -  Set control for volume (default: 7)" << std::endl;
			std::cout << "  -bc [control]  -  Set control for balance (default: 8)" << std::endl;
			std::cout << "  -n [name]      -  Set JACK client name; max. 63 characters (default: 'jack-balancer')" << std::endl;
			std::cout << "  -g [factor]    -  Set gain factor, i.e. 0.1 for 90 % attenuation (default: 1.0)" << std::endl;
			exit(0);
		} else if (arg == "-v") {
			assignTarget = &VERBLEVEL;
			targetName = "verbosity level";
		} else if (arg == "-c") {
			assignTarget = &CHANNEL;
			targetName = "channel";
		} else if (arg == "-vc") {
			assignTarget = &VOLCONTROL;
			targetName = "volume control";
		} else if (arg == "-bc") {
			assignTarget = &BALCONTROL;
			targetName = "balance control";
		} else if (arg == "-n") {
			assignTarget = &CLIENTNAME;
			targetName = "client name";
		} else if (arg == "-g") {
			assignTarget = &GAIN;
			targetName = "gain";
		} else {
			std::cerr << "Unknown argument '" + arg + "'!" << std::endl;
			assignTarget = nullptr;
			exit(1);
		}
		
		if (assignTarget != nullptr) {
			if (a < (argc-1)) {
				char* value = argv[++a];
				if (VERBLEVEL == 2)
					std::cout << "Parsing argument " << a << " which is " << value << std::endl;
				try {
					if(assignTarget == &GAIN) {
						//Cast *char to double:
						double *castAssignTarget = static_cast<double*>(assignTarget);
						double castResult = std::stod(value);
						if(castResult >= 0.0) {
							*castAssignTarget = castResult;
							std::cout << "Set " << targetName << " to " << *castAssignTarget << std::endl;
						} else {
							std::cerr << "'" << value << "' is not a valid value for " << targetName << "!" << std::endl;
							exit(1);
						}
					} else if (assignTarget == &CLIENTNAME) {
						//Copy *char to *char:
						char* *castAssignTarget = static_cast<char**>(assignTarget);
						memccpy(*castAssignTarget, value, 0, 63);
						std::cout << "Set " << targetName << " to '" << *castAssignTarget << "'" << std::endl;
					} else {
						//Cast *char to int:
						unsigned short *castAssignTarget = static_cast<unsigned short*>(assignTarget);
						short castResult = std::stoi(value);
						if(castResult >= 0) {	//NOTE: Channel values > 15 or Controller numbers > 127 are invalid but will not be detected with this. Oh well...
							*castAssignTarget = static_cast<unsigned short>(castResult);
							std::cout << "Set " << targetName << " to " << *castAssignTarget << std::endl;
						} else {
							std::cerr << "'" << value << "' is not a valid value for " << targetName << "!" << std::endl;
							exit(1);
						}
					}
				} catch (...) {
					std::cerr << "Error while assigning value '" << value << "'!" << std::endl;
					exit(1);
				}
			} else {
				std::cerr << "Missing value for last parameter!" << std::endl;
				exit(1);
			}

		}
		
	}

	//Calculate values of volume function:
	if (VERBLEVEL == 2)
		std::cout << "\n=== Values of volume function ===" << std::endl;
	for (int i = 0; i < CONTROLSTEPS; i++) {
		double balanceFactor = (static_cast<double>(i)) / 127;

		double val = (std::pow(10.0f, balanceFactor) - 1) / 9;
		if (val > 1.0)
			val = 1.0;

		volumeFcts[i] = val;

		if (VERBLEVEL == 2)
			printf ("%03d:    %.6f\n", i, volumeFcts[i]);
	}
	if(VERBLEVEL >= 1)
		std::cout << "Calculated values of volume function." << std::endl;

	if (VERBLEVEL == 2)
		std::cout << "\n=== Values of volume function ===" << std::endl;
	//Calculate values of balance function:
	for (int i = 0; i < CONTROLSTEPS; i++) {
		double balanceFactor = 2.0 * (static_cast<double>(i)/127 - 0.5);	//Values from -1.0f (left) to 1.0f (right)
		
		//Values from 0.0f (silent) to 1.0f (gain 0.0dB):
		double valLeftUnscaled = -balanceFactor * std::pow(balanceFactor, 2) / 2 + 0.5;
		double valRightUnscaled = 1.0 - valLeftUnscaled;

		//Scale values so that louder channel will always have 0.0 dB gain:
		double scalingFactor = 1.0;
		if (balanceFactor >= 0.0) {
			scalingFactor = 1.0 / valRightUnscaled;
		} else {
			scalingFactor = 1.0 / valLeftUnscaled;
		}

		balanceFctsL[i] = scalingFactor * valLeftUnscaled;
		balanceFctsR[i] = scalingFactor * valRightUnscaled;

		if (VERBLEVEL == 2)
			printf ("%03d:    L: %.6f    R: %.6f\n", i, balanceFctsL[i], balanceFctsR[i]);
	}
	if(VERBLEVEL >= 1)
		std::cout << "Calculated values of balance function." << std::endl;
	
	if(VERBLEVEL >= 1 && GAIN != 1.0)
		std::cout << "Gain: " << GAIN << std::endl;


	// Announce ourselves as a new JACK client
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;
	client = jack_client_open (CLIENTNAME, options, &status, server_name);
	if (client == NULL) {
		fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			std::cerr << "Unable to connect to JACK server!" << std::endl;
		}
		exit (1);
	}
	if (status & JackServerStarted) {
		std::cout << "JACK server started." << std::endl;
	}
	if (status & JackNameNotUnique) {
		const char* clientNameNew = jack_get_client_name(client);
		memccpy(CLIENTNAME, clientNameNew, 0, 63);
		std::cerr << "unique name '" << CLIENTNAME << "' assigned!" << std::endl;
	}

	// Define JACK interface functions
	jack_set_process_callback (client, process, 0);
	jack_on_shutdown (client, jack_shutdown, 0);

	// Create ports
	output_port1 = jack_port_register (client, "output1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	output_port2 = jack_port_register (client, "output2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
	input_port1 = jack_port_register (client, "input1", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	input_port2 = jack_port_register (client, "input2", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	midi_port = jack_port_register (client, "control", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);

	if ((output_port1 == NULL) || (output_port2 == NULL) || (input_port1 == NULL) || (input_port2 == NULL) || (midi_port == NULL)) {
		std::cerr << "No more JACK ports available!" << std::endl;
		exit (1);
	}

	
	// Start running
	if (jack_activate (client)) {
		std::cerr << "Cannot activate client!" << std::endl;
		exit (1);
	}
	std::cout << "Client running..." << std::endl;

	// Install a signal handler to properly quits jack client
	signal(SIGQUIT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);


	// Keep running until Ctrl+C
	while (1) {
		sleep (1);
	}

	jack_client_close (client);
	exit (0);
}
