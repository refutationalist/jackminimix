( travis removed )


JackMiniMix with Websockets
===========================

JackMiniMix is a simple mixer for the  Jack Audio Connection Kit with an OSC 
based control interface. It is released under the  GPL licence.

For the latest version of JackMiniMix, please see:
http://www.aelius.com/njh/jackminimix/


OSC Interface
-------------

Channels numbers range from 1 to the total number of channels. Gains are in floating point decibels in the range -90 to 90 dB, where -90 dB is treated as infinite.

    /mixer/get_channel_count        - Get the number of channels
     replies with:
    /mixer/channel_count (i)

    /mixer/channel/set_gain (if)    - Set the gain of channel i to f dB
     replies with:
    /mixer/channel/gain (if)

    /mixer/channel/set_label (is)   - Set the label of channel i to s
     replies with:
    /mixer/channel/label (is)
 
    /mixer/channel/get_gain (i)     - Get gain of channel i
     replies with:
    /mixer/channel/gain (if)

    /mixer/channel/get_label (i)    - Get the label of channel i
     replies with:
    /mixer/channel/label (is)
  
    /ping                           - Check mixer is still there
     replies with:
    /pong

Replies are send back to the port/socket that they were sent from.

Websocket Interface
-------------------

This version of JackMiniMix has been poorly modified to include a JSON websocket interface for remote control.  The commands are **not** one-to-one mapped to OSC commands, as the method of operation is expected to be different.  Commands are sent in an array to allow for complex chaining of events in a single transaction.  Individual commands look like this:

### Sending Commands

    {"act":"mixer_state"}                 - Dump data on all channels
    {"act":"state","ch":1}                - returns gain, label, and mute state
    {"act":"gain","ch":1,"db":-23.2}      - Sets gain to -23.2dB
    {"act":"mute","ch":1}                 - Mutes channel 1
    {"act":"unmute","ch":1}               - Unmutes channel 1
    {"act":"label","ch":1,"txt":"Mic 1"}  - Sets label of channel 1 to "Mic 1"
    {"act":"delay", "us":100}             - Delay command processing for 100us

Therefore, a string of commands would look like this:

```json
[
    {
        "act": "mute",
        "ch": 2
    },
    {
        "act": "unmute",
        "ch": 1
    },
    {
        "act": "gain",
        "ch": 1,
        "db": 0
    }
]
```

### Receving Updates

The main message a client will receive is an update array, which is the complete state of channels that have changed, unless it is in response to a ``mixer_state``  command or on inital connection, in which case all channels are sent.  When the state of a channel is changed, an update is sent to all clients.  This could become strange.

State data will look like:

```json
{
    "update": [
        {
            "ch": 1,
            "label": "Mic 1",
            "db": 2.7,
            "mute": false
        },
        {
            "ch": 2,
            "label": "Cows",
            "db": -45,
            "mute": true
        }
    ]
}
```


Unless you get some kind of error, in which case:
```json
{"error":"Something somewhere has gone wrong."}
```

Simple, right?

Heh.