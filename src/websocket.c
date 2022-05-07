/*

	websocket.c
	Copyright (C) 2020  Sam Mulvey

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json.h>
#include <stdbool.h>
#include <unistd.h>
#include <ws.h>
#include "minimix.h"


#define WS_ERROR_LEN 1024


int get_action_int(struct json_object *in, char *key, int *result) {
	struct json_object *tmp;

	if (json_object_object_get_ex(in, key, &tmp) && json_object_is_type(tmp, json_type_int)) {
		*result = json_object_get_int(tmp);
		return 1;
	} else {
		return 0;
	}
}

int get_action_string(struct json_object *in, char *key, char **result) {
	struct json_object *tmp;

	if (json_object_object_get_ex(in, key, &tmp)) {
		(*result) = (char *) json_object_get_string(tmp);
		return 1;
	} else {
		return 0;
	}
	
}


int get_action_double(struct json_object *in, char *key, double *result) {
	struct json_object *tmp;

	if (json_object_object_get_ex(in, key, &tmp) && 
			(json_object_is_type(tmp, json_type_double) || json_object_is_type(tmp, json_type_int))) {
		*result = json_object_get_double(tmp);
		return 1;
	} else {
		return 0;
	}
}


void ws_client_error(ws_cli_conn_t *client, char *str) {
	char errorstr[WS_ERROR_LEN];
	snprintf(errorstr, WS_ERROR_LEN, "{\"error\":\"%s\"}\n" , str);
	ws_sendframe_txt(client, errorstr);
}


void ws_channel_state(ws_cli_conn_t *client, int channel) {
	struct json_object *document, *update, *element;
	int start, end;
	char buf[64];


	if (channel == 0) {
		start = 0;
		end = channel_count;
	} else if (channel > channel_count) {
		ws_client_error(client, "Invalid channel.");
		return;
	} else {
		start = channel;
		end = channel+1;
	}




	document = json_object_new_object();
	update = json_object_new_array();

	for (int i = start ; i < end ; i++) {

		snprintf(buf, 64, "%.7f", channels[i].desired_gain);
		element = json_object_new_object();
		json_object_object_add(element, "ch",    json_object_new_int(i+1));
		json_object_object_add(element, "label", json_object_new_string(channels[i].label));
		json_object_object_add(element, "gain",  json_object_new_string(buf));
		json_object_object_add(element, "mute",  json_object_new_boolean(channels[i].mute));
		json_object_array_add(update, element);
	}


	json_object_object_add(document, "update", update);

	ws_sendframe_txt(client, json_object_to_json_string_ext(document, JSON_C_TO_STRING_PLAIN));


}

void ws_set_gain(ws_cli_conn_t *client, struct json_object *action) {
	int ch;
	double db;
	
	if (!get_action_int(action, "ch", &ch))  {
		ws_client_error(client, "Gain: no channel specified");
		return;
	}

	if (!get_action_double(action, "db", &db)) {
		ws_client_error(client, "Gain: no db specified");
	}

	if (ch < 1 || ch > channel_count) {
		ws_client_error(client, "Gain: invalid channel specified");
		return;
	}
	
	if (db < -90) db = -90;
	if (db > 90) db = 90;

	if (verbose) printf("ws client %s set gain on channel %d to %f\n", ws_getport(client), ch, db);

	channels[ch-1].desired_gain = (float) db;
	ws_channel_state(NULL, ch-1);



}

void ws_set_mute(ws_cli_conn_t *client, struct json_object *action, bool set) {
	int ch;

	if (!get_action_int(action, "ch", &ch))  {
		ws_client_error(client, "Mute: no channel specified");
		return;
	}

	if (ch < 1 || ch > channel_count) {
		ws_client_error(client, "Mute: invalid channel specified");
		return;
	}

	if (set) {
		if (verbose) printf("ws client %s muted channel %d\n", ws_getport(client), ch);
		channels[ch-1].mute = true;
	} else {
		if (verbose) printf("ws client %s unmuted channel %d\n", ws_getport(client), ch);
		channels[ch-1].mute = false;
	}

	ws_channel_state(NULL, ch-1);
}

void ws_set_label(ws_cli_conn_t *client, struct json_object *action) {
	int ch;
	char *label;

	if (!get_action_int(action, "ch", &ch)) {
		ws_client_error(client, "Label: no channel specified");
		return;
	}

	if (!get_action_string(action, "txt", &label)) {
		ws_client_error(client, "Label: no label specified");
		return;
	}

	if (ch < 1 || ch > channel_count) {
		ws_client_error(client, "Label: invalid channel specified");
		return;
	}

	printf("LABEL: %s\n", label);

	strncpy(channels[ch-1].label, label, CHANNEL_LABEL_LEN);
	if (verbose) printf("ws client %s changed label of channel %d to '%s'\n", ws_getport(client), ch, channels[ch-1].label);
	ws_channel_state(NULL, ch-1);
}

void ws_messages(ws_cli_conn_t *client, const unsigned char *incoming, uint64_t size, int type) {

	struct json_object *message, *action;
	char *name, *label, *port;
	int cmdlen, ch, us;
	double db;

	port = ws_getport(client);

	message = json_tokener_parse(incoming);
	if (!json_object_is_type(message, json_type_array)) {
		ws_client_error(client, "Improper formatting.");
		return;
	}


	for (int i = 0 ; i < json_object_array_length(message) ; i++) {
		action = json_object_array_get_idx(message, i);

		name = (char *) json_object_get_string(json_object_object_get(action, "act"));


		if (strncmp(name, "mixer_state", 11) == 0) {
			if (verbose) printf("ws client %s requested full mixer state\n", port);
			ws_channel_state(client, 0);

		} else if (strncmp(name, "state", 5) == 0) {
			
			if (get_action_int(action, "ch", &ch)) {
				if (verbose) printf("ws client %s requested state for channel %d\n", port, ch);
				ws_channel_state(client, ch-1);
			} else {
				ws_client_error(client, "Bad state request.");
			}


		} else if (strncmp(name, "delay", 5) == 0) {

			if (get_action_int(action, "us", &us)) {
				if (verbose) printf("ws client %s requested delay of %d us\n", port, us);
				usleep(us);
			} else {
				ws_client_error(client, "Delay: no amount specified");
			}
			 

		} else if (strncmp(name, "gain", 4) == 0) {

			ws_set_gain(client, action);
		

		} else if (strncmp(name, "mute", 4) == 0) {

			ws_set_mute(client, action, true);

		} else if (strncmp(name, "unmute", 6) == 0) {

			ws_set_mute(client, action, false);

		} else if (strncmp(name, "label", 5) == 0) {

			ws_set_label(client, action);

			
			
		} else {
			ws_client_error(client, "Bad command or taco.");
		}



	}
	
}


void ws_connect(ws_cli_conn_t *client) {
	if (verbose) printf("websocket connected from port %s\n", ws_getport(client));
	ws_channel_state(client, 0);
}

void ws_disconnect(ws_cli_conn_t *client) {
	if (verbose) printf("websocket disconnected from port %s\n", ws_getport(client));
}



void init_websocket(char *port) {
	struct ws_events wse;
		wse.onopen    = &ws_connect;
		wse.onclose   = &ws_disconnect;
		wse.onmessage = &ws_messages;

	if (verbose) printf("Websocket server URL: ws://localhost:%s\n", port);
	ws_socket(&wse, "localhost", port, 1, 0);


}
