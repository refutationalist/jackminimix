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


#ifndef __jmm_minimix_h
#define __jmm_minimix_h

#include <jack/jack.h>


#define		DEFAULT_CLIENT_NAME		"minimixer"		// Default name of JACK client
#define		DEFAULT_CHANNEL_COUNT	(4)				// Default number of input channels
#define		CHANNEL_LABEL_LEN		(12)			// Max length of channel label strings
#define		GAIN_FADE_RATE			(400.0f)	// Rate to fade at (dB per second)


typedef struct {
	char label[CHANNEL_LABEL_LEN];	// Label for Channel
	float current_gain;				// decibels
	float desired_gain;				// decibels
	jack_port_t *left_port;			// Left Input Port
	jack_port_t *right_port;		// Right Input Port
} jmm_channel_t;

extern jack_port_t *outport[2];
extern jack_client_t *client;
 
extern unsigned int verbose;
extern unsigned int quiet;
extern unsigned int running;
extern unsigned int channel_count;
extern jmm_channel_t *channels;


#endif /* __minimix_j */
