# Smart-Thermostat-IR-Blaster
Summary: Control your Remote Controlled window AC unit using a smarter thermostat! Works with most 24VAC based systems.


I own a fancy Nest thermostat to control my home heating, but no central air.  My beefy 15,000 BTU window AC unit is more than enough for my small home.  The Nest has no "off the shelf" solution for controlling heating/cooling by anything else other than the normal thermostat wire.  Which in my case is 24VAC.  My window AC unit came with an IR remote control, so that got me thinking.

I did find other solutions online, but they were too complex.  Either trying to make their own smart thermostat or depending on web APIs that can change over time.

The circuit I came up with is a more simplistic approach.  Monitor the Nest's wire to control AC then send ON/OFF IR signals to my window AC that is within line of sight. No wifi, no batteries. Most impressive, I got the whole program to fit on an 8-pin attiny85 chip.

## Wiring
The Nest normally needs 3-4 wires for heating operation and 4-5 wires for cooling.
- Y = Cooling Control
- G = Fan / Blower
- W = Heating Control
- C = 24VAC Common
- Rh = 24VAC Power

The C and Rh wires provide 24VAC.  The circuit rectifies and regulates the 24VAC into 5VDC.

The Y wire is monitored through a LTV-814H optoisolator.

## Usage
To test IR:
- Press button once to transmit recorded IR signal for ON
- Press button twice to transmit recorded IR signal for OFF

To record IR:
- Hold button for 2 seconds and continue holding
- LED will blink prior to recording, confirms selection
    - Fast Blinking means recording IR signal for ON
    - Slow Blinking means recording IR signal for OFF
    - Wait for LED to stop blinking before pressing the remote control button to learn the new IR code
- Release button to save recorded IR

When signal state change is detected on the Therm Cmd input:
- Transmit recorded IR signal for ON / OFF depending on new state

## Library
https://github.com/z3t0/Arduino-IRremote
