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
#include <string.h>
#include <lo/lo.h>

#include "minimix.h"

static
int osc_ping_handler(const char *path, const char *types, lo_arg **argv, int argc,
	lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int result;

	// Display the address the ping came from
	if (verbose) {
		char *url = lo_address_get_url(src);
		printf( "Got ping from: %s\n", url);
		free(url);
	}

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/pong", "" );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

    return 0;
}

static
int osc_wildcard_handler(const char *path, const char *types, lo_arg **argv, int argc,
	lo_message msg, void *user_data)
{
	if (verbose) {
		fprintf(stderr, "Warning: unhandled OSC message: '%s' with args '%s'.\n", path, types);
	}

    return -1;
}

static
int osc_set_gain_handler(const char *path, const char *types, lo_arg **argv, int argc,
	lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	float gain = argv[1]->f;
	int result;

	if (verbose) {
    	printf("Received channel gain change OSC message ");
		printf(" (channel=%d, gain=%fdB)\n", chan, gain);
	}

	// Make sure gain is in range
	if (gain<-90) gain = -90;
	if (gain>90) gain = 90;

	// Make sure channel number is in range
	if (chan < 1 || chan > channel_count) {
		fprintf(stderr,"Warning: channel number in OSC message is out of range.\n");
		return 0;
	}

	// store the new value
	channels[chan-1].desired_gain = gain;

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel/gain", "if", chan, channels[chan-1].desired_gain );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;
}

static
int osc_get_gain_handler(const char *path, const char *types, lo_arg **argv, int argc,
	lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	int result;

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel/gain", "if", chan, channels[chan-1].desired_gain );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;
}

static
int osc_set_label_handler(const char *path, const char *types, lo_arg **argv, int argc,
	lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	char* label = &argv[1]->s;
	int result;

	if (verbose) {
    	printf("Received channel label change OSC message ");
		printf(" (channel=%d, label='%s')\n", chan, label);
	}

	if (chan < 1 || chan > channel_count) {
		fprintf(stderr, "Warning: channel number in OSC message is out of range.\n");
		return 1;
	}

	// store the new value
	strncpy( channels[chan-1].label, label, CHANNEL_LABEL_LEN);

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel/label", "is", chan, channels[chan-1].label );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;
}

static
int osc_get_label_handler(const char *path, const char *types, lo_arg **argv, int argc,
	lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	int result;

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel/label", "is", chan, channels[chan-1].label );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;
}

static
int osc_set_mute_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message msg, void *user_data)
{

	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	int result;

	if (verbose) {
		printf("Receved mute set OSC message ");
		printf(" (channel=%d, mute=ON)\n", chan);
	}


	// Make sure channel number is in range
	if (chan < 1 || chan > channel_count) {
		fprintf(stderr,"Warning: channel number in OSC message is out of range.\n");
		return 0;
	}

	// store the new value
	channels[chan-1].mute = true;

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel/mute", "iT", chan );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;


}

static
int osc_unset_mute_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message msg, void *user_data)
{

	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	int result;

	if (verbose) {
		printf("Receved mute set OSC message ");
		printf(" (channel=%d, mute=OFF)\n", chan);
	}


	// Make sure channel number is in range
	if (chan < 1 || chan > channel_count) {
		fprintf(stderr,"Warning: channel number in OSC message is out of range.\n");
		return 0;
	}

	// store the new value
	channels[chan-1].mute = false;

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel/mute", "iF", chan );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;


}

static
int osc_get_mute_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int chan = argv[0]->i;
	char *mute_msgtype = malloc(3);
	int result;

	// Make sure channel number is in range
	if (chan < 1 || chan > channel_count) {
		fprintf(stderr,"Warning: channel number in OSC message is out of range.\n");
		return 0;
	}


	// Send back reply
	result = lo_send_from(
		src,
		serv,
		LO_TT_IMMEDIATE,
		"/mixer/channel/gain",
		(channels[chan-1].mute == true) ? "iT" : "iF",
		chan
	);
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;
}

int osc_get_channel_count_handler(const char *path, const char *types, lo_arg **argv, int argc,
		 lo_message msg, void *user_data)
{
	lo_address src = lo_message_get_source( msg );
	lo_server serv = (lo_server)user_data;
	int result;

	// Send back reply
	result = lo_send_from( src, serv, LO_TT_IMMEDIATE, "/mixer/channel_count", "i", channel_count );
	if (result<1) fprintf(stderr, "Error: sending reply failed: %s\n", lo_address_errstr(src));

	return 0;
}


static
void osc_error_handler(int num, const char *msg, const char *path)
{
    fprintf(stderr, "LibLO server error %d in path %s: %s\n", num, path, msg);
    fflush(stdout);
}


lo_server_thread init_osc( const char * port )
{
	lo_server_thread st = NULL;
	lo_server serv = NULL;

	// Create new server
	st = lo_server_thread_new( port, osc_error_handler );
	if (!st) return NULL;

	// Add the methods
	serv = lo_server_thread_get_server( st );
    lo_server_thread_add_method(st, "/mixer/get_channel_count", "", osc_get_channel_count_handler, serv);
    lo_server_thread_add_method(st, "/mixer/channel/set_gain", "if", osc_set_gain_handler, serv);
    lo_server_thread_add_method(st, "/mixer/channel/get_gain", "i", osc_get_gain_handler, serv);
    lo_server_thread_add_method(st, "/mixer/channel/get_label", "i", osc_get_label_handler, serv);
    lo_server_thread_add_method(st, "/mixer/channel/set_label", "is", osc_set_label_handler, serv);
	lo_server_thread_add_method(st, "/mixer/channel/set_mute", "iT", osc_set_mute_handler, serv);
	lo_server_thread_add_method(st, "/mixer/channel/set_mute", "iF", osc_unset_mute_handler, serv);
	lo_server_thread_add_method(st, "/mixer/channel/get_mute", "i", osc_get_mute_handler, serv);
	lo_server_thread_add_method(st, "/ping", "", osc_ping_handler, serv);

    // add method that will match any path and args
    lo_server_thread_add_method(st, NULL, NULL, osc_wildcard_handler, serv);

	// Start the thread
	lo_server_thread_start(st);

	if (!quiet) {
		char *url = lo_server_thread_get_url( st );
		printf( "OSC server URL: %s\n", url );
		free(url);
	}

	return st;
}

void finish_osc( lo_server_thread st )
{
	if (verbose) printf( "Stopping OSC server thread.\n");

	lo_server_thread_stop( st );
	lo_server_thread_free( st );

}
