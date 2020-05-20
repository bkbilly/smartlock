# smartlock

The aim of this project is to create a smart lock that can be easily configured.

## Hardware
Components that will be used:
  - RC522 (Reading NFC tags)
  - ESP-32s
  - Worm Motor with Encoder DC12V
  - A4988 Stepper Motor Driver

## Software
It is using WiFi to connect to an MQTT Server and has support for Over the Air updates.

These are the messages that are supported:
  - **smartlock/rfid_uid** message=[<uid>]
  - **smartlock/status** message=['online']
  - **smartlock/rfidadd_timer** message=['ON','OFF']
  - **smartlock/rfiddel_timer** message=['ON','OFF']
  - **smartlock/access** message=['authorized','denied']
  - **smartlock/saved_uids** message=[<uid list>]

  - **smartlock/rfidadd_timer/set** message=['ON','OFF']
  - **smartlock/rfiddel_timer/set** message=['ON','OFF']
  - **smartlock/saved_uids/set** message=['DELETE','']
