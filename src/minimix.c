/*

	minimix.c
	Copyright (C) 2005  Nicholas J. Humfrey
	
	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.
	
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	
	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>

#include <getopt.h>
#include "config.h"
#include "db.h"
#include "minimix.h"
#include "osc.h"
#include "websocket.h"



jack_port_t *outport[2] = {NULL, NULL};
jack_port_t *monoport = NULL;
jack_client_t *client = NULL;
 
bool verbose = false;
bool quiet = false;
bool running = true;
bool mono = false;
unsigned int channel_count = DEFAULT_CHANNEL_COUNT;
jmm_channel_t *channels = NULL;


static
void signal_handler (int signum)
{
	if (!quiet) {
		switch(signum) {
			case SIGTERM:	fprintf(stderr, "Got termination signal.\n"); break;
			case SIGINT:	fprintf(stderr, "Got interupt signal.\n"); break;
		}
	}
	running = false;
}




static
void shutdown_callback_jack(void *arg)
{
	running = false;
}


static
int process_jack_audio(jack_nframes_t nframes, void *arg)
{
	jack_default_audio_sample_t *out_left =
		jack_port_get_buffer(outport[0], nframes);
	jack_default_audio_sample_t *out_right =
		jack_port_get_buffer(outport[1], nframes);

	jack_default_audio_sample_t *out_mono;
	jack_nframes_t n=0;
	int ch;

	if (mono) out_mono = jack_port_get_buffer(monoport, nframes);
	
	// Put silence into the outputs
	for ( n=0; n<nframes; n++ ) {
		out_left[ n ] = 0;
		out_right[ n ] = 0;
	}

	// Mix each input into the output buffer
	for ( ch=0; ch < channel_count ; ch++ ) {
		if (channels[ch].mute == true) continue;
		float mix_gain;
		jack_default_audio_sample_t *in_left =
			jack_port_get_buffer(channels[ch].left_port, nframes);
		jack_default_audio_sample_t *in_right =
			jack_port_get_buffer(channels[ch].right_port, nframes);
		
		// Adjust the current gain towards desired gain ?
		if (channels[ch].current_gain != channels[ch].desired_gain) {
			float fade_step = (GAIN_FADE_RATE / jack_get_sample_rate( client )) * nframes;
			if (channels[ch].current_gain < channels[ch].desired_gain-fade_step) {
				channels[ch].current_gain += fade_step;
			} else if (channels[ch].current_gain > channels[ch].desired_gain+fade_step) {
				channels[ch].current_gain -= fade_step;
			} else {
				channels[ch].current_gain = channels[ch].desired_gain;
			}
		}
		
		// Mix the audio
		mix_gain = db2lin( channels[ch].current_gain );
		for ( n=0; n<nframes; n++ ) {
			out_left[ n ] += (in_left[ n ] * mix_gain);
			out_right[ n ] += (in_right[ n ] * mix_gain);

			if (mono) out_mono[n] = (  (in_left[n] + in_right[n]) * mix_gain ) / 2;
		}
		
	}

	return 0;
}




static
void init_jack( const char * client_name ) 
{
	jack_status_t status;

	// Register with Jack
	if ((client = jack_client_open(client_name, JackNullOption, &status)) == 0) {
		fprintf(stderr, "Failed to start jack client: %d\n", status);
		exit(1);
	}
	if (!quiet) printf("JACK client registered as '%s'.\n", jack_get_client_name( client ) );

	// Create our pair of output ports
	if (!(outport[0] = jack_port_register(client, "out_left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0))) {
		fprintf(stderr, "Cannot register output port 'out_left'.\n");
		exit(1);
	}
	
	if (!(outport[1] = jack_port_register(client, "out_right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0))) {
		fprintf(stderr, "Cannot register output port 'out_right'.\n");
		exit(1);
	}

	if (mono) {
		if (verbose) printf("creating mono output\n");
		if (!(monoport = jack_port_register(client, "out_mono", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0))) {
			fprintf(stderr, "Cannot register output port 'out_mono'.\n");
			exit(1);
		}

	}
	
	// Register shutdown callback
	jack_on_shutdown (client, shutdown_callback_jack, NULL );

	// Register the peak audio callback
	jack_set_process_callback(client, process_jack_audio, 0);

}

static
void connect_jack_port( jack_port_t *port, const char* in )
{
	const char* out = jack_port_name( port );
	int err;
		
	if (!quiet) printf("Connecting %s to %s\n", out, in);
	
	if ((err = jack_connect(client, out, in)) != 0) {
		fprintf(stderr, "connect_jack_port(): failed to jack_connect() ports: %d\n",err);
		exit(1);
	}
}


// crude way of automatically connecting up jack ports
static
void autoconnect_jack_ports( jack_client_t* client )
{
	const char **all_ports;
	unsigned int ch=0;
	int i;

	// Get a list of all the jack ports
	all_ports = jack_get_ports(client, NULL, NULL, JackPortIsInput);
	if (!all_ports) {
		fprintf(stderr, "autoconnect_jack_ports(): jack_get_ports() returned NULL.");
		exit(1);
	}
	
	// Step through each port name
	for (i = 0; all_ports[i]; ++i) {
		
		// Connect the port
		connect_jack_port( outport[ch], all_ports[i] );
		
		// Found enough ports ?
		if (++ch >= 2) break;
	}
	
	free( all_ports );
}


static
void finish_jack( jack_client_t *client )
{
	// Leave the Jack graph
	jack_client_close(client);
}


static
jack_port_t* create_input_port( const char* side, int chan_num )
{
	int port_name_size = jack_port_name_size();
	char *port_name = malloc( port_name_size );
	jack_port_t *port;
	
	snprintf( port_name, port_name_size, "in%d_%s", chan_num+1, side );
	port = jack_port_register(client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
	if (!port) {
		fprintf(stderr, "Cannot register input port '%s'.\n", port_name);
		exit(1);
	}
	
	return port;
}

static
jmm_channel_t* init_channels( int chan_count ) 
{
	jmm_channel_t* channels = (jmm_channel_t*)malloc( 
			sizeof(jmm_channel_t) * chan_count);
	int c;
	
	// Initialise each of the channels
	for(c=0; c<chan_count; c++) {
	
		snprintf( channels[c].label, CHANNEL_LABEL_LEN, "Channel %d", c+1 );
		
		if (c==0) {
			// Channel 1 starts with gain of 0
			channels[c].current_gain=0.0f;
			channels[c].desired_gain=0.0f;
		} else {
			// Other channels start faded down
			channels[c].current_gain=-90.0f;
			channels[c].desired_gain=-90.0f;
		}
		
		// Create the JACK input ports
		channels[c].left_port = create_input_port( "left", c );
		channels[c].right_port = create_input_port( "right", c );
		channels[c].mute = false;
	}
			
	return channels;
}

static
void finish_channels( jmm_channel_t* channels )
{

}

/* Display how to use this program */
static
int usage( )
{
	printf("JackMiniMix version %s\n\n", PACKAGE_VERSION);
	printf("Usage: %s [options] [<channel label> ...]\n", PACKAGE_NAME);
	printf("   -a            Automatically connect our output JACK ports\n");
	printf("   -l <port>     Connect left output to this input port\n");
	printf("   -r <port>     Connect right output to this input port\n");
	printf("   -c <count>    Number of input channels (default 4)\n");
	printf("   -p <port>     Set the UDP port number for OSC\n");
	printf("   -w <port>     Set the TCP port for websocket\n");
	printf("   -n <name>     Name for this JACK client (default minimix)\n");
	printf("   -m            Create a mono out\n");
	printf("   -v            Enable verbose mode\n");
	printf("   -q            Enable quiet mode\n");
	printf("\n");
	exit(1);
}


int main(int argc, char *argv[])
{
	lo_server_thread server_thread = NULL;
	int autoconnect = 0;
	char *client_name = DEFAULT_CLIENT_NAME;
	char *connect_left = NULL;
	char *connect_right = NULL;
	char* osc_port = NULL;
	int i,opt,ws_port;
	
	
	// Parse the command line arguments
	while ((opt = getopt(argc, argv, "al:r:c:n:p:w:vqhm")) != -1) {
		switch (opt) {
			case 'a':  autoconnect = 1; break;
			case 'l':  connect_left = optarg; break;
			case 'r':  connect_right = optarg; break;
			case 'c':  channel_count = atoi(optarg); break;
			case 'n':  client_name = optarg; break;
			case 'p':  osc_port = optarg; break;
			case 'v':  verbose = true; break;
			case 'q':  quiet = true; break;
			case 'm':  mono = true; break;
			case 'w':  ws_port = atoi(optarg); break;
			default:
				fprintf(stderr, "Unknown option '%c'.\n", (char)opt);
			case 'h':
				usage();
				break;
		}
	}
    argc -= optind;
    argv += optind;
	
	// Check parameters
	if (channel_count<1) usage();
		
	// Validate parameters
	if (quiet && verbose) {
    	fprintf(stderr, "Can't be quiet and verbose at the same time.\n");
    	usage();
	}

	// Dislay welcoming message
	if (verbose) printf("Starting JackMiniMix version %s with %d channels.\n",
							VERSION, channel_count);

	// Set signal handlers
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);


	// Setup JACK
	init_jack( client_name );

	// Create the channel descriptors
	channels = init_channels( channel_count );
	
	// Label the channels
	for(i=0; i<argc && i<channel_count; i++) {
		strncpy( channels[i].label, argv[i], CHANNEL_LABEL_LEN );
	}

	// Set JACK running
	if (jack_activate(client)) {
		fprintf(stderr, "Cannot activate client.\n");
		exit(1);
	}

	// Auto-connect our output ports ?
	if (autoconnect) autoconnect_jack_ports( client );
	if (connect_left) connect_jack_port( outport[0], connect_left );
	if (connect_right) connect_jack_port( outport[1], connect_right );


	// Setup OSC
	if (osc_port) server_thread = init_osc( osc_port );

	// Setup Websocket
	if (ws_port) init_websocket( ws_port );


	// Sleep until we are done (work is done in threads)
	while (running) {
		usleep(1000);
	}
	
	
	// Cleanup
	if (osc_port) finish_osc( server_thread );
	finish_jack( client );
	finish_channels( channels );

	return 0;
}

