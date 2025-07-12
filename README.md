PIC16F628A
  - Pin 5 (VSS) to ground
  - Pin 6 (RB0) towards mosfet Gate
  - Pin 7 (RB1) to ground to emulate false "powerloss" on bootup
  - Pin 11 (RB5) to ground to emilate "pi running" on bootup
  - Pin 14 (VDD) to +5V
  - Pin 18 (AN1) to potentiometer center point

FQP50N06  (G D S)
  - Gate to pin 6 (RB0) to drive the relay (emulated by a LED)
  - Source to ground
  - Drain to charge (LED -> resistor -> 5V)
  - 1k resitor between Gate and Source to avoid a floating gate

Potentiometer
  - Center to pin 18 (AN1) for duration
  - Top to +5
  - Bottom to GND

2N3904 in OpenCollector configuration towards PiRunning (E B C - flat fat towards you)
  - E to ground
  - B to controlled signal --> PA9
  - C to PiRunning (Pin 11 - RB5)
  - Pull down resistor between B and E (ground) 6.8k
Or 2N7000 in Open Drain configuration towards PiRunning (S G D - flat face towards you)
  - S to ground
  - G to controlled signal --> PA9
  - D to PiRunning (Pin 11 - RB5)
  - Pull down resistor between G and S (ground) 1k

BSS88 as a protection on PowerLoss on the OrangePi side
  - D to PowerLoss from OrangePiPower
  - G to 3.3V
  - S to OrangePi pin

From AN97055 via https://electronics.stackexchange.com/questions/82104/single-transistor-level-up-shifter


