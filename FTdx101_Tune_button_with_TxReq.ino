
/*
    Create Tune Button for FTdx101.
    On button press: read present power setting, then set PWR to a selected value, enable the TX relay.
    On button release: disable the TX relay and restore original power setting.
    This software is written by Eeltje Luxen, PA0LUX and is in the public domain.
*/


int RelayPin = 13;              // TX Relaydriver is connected to pin 13
int switchPin = 2;              // Tune button is connected to pin 2
int redledPin = 3;              // red led is connected to pin 3, connection to radio failed
int greenledPin = 4;            // green led connected to pin 4, communicating with radio
int val;                        // variable for reading the Tune button status
int val2;                       // variable for reading the delayed/debounced status of Tune button
bool tune = true;               // do not tune when false
bool buttonrelease = false;     // flag to send just one set of commands at Tune button release
bool buttonpress = false;       // flag to send just one set of commands at Tune button press
bool receiving;                 // variable for the receive radio data process
String prevpwr;                 // this string stores the present power setting
String CAT_buffer;              // string to hold the received CAT data from radio
char rx_char;                   // variable for each character received from the radio
char t;                         // used to clear input buffer


#define check_conn "PC;"                // sed to inquire if radio is connected(power)
#define set_tune_pwr "PC020;"           // set tune power you can choose which PWR to use for tuning (5 - 100 W)
#define get_PWR "PC;"                   // define request Power command
#define timeout_delay 1000;             // the delay to flag a data timeout from the radio, now set at 1000 mSec
unsigned long timeout, current_millis;  // variables to handle communication timeout errors




void setup()
{
  Serial.begin(9600, SERIAL_8N2);           // RS232 connection speed to FTdx101 NOTE 2 stopbits
  pinMode(RelayPin, OUTPUT);                // set the Relaydriver pin as output
  pinMode(switchPin, INPUT);                // set the Tune button pin as input
  pinMode(3, OUTPUT);                       // output to red statusled
  pinMode(4, OUTPUT);                       // output to green statusled
}


void loop() {

  check_connection();                     // check if the radio is connected and answering requests
  val = digitalRead(switchPin);           // read Tune button and store it in val
  delay(50);                              // 50 mS debounce time
  val2 = digitalRead(switchPin);          // read Tune button again for bounces
  if (val == val2) {                      // when no bounce
    if (val == HIGH) {                    // the Tune button is pressed, and no bounce
      if (buttonpress == false) {         // input and output CAT commands only once when button pressed (it is in loop)
        send_command(get_PWR);            // send the request Power Command to radio, call send routine
        get_response();                   // receive the present power setting from radio
        prevpwr = CAT_buffer;             // prevpwr holds original power setting to restore later
        if (tune == true) {               // valid power info is received, we can set tune power now
          send_command(set_tune_pwr);     // send the tune power to the radio, call send routine
          delay(50);                      // wait to TX 20 mS after CAT has been sent
          digitalWrite(RelayPin, HIGH);   // turn TX Relay on and keep it on until Tune button released, now transmitting
          tune = false;                   // tuning flag reset, be ready for next button press.
        }
      }
      buttonpress = true;                         // Tune button was pressed and now we
      buttonrelease = true;                       // do Tune button release
    }
    if ((val == LOW) && (buttonrelease == true)) {          // now the Tune button has been released
      digitalWrite(RelayPin, LOW);                          // turn TX Relay off, stop transmitting
      delay(35);                                            // wait until TX is switched off before sending CAT commands.
      Serial.print(prevpwr);                                // restore original power setting to the radio
      buttonrelease = false;                                // reset flag, Tune button was released
      buttonpress = false;                                  // reset flag, Tune button not pressed anymore
    }
  }
}


void send_command(String CAT_command)                      // this routine sends the CAT command string to the radio

{
  do
  { t = Serial.read();                                     // this is to empty the input buffer when starting reading data from radio,
  } while (Serial.available() > 0);                        // buffer contains erratic data when the radio is switched on (fix power up sequence)
  Serial.print(CAT_command);                               // sends a CAT command to the radio
}


void get_response()                                         // this roitine receives the CAT command response from the radio
{

  // set a timeout value for if we do not get an answer from the radio, time out is non blocking
  current_millis = millis();                                // get the current time
  timeout = current_millis + timeout_delay;                 // calculate the timeout time

  // check for millis() rollover condition - the Arduino millis() counter rolls over about every 47 days
  if (timeout < current_millis)                             // we've calculated the timeout during a millis() rollover event
  {
    timeout = timeout_delay;                        // go ahead and calculate as if we've rolled over already (adds a few millis to the timeout delay)
  }

  // Get ready to receive CAT response from the radio
  receiving = true;                                         // start receiving
  CAT_buffer = "";                                          // clear CAT buffer
  do
  {
    if (millis() > timeout)                                 // no data received within timeout delay
    {
      // We timed out - exit
      receiving = false;                                    // clear receive flag
      CAT_buffer = "";                                      // clear buffer
      tune = false;                                         // do not start tuning, no info received
      break;                                                // no receive response, exit to loop
    }
    if (Serial.available() && receiving)                    // if there's a character in the rx buffer and we are receiving
    {
      rx_char = Serial.read();                              // get one character at the time from radio
      if (rx_char == ';')                                   // ";" indicates the end of the response from the radio
      {
        receiving = false;                                  // turn off the ok to receive flag, this was last character
        CAT_buffer = CAT_buffer + ";";                      // add ";" to buffer, terminator is needed for restore command send to radio
        tune = true;                                        // a complete answer is received, now we are allowed to proceed, return to loop
      }  else
      {
        CAT_buffer = CAT_buffer + rx_char;                  // add the received character to the CAT rx string, build the received string
      }
    }
  } while (receiving);                                      // keep looping while ok to receive data from the radio
}


void check_connection() {                               // this routine checks if the radio is connected

  send_command(check_conn);                             // send command used to test if the radio answers
  delay(200);                                           // do not ask to often
  get_response();                                       // get an answer from the radio
  if (CAT_buffer.startsWith("PC")) {                    // if the answer is correct (it should start with PC)
    digitalWrite(redledPin, LOW);                       // red led off
    digitalWrite(greenledPin, HIGH);                    // green led on, connection with radio established
  }
  else {                                                // we do not get a (correct-) answer
    digitalWrite(redledPin, HIGH);                      // red led on, no connection with radio, bad data or radio powered off
    digitalWrite(greenledPin, LOW);                     // green led off, no connection with radio
  }
}
