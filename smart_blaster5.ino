
/*
   Smart Thermostat IR Blaster

   Author: Dustin Westaby
   Date: June 2017

   Chipset: ATTINY85

   Hardware IO Map:
   IR OUT*   = PB4 = PIN 3 = D4 / A2
   USER SW   = PB3 = PIN 2 = D3 / A3
   THERM CMD = PB2 = PIN 7 = D2 / A1
   IR IN*    = PB1 = PIN 6 = D1 / PWM
   USER LED  = PB0 = PIN 5 = D0 / PWM

   *Hardware rev1 has a mistake that flips these signals
   PCB MUST BE MODIFIED TO WORK

   Current Usage:
      To test IR:
      - Press button once to transmit recorded IR signal for ON
      - Press button twice to transmit recorded IR signal for OFF
      To record IR:
      - Hold button for 2 seconds and continue holding
      - LED will blink prior to recording
      - - Fast Blinking means recording IR signal for ON
      - - Slow Blinking means recording IR signal for OFF
      - - Wait for LED to stop blinking before pressing the remote control button to learn the new IR code
      - Release button to save recorded IR
      When signal state change is detected on the Therm Cmd input:
      - Transmit recorded IR signal

*/


/*
 * IRrecord: record and play back IR signals as a minimal
 * An IR detector/demodulator must be connected to the input RECV_PIN.
 * An IR LED must be connected to the output PWM pin 3.
 * A button must be connected to the input BUTTON_PIN; this is the
 * send button.
 * A visible LED can be connected to STATUS_PIN to provide status.
 *
 * The logic is:
 * If the button is pressed, send the IR code.
 * If an IR code is received, record it.
 *
 */

#include <tiny_IRremote.h>

#define RECV_PIN   4 //WARNING, See PCB rev1 modification notes
#define BUTTON_PIN 3
#define STATUS_PIN 0
#define THERMOSTAT_SIGNAL_PIN 2

#ifdef DEBUG
 #define DEBUG_PRINTLN(x)        Serial.println(x)
 #define DEBUG_PRINT(x)          Serial.print(x)
 #define DEBUG_SERIAL_BEGIN(x)   Serial.begin(x);
 #define DEBUG_HEX_PRINTLN(x)    Serial.println(x, HEX);
 #define DEBUG_DEC_PRINTLN(x)    Serial.println(x, DEC);
#else
 #define DEBUG_PRINTLN(x)
 #define DEBUG_PRINT(x)
 #define DEBUG_SERIAL_BEGIN(x)
 #define DEBUG_HEX_PRINTLN(x)
 #define DEBUG_DEC_PRINTLN(x)
#endif

IRrecv irrecv(RECV_PIN);
IRsend irsend;

decode_results results;

#define ON_RECORDING  0
#define OFF_RECORDING 1

// Storage for the recorded code
int8_t codeType[2] = {-1,-1};
uint32_t codeValue[2];
uint8_t codeLen[2];

uint8_t toggle = 0; // The RC5/6 toggle state

#define THERM_SIGNAL_ON  LOW
#define THERM_SIGNAL_OFF HIGH

#define BUTTON_PRESSED LOW
#define BUTTON_RELEASED HIGH

#define NOT_STARTED 0
#define COMPLETE 1
#define IN_PROGRESS 2

#define SIGNAL_FILTER_MS 100 //max value is 254

uint8_t last_thermostat_signal = 0;

void setup()
{
  DEBUG_SERIAL_BEGIN(9600);

  irrecv.enableIRIn(); // Start the receiver

  pinMode(BUTTON_PIN, INPUT);
  pinMode(STATUS_PIN, OUTPUT);
  pinMode(THERMOSTAT_SIGNAL_PIN,INPUT);

  digitalWrite(STATUS_PIN, LOW);

//   init_values_from_eeprom();
}

void loop()
{
  //handle_thermostat_signal_changes();
  handle_user_button_presses();
}

void handle_user_button_presses()
{
  static uint8_t lastButtonState;
  static long button_held_timer = 0;
  static uint8_t recording_state = NOT_STARTED;
  static uint8_t button_code = ON_RECORDING;

  uint8_t buttonState = digitalRead(BUTTON_PIN);

  // If button was "just" pressed, send the code.
  if (lastButtonState == BUTTON_RELEASED &&
      buttonState == BUTTON_PRESSED)
  {
    DEBUG_PRINTLN("Pressed, sending");
    digitalWrite(STATUS_PIN, HIGH);
    sendCode(false, button_code);
    digitalWrite(STATUS_PIN, LOW);
    delay(50); // Wait a bit between retransmissions

    //reset variables
    button_held_timer = millis();
  }

  // button held for less than 2 seconds, inform user of code number
  if ( (buttonState == BUTTON_PRESSED) &&
       !(button_held_timer + 2000 < millis()) )
  {
    //inform user of code number
    blink_led((button_code+1)*250);

    // ON_RECORDING  Fast Blinking 400ms  4 Blinks per second
    // OFF_RECORDING Slow Blinking 500ms  2 Blinks per seoond
  }

  // button held for 2+ seconds, start recording
  if( (buttonState == BUTTON_PRESSED) &&
      (button_held_timer + 2000 < millis()) )
  {
    //IR save state machine
    switch(recording_state)
    {
      case NOT_STARTED:
        irrecv.enableIRIn(); // Re-enable receiver
        recording_state = IN_PROGRESS;
      case IN_PROGRESS:
        if (irrecv.decode(&results))
        {
          digitalWrite(STATUS_PIN, HIGH);
          storeCode(&results, button_code);
          digitalWrite(STATUS_PIN, LOW);
          irrecv.resume(); // resume receiver (might be able to delete this, TBD)
          recording_state = COMPLETE;
        }
        break;
      case COMPLETE:
       //save recording
//         EEPROM.put(IR_COMMAND_EEPROM_ADDRESS, pulses);
//         EEPROM.update(PULSE_SIZE_EEPROM_ADDRESS, pulse_size);
        break;
    }

  }

  // button released
  if( buttonState == BUTTON_RELEASED )
  {
    DEBUG_PRINTLN("Released");

    //reset variables
    recording_state = NOT_STARTED;

    //switch to the next code
    if(button_code == ON_RECORDING)
    {
      button_code = OFF_RECORDING;
    }
    else
    {
      button_code = ON_RECORDING;
    }

  }

  lastButtonState = buttonState;
}




// Stores the code for later playback
// Most of this code is just logging
void storeCode(decode_results *results, uint8_t codeStore)
{
  uint8_t count = results->rawlen;

  if (results->decode_type == UNKNOWN)
  {
    DEBUG_PRINTLN("Received unknown code, can't save to short memory space");
  }
  else
  {
    if (results->decode_type == NEC)
    {
      if (results->value == REPEAT)
      {
        DEBUG_PRINTLN("NEC repeat; ignoring.");
        return;
      }
    }

    DEBUG_HEX_PRINTLN(results->value);

    codeType[codeStore]  = results->decode_type;
    codeValue[codeStore] = results->value;
    codeLen[codeStore]   = results->bits;
  }
}

void sendCode(uint8_t repeat, uint8_t codeStore)
{
  if (codeType[codeStore] == NEC)
  {
    if (repeat)
    {
      irsend.sendNEC(REPEAT, codeLen[codeStore]);
      DEBUG_PRINTLN("Sent NEC repeat");
    }
    else
    {
      irsend.sendNEC(codeValue[codeStore], codeLen[codeStore]);
      DEBUG_PRINT("Sent NEC ");
      DEBUG_HEX_PRINTLN(codeValue[codeStore]);
    }
  }
  else if (codeType[codeStore] == SONY)
  {
    irsend.sendSony(codeValue[codeStore], codeLen[codeStore]);
    DEBUG_PRINT("Sent Sony ");
    DEBUG_HEX_PRINTLN(codeValue);
  }
  else if (codeType[codeStore] == RC5 || codeType[codeStore] == RC6)
  {
    if (!repeat)
    {
      // Flip the toggle bit for a new button press
      toggle = 1 - toggle;
    }

    // Put the toggle bit into the code to send
    codeValue[codeStore] = codeValue[codeStore] & ~(1 << (codeLen[codeStore] - 1));
    codeValue[codeStore] = codeValue[codeStore] | (toggle << (codeLen[codeStore] - 1));

    if (codeType == RC5)
    {
      DEBUG_PRINT("Sent RC5 ");
      DEBUG_HEX_PRINTLN(codeValue[codeStore]);
      irsend.sendRC5(codeValue[codeStore], codeLen[codeStore]);
    }
    else
    {
      irsend.sendRC6(codeValue[codeStore], codeLen[codeStore]);
      DEBUG_PRINT("Sent RC6 ");
      DEBUG_HEX_PRINTLN(codeValue[codeStore]);
    }
  }
  else if (codeType[codeStore] == UNKNOWN)
  {
    DEBUG_PRINTLN("Raw Needed");
  }
}


void handle_thermostat_signal_changes()
{
  uint8_t current_thermostat_signal = digitalRead(THERMOSTAT_SIGNAL_PIN);

  //debounce AC input
  current_thermostat_signal = debounce_ac_input(current_thermostat_signal);

  //if signal changed
  if(current_thermostat_signal != last_thermostat_signal)
  {
    digitalWrite(STATUS_PIN, HIGH);

    //transmit ON/OFF signal
    if(current_thermostat_signal == THERM_SIGNAL_ON)
    {
      DEBUG_PRINTLN("Pressed, sending on signal");
      sendCode(false, ON_RECORDING);
    }
    else //THERM_SIGNAL_OFF
    {
      DEBUG_PRINTLN("Pressed, sending off signal");
      sendCode(false, OFF_RECORDING);
    }

    digitalWrite(STATUS_PIN, LOW);
  }

  //save signal state through power loss
  last_thermostat_signal = current_thermostat_signal;
//   EEPROM.update(THERM_SIGNAL_EEPROM_ADDRESS, last_thermostat_signal);
}

uint8_t debounce_ac_input(uint8_t signal_to_filter)
{
   static uint8_t consecutive_ms_of_low_reads = 0;
   static long last_off_detected_ms = 0;

   // AC input is fullwave rectified and crosses 0V periodically when high
   // So we filter low signals

   // Note: 60Hz crosses 0V every 8.34ms and peaks 4.17ms later
   // SIGNAL_FILTER_MS should be set to at least 8 ms

   //check for low signal
   if(signal_to_filter == 0)
   {
      //check for 1 ms elapsed
      if( last_off_detected_ms < millis())
      {
         //reset timer
         last_off_detected_ms = millis();

         //increment counter
         consecutive_ms_of_low_reads++;

         //if signal was low, but consecutive milliseconds is less than SIGNAL_FILTER_MS
         if(consecutive_ms_of_low_reads < SIGNAL_FILTER_MS)
         {
            //ignore current signal (FILTERED)
            signal_to_filter = last_thermostat_signal;

            //lock counter to max
            consecutive_ms_of_low_reads = SIGNAL_FILTER_MS;
         }
         //else, use low signal (UNFILTERED)
      }
   }
   else
   {
      //signal was high (UNFILTERED)

      //reset counter
      consecutive_ms_of_low_reads = 0;
   }

   //return the filtered or unfiltered signal
   return signal_to_filter;
}

/* ********************************************************
   Low-Level Sub Functions
   ******************************************************** */

void blink_led(uint16_t interval_ms)
{
   static uint8_t ledState = HIGH;

   if(millis()%interval_ms==0)
   {
      // if the LED is off turn it on and vice-versa:
      if (ledState == LOW)
      {
         ledState = HIGH;
      }
      else
      {
         ledState = LOW;
      }

      digitalWrite(LED_STATUS_PIN, ledState);
   }
}
