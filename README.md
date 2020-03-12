# mqttGetRetained

## Purpose

mqttGetRetained subscribes to one or more mqtt topics and gets the retained message if it exists.


## Installation


`sudo apt-get install mosquitto-dev`

`sudo apt-get install libjson-c-dev`

`sudo apt-get install libmosquittopp-dev`

`sudo apt-get install libssl1.0-dev`

## Build

`make`


## Command line switches

switch|Required/Optional|argument|description
---|---|---|---
--mqtt-host|REQUIRED|mqtt server|mqtt server
--mqtt-topic|REQUIRED|topic|mqtt topic
--mqtt-port|OPTIONAL|number|default is 1883
--mqtt-user-name|OPTIONAL|user name|maybe required by system
--mqtt-passwd|OPTIONAL|password|maybe required by system
--json-enclosing-array|OTIONAL|json label|default is "retained"
--verbose|OPTIONAL|(none)|Turn on verbose (debuging).
--help|OPTIONAL|(none)|displays help and exits


## example

`mqttGetRetained --mqtt-host localhost --mqtt-user-name clare --mqtt-passwd kb0ssj  --mqtt-topic right --mqtt-topic left/right --mqtt-topic left --verbose  --json-enclosing-array retainedMessages  --mqtt-topic bar`

upon execution results in the following json object output to stdout.

`{
  "retainedMessages":[
    {
      "bar":{
        "message":null
      }
    },
    {
      "left":{
        "message":"whatever"
      }
    },
    {
      "left\/right":{
        "message":"whatever 4"
      }
    },
    {
      "right":{
        "message":"whatever 44"
      }
    }
  ]
}`
