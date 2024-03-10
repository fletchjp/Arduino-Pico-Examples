// VLCB4IN4OUT
// Version for use with Raspberry Pi Pico with software CAN Controller.
// Uses both cores of the RP2040.


/*
   Copyright (C) 2023 Martin Da Costa
  //  This file is part of VLCB-Arduino project on https://github.com/SvenRosvall/VLCB-Arduino
  //  Licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
  //  The full licence can be found at: http://creativecommons.org/licenses/by-nc-sa/4.0

*/

/*
      3rd party libraries needed for compilation:

      Streaming   -- C++ stream style output, v5, (http://arduiniana.org/libraries/streaming/)
      ACAN2040    -- library to support Pico PIO CAN controller implementation
      CBUSSwitch  -- library access required by CBUS and CBUS Config
      CBUSLED     -- library access required by CBUS and CBUS Config
*/
///////////////////////////////////////////////////////////////////////////////////
// Pin Use map:
// Pin 1   GP0  CAN Rx
// Pin 2   GP1  CAN Tx
// Pin 3        0V
// Pin 4   GP2  Not Used
// Pin 5   GP3  Not Used
// Pin 6   GP4  Not Used
// Pin 7   GP5  Not Used
// Pin 8        0V
// Pin 9   GP6  Not Used
// Pin 10  GP7  Not Used
// Pin 11  GP8  Not Used
// Pin 12  GP9  Not Used
// Pin 13   0V
// Pin 14  GP10 Not Used
// Pin 15  GP11 Not Used
// Pin 16  GP12 Not Used
// Pin 17  GP13 Request Switch (CBUS)
// Pin 18       0V
// Pin 19  GP14 CBUS green LED pin
// Pin 20  GP15 CBUS yellow LED pin
// Pin 21  GP16 Not Used
// Pin 22  GP17 Not Used
// Pin 23       0V
// Pin 24  GP18 Switch 1
// Pin 25  GP19 Switch 2
// Pin 26  GP20 Switch 3
// Pin 27  GP21 Switch 4
// Pin 28       0V
// Pin 29  GP22 LED 1
// Pin 30  RUN Reset (active low)
// Pin 31  GP26 LED 2
// Pin 32  GP27 LED 3
// Pin 33       0V
// Pin 34  GP28 LED 4
// Pin 35   ADC_VREF
// Pin 36   3V3
// Pin 37   3V3_EN
// Pin 38   0V
// Pin 39   VSYS
// Pin 40   VBUS
//////////////////////////////////////////////////////////////////////////

#define DEBUG 1  // set to 0 for no serial debug

#if DEBUG
#define DEBUG_PRINT(S) Serial << S << endl
#else
#define DEBUG_PRINT(S)
#endif

// 3rd party libraries
#include <Streaming.h>
#include <Bounce2.h>

// VLCB library header files
#include <Controller.h>                   // Controller class
#include <VCAN2040.h>               // CAN controller
#include <Configuration.h>             // module configuration
#include <Parameters.h>             // VLCB parameters
#include <vlcbdefs.hpp>               // VLCB constants
#include <LEDUserInterface.h>
#include "MinimumNodeService.h"
#include "CanService.h"
#include "NodeVariableService.h"
#include "EventConsumerService.h"
#include "EventProducerService.h"
#include "ConsumeOwnEventsService.h"
#include "EventTeachingService.h"
#include "SerialUserInterface.h"

// Module library header files
#include "LEDControl.h"

// constants
const byte VER_MAJ = 1;          // code major version
const char VER_MIN = 'a';        // code minor version
const byte VER_BETA = 0;         // code beta sub-version
const byte MANUFACTURER = MANU_DEV;   // Module Manufacturer set to Development
const byte MODULE_ID = 82;       // CBUS module type

const byte LED_GRN = 14;         // VLCB green Unitialised LED pin
const byte LED_YLW = 15;         // VLCB yellow Normal LED pin
const byte SWITCH0 = 13;         // VLCB push button switch pin

// Controller objects
VLCB::Configuration modconfig;               // configuration object
VLCB::VCAN2040 vcan2040 (16,4);                  // CAN transport object
VLCB::LEDUserInterface ledUserInterface(LED_GRN, LED_YLW, SWITCH0);
VLCB::SerialUserInterface serialUserInterface(&vcan2040);
VLCB::MinimumNodeService mnService;
VLCB::CanService canService(&vcan2040);
VLCB::NodeVariableService nvService;
VLCB::ConsumeOwnEventsService coeService;
VLCB::EventConsumerService ecService;
VLCB::EventTeachingService etService;
VLCB::EventProducerService epService;
VLCB::Controller controller(&modconfig,
                            {&mnService, &ledUserInterface, &serialUserInterface, &canService, &nvService, &ecService, &epService, &etService, &coeService}); // Controller object

// module name, must be 7 characters, space padded.
unsigned char mname[7] = { '4', 'I', 'N', '4', 'O', 'U', 'T' };

// Module objects
const byte LED[] = { 22, 26, 27, 28 };     // LED pin connections through typ. 1K8 resistor
const byte SWITCH[] = { 18, 19, 20, 21 };  // Module Switch takes input to 0V.

const bool active = 0;  // 0 is for active low LED drive. 1 is for active high

const byte NUM_LEDS = sizeof(LED) / sizeof(LED[0]);
const byte NUM_SWITCHES = sizeof(SWITCH) / sizeof(SWITCH[0]);

// module objects
Bounce moduleSwitch[NUM_SWITCHES];  //  switch as input
LEDControl moduleLED[NUM_LEDS];     //  LED as output
byte switchState[NUM_SWITCHES];

// forward function declarations
void eventhandler(byte, const VLCB::VlcbMessage *);
void printConfig();
void processSwitches();

//
///  setup VLCB - runs once at power on called from setup()
//
void setupVLCB() {
  // set config layout parameters
  modconfig.EE_NVS_START = 10;
  modconfig.EE_NUM_NVS = NUM_SWITCHES;
  modconfig.EE_EVENTS_START = 50;
  modconfig.EE_MAX_EVENTS = 64;
  modconfig.EE_PRODUCED_EVENTS = NUM_SWITCHES;
  modconfig.EE_NUM_EVS = 1 + NUM_LEDS;  

  // initialise and load configuration
  controller.begin();

  Serial << F("> mode = ") << ((modconfig.currentMode) ? "Normal" : "Uninitialised") << F(", CANID = ") << modconfig.CANID;
  Serial << F(", NN = ") << modconfig.nodeNum << endl;

  // show code version and copyright notice
  printConfig();

  // set module parameters
  VLCB::Parameters params(modconfig);
  params.setVersion(VER_MAJ, VER_MIN, VER_BETA);
  params.setManufacturer(MANUFACTURER);
  params.setModuleId(MODULE_ID);  

  // assign to Controller
  controller.setParams(params.getParams());
  controller.setName(mname);

  // module reset - if switch is depressed at startup
  if (ledUserInterface.isButtonPressed())
  {
    Serial << F("> switch was pressed at startup") << endl;
    modconfig.resetModule();
  }

  // register our VLCB event handler, to receive event messages of learned events
  ecService.setEventHandler(eventhandler);

  // set Controller LEDs to indicate mode
  controller.indicateMode(modconfig.currentMode);

  // configure and start CAN bus and VLCB message processing
  //vcan2040.setNumBuffers(16, 4);  // more buffers = more memory used, fewer = less
  vcan2040.setPins(1, 0);         // select pins for CAN Tx & Rx

  if (!vcan2040.begin()) {
    Serial << get_core_num() << F("> error starting VLCB") << endl;
  } else {
    Serial << get_core_num() << F("> VLCB started") << endl;
  }
}

//
///  setup Module - runs once at power on called from setup()
//

void setupModule()
{
  unsigned int nodeNum = modconfig.nodeNum;
  // configure the module switches, active low
  for (byte i = 0; i < NUM_SWITCHES; i++)
  {
    moduleSwitch[i].attach(SWITCH[i], INPUT_PULLUP);
    moduleSwitch[i].interval(5);
    switchState[i] = false;
  }

  // configure the module LEDs
  for (byte i = 0; i < NUM_LEDS; i++)
  {
    moduleLED[i].setPin(LED[i], active);  //Second arguement sets 0 = active low, 1 = active high. Default if no second arguement is active high.
    moduleLED[i].off();
  }

  Serial << get_core_num() << "> Module has " << NUM_LEDS << " LEDs and " << NUM_SWITCHES << " switches." << endl;
}

void setup() 
{
  Serial.begin(115200);
  delay(2000);
  Serial << endl << get_core_num() << F("> ** VLCB 4 in 4 out Pico Dual Core ** ") << __FILE__ << endl;

  // show code version and copyright notice
  printConfig();

  setupVLCB();

  // end of setup
  DEBUG_PRINT(get_core_num() << F("> VLCB ready"));
//  delay(20);
}

void setup1()
{
  delay(2010);
  setupModule();

  // end of setup
  DEBUG_PRINT(get_core_num() << F("> Module ready"));
//  delay(25);
}

void loop()
{
  // do VLCB message processing
  controller.process();

  if (rp2040.fifo.available() == 2)
  {
    uint8_t _state = (uint8_t)rp2040.fifo.pop();
    uint16_t sendData = (uint16_t)rp2040.fifo.pop();
    epService.sendEvent(_state, sendData);
  }

// end of loop0()
}

void loop1()
{
  if (rp2040.fifo.available() > 0)
  {
    byte msgLen = rp2040.fifo.pop();
    // Ensure Core 0 has loaded complete message into FIFO
     while((msgLen + 1) != rp2040.fifo.available()){
  }
    receivedData(msgLen);  // Action FIFO contents
  }

  // Run the LED code
  for (byte i = 0; i < NUM_LEDS; i++)
  {
    moduleLED[i].run();
  }

  // test for switch input
  processSwitches();

// end of loop1()
}

void processSwitches(void)
{
  
  for (byte i = 0; i < NUM_SWITCHES; i++)
  {
    moduleSwitch[i].update();
    if (moduleSwitch[i].changed())
    {
      byte nv = i + 1;
      byte nvval = modconfig.readNV(nv);
      bool state;
      byte swNum = i + 1;

      DEBUG_PRINT(get_core_num() << F(" sk> Button ") << i << F(" state change detected ") << nvval);     

      switch (nvval)
      {
        case 1:
          // ON and OFF
           state = (moduleSwitch[i].fell());
          DEBUG_PRINT(get_core_num() << F("sk> Button ") << i << (moduleSwitch[i].fell() ? F(" pressed, send state: ") : F(" released, send state: ")) << state);
          rp2040.fifo.push(state);
          rp2040.fifo.push(swNum);
          
          break;

        case 2:
          // Only ON
          if (moduleSwitch[i].fell())
          {
            state = true;
            DEBUG_PRINT(get_core_num() << F("sk> Button ") << i << F(" pressed, send state: ") << state);
            rp2040.fifo.push(state);
            rp2040.fifo.push(swNum);
          }
          break;

        case 3:
          // Only OFF
          if (moduleSwitch[i].fell())
          {
            state = false;
            DEBUG_PRINT(get_core_num() << F("> Button ") << i << F(" pressed, send 0x") << state);
            rp2040.fifo.push(state);
            rp2040.fifo.push(swNum);
          }
          break;

        case 4:
          // Toggle button
          if (moduleSwitch[i].fell())
          {
            switchState[i] = !switchState[i];
            state = switchState[i];
            DEBUG_PRINT(get_core_num() << F("> Button ") << i << (moduleSwitch[i].fell() ? F(" pressed, send state: ") : F(" released, send state")) << state);
            rp2040.fifo.push(state);
            rp2040.fifo.push(swNum);
          }
          break;

        default:
          DEBUG_PRINT(get_core_num() << F("sk> Button ") << i << F(" do nothing."));
          break;
      }
    }
  }
}

//
/// called from the VLCB library when a learned event is received
//
void eventhandler(byte index, const VLCB::VlcbMessage *msg)
{
  rp2040.fifo.push(msg->len);
  rp2040.fifo.push(index);
  for (byte n = 0; n < msg->len; n++)
  {
    rp2040.fifo.push(msg->data[n]);
  }
  DEBUG_PRINT(get_core_num() << F("> event handler put: index = ") << index << F(", opcode = 0x") << _HEX(msg->data[0]));
  DEBUG_PRINT(get_core_num() << F("> event handler put: length = ") << msg->len);
}

void receivedData(byte length)
{
  byte rcvdData[length];
  byte index = rp2040.fifo.pop();
  for (byte n = 0; n < length; n++)
  {
    rcvdData[n] = rp2040.fifo.pop();
  }

  byte opc = rcvdData[0];
  delay(50);
  DEBUG_PRINT(get_core_num() << F("> event handler popped: index = ") << index << F(", opcode = 0x") << _HEX(rcvdData[0]));
  DEBUG_PRINT(get_core_num() << F("> event handler popped: length = ") << length);

#if DEBUG
  unsigned int node_number = (rcvdData[1] << 8) + rcvdData[2];
  unsigned int event_number = (rcvdData[3] << 8) + rcvdData[4];
#endif

  DEBUG_PRINT(get_core_num() << F("> NN = ") << node_number << F(", EN = ") << event_number);
  DEBUG_PRINT(get_core_num() << F("> op_code = 0x") << _HEX(opc));

  switch (opc) {
    case OPC_ACON:
    case OPC_ASON:
      for (byte i = 0; i < NUM_LEDS; i++)
      {
        byte ev = i + 2;
        byte evval = modconfig.getEventEVval(index, ev);

        switch (evval) {
          case 1:
            moduleLED[i].on();
            break;

          case 2:
            moduleLED[i].flash(500);
            break;

          case 3:
            moduleLED[i].flash(250);
            break;

          default:
            break;
        }
      }
      break;

    case OPC_ACOF:
    case OPC_ASOF:
      for (byte i = 0; i < NUM_LEDS; i++) {
        byte ev = i + 2;
        byte evval = modconfig.getEventEVval(index, ev);

        if (evval > 0) {
          moduleLED[i].off();
        }
      }
      break;
  }
}

void printConfig(void) {
  // code version
  Serial << get_core_num() << F("> code version = ") << VER_MAJ << VER_MIN << F(" beta ") << VER_BETA << endl;
  Serial << get_core_num() << F("> compiled on ") << __DATE__ << F(" at ") << __TIME__ << F(", compiler ver = ") << __cplusplus << endl;

  // copyright
  Serial << get_core_num() << F("> © Martin Da Costa (MERG M6223) 2024") << endl;
  
}
