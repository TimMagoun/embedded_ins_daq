This is an embedded data acquisition unit, or DAQ, for short. The goal of this project is to store UART bytes into an SD card as a data stream where UART data comes from up to 4 serial ports.

All data stream will be consolidated into one binary file, and the binary file will contain packets of data containing the following items:

- The sensor ID, which is the port number that the data came from
- The timestamp, which is a monotonic clock with microsecond accuracy
- The length of the bytes itself
- A checksum at the end of it

In addition, we will be able to store timestamp triggers into that same binary file. Each timestamp trigger will add an entry containing the sensor ID, the monotonic timestamp, and the type of trigger, which could either be a trigger or a sync. The trigger is a device-generated pulse, whereas the sync is an interrupt coming from the outside.

It is important that we prioritize reliable writes into the SD card, as well as microsecond-accurate time stamping based on when a packet arrives, when a trigger is sent out, and when a sync is detected via interrupt.
We require microsecond accuracy for all timing-related functions here.

For the first prototype, we will also need to build a simple State Manager with the following states:

- init
- ready
- running
- faulted
- init will transition into ready once it verifies a simple configuration. At first, we will make that configuration known at compile time.
- ready can transition into running with the start trigger, and running can transition back into ready with the stop trigger.
- All states can transition into the faulted state, and it will stay in that faulted state because all faults are irrecoverable.

I foresee the following simple design for our first prototype. The goal of this prototype is to make sure that the data is being captured correctly and to discover what feature we truly value.I don't want to add too many features or even too much robustness and status and transitions in case they are not actually valuable when I start using this. The use case for this device will be to capture UART traffic from GNSS receivers (multiple GNSS receivers) and multiple inertial measurement units. It is very important that the data capture is robust and the timing is accurate for later post-processing.

There will be a UART capture module that is responsible for communicating with a sensor, time stamping the first byte of a chunk, and sending chunks via a queue into storage. Likewise, there will be a Trigger Sync module that is responsible for capturing the time stamp of a trigger output or a sync input and packaging that into a record to be sent into the storage recorder.There will be multiple instances of UART capture and trigger and sync capture because we have up to four ports, and each port can also have a trigger or sync line associated with it.The storage/recorder module will then take these records, put them into a uniform packet format, and give it to the SD card writer module. The SD card writer module will then be able to write onto an SD card and it performs all the file system handling as well as sending faults if the SD card is unplugged mid-session.It's important to note that the capture modules (both UART and TriggerSync) will be sending to the storage module via a queue. The storage module will be responsible for converging multiple queues into one queue to be sent to the SD card module. The SD card module will then also pull from the storage module via another queue.This ensures that temporary delays in SD card writing do not slow down the rest of the pipeline.Every module described here will also have a queued connection to a status and fault module. The use case of that module is not too important right now, but I want to be able to send debug updates or fault messages to a centralized place. The fault messages will be generic.

The test rig I have built up for this is a mock UART data sender which will be sending UART traffic at up to 921600 baud as well as generating trigger and sync signals. The sync signals can be bi-directional, meaning we need to detect rising and falling edges on that line.I will connect that mock data center to our ESP32P4 and start recording. Afterwards, I will export the data recorded and verify that data integrity is good and that we have captured 100% of the messages. I will independently use an oscilloscope or logic analyzer to validate the accuracy of the timing capture within the ESP.
