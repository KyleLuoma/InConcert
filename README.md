# InConcert
Distributed system-based live performance augmentation


## Components

### Aggregator-Broadcaster

#### Communications:
The aggregator broadcaster provides a wireless access point service. Devices should connect directly in order to send and receive messages.

Packets are sent and received using sockets with UDP datagram payloads. Payloads are defined in ~/aggregator-broadcaster/inconcert-communication.h

##### Receives:

 - Tempo messages: Sensor devices are responsible for determining their BPM and sending a complete tempo message to the broadcaster. When a tempo message is received, the sending client is added to a list of tempo sources, and the latest tempo broadcast from each client is used to determine aggregate tempo.

 - Event messages: Sensor devices can send discrete aperiodic event messages that are associated with performance actions such as drum hits or dance movements.

 - Registration messages: Devices unable to receive traffic on the UDP network broadcast IP may send a registration message. If the aggregator receives such a message, it will send traffic directly to the registered device.

NOTE: do this ONLY if the device can't receive message over the broadcast, as it increases network traffic significantly.

##### Sends:

 - Time messages: These messages are sent out every beat interval. The interval is determined by the aggregate BPM from sensors. Use this to synch actuators with the actual tempo of the performance. You should expect +- 5ms accuracy.

 - Tempo messages: Sent periodically over broadcast to keep clients aware of the current BPM.

 - Event Messages: Forwarded from sensor clients out to the broadcast channel. The aggregator performs no processing on these messages and assumes they are correct from the client.


### Sensor-Drum

#### Communicatoins:

##### Sends:

 - Tempo messages: calculated tempo based on sensor event analysis

 - Event messages: One event per cycle that at least one drum hit was detected.

##### Receives:

 - Time messages: checks for time messages on a periodic basis and updates tempo intervals based on new data. If a time message is missed, the tempo is maintained using last available interval data.

### Sensor-Dance

### Actuator-Stage-Lights

### Actuator-Metronome

#### Communications:

##### Sends:
Nothing

##### Receives:
 - Tempo messages: used for updating pendulum rate
 - Time messages: Used for re-synching with the aggregator main rhythm after a new tempo message is received



### References:
[Sending and Receiving UDP packets on Arduino](https://docs.arduino.cc/library-examples/wifi-library/WiFiUdpSendReceiveString)
[Network Time Protocol Wikipedia](https://en.wikipedia.org/wiki/Network_Time_Protocol)
