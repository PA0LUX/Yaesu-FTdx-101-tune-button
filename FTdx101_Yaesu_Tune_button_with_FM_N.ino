
/*
    Create a Tune Button for FTdx101 and other Yaesu transceivers.
    On button press: read present power and mode setting, then set PWR to a selected value, enable the MOX.
    On button release: disable the TX and restore original power and mode setting.
    This software is written by Eeltje Luxen, PA0LUX and is in the public domain.
    september 2025 added support for VFO B tuning (Split mode) for FTdx101D and FTdx101MP.
*/


int switchPin = 2;              // Tune button is connected to pin 2
int redledPin = 3;              // red led is connected to pin 3, connection to radio failed
int greenledPin = 4;            // green led connected to pin 4, communicating with radio
int val;                        // variable for reading the Tune button status
int val2;                       // variable for reading the delayed/debounced status of Tune button
bool buttonrelease = false;     // flag to send just one set of commands at Tune button release
bool buttonpress = false;       // flag to send just one set of commands at Tune button press
bool tune = true;               // flag, do not tune when false
String prevpwr;                 // this string stores the original power setting
String prevmode;                // this string stores the original mode setting from VFO A
String prevmodesplit;           // this string stores the original mode setting from VFO B
bool receiving;                 // variable for the receive radio data process



#define check_conn "PC;"                // used to inquire if radio is connected(power)
#define set_tune_pwr "PC020;"           // set tune power, you can choose which PWR to use for tuning (5 - 100 W)
#define get_PWR "PC;"                   // define request Power command
#define get_Mode "MD0;"                 // define request Mode command VFO A
#define get_Mode_Split "MD1;"           // define request Mode command VFO B
#define timeout_delay 1000;             // delay to flag a data timeout from the radio, now set at 1000 mSec
unsigned long timeout, current_millis;  // variables to handle communication timeout errors

String CAT_buffer;                      // string to hold the received CAT data from radio
char rx_char;                           // variable for each character received from the radio
char t;                                 // used to clear input buffer


void setup()
{
  Serial.begin(9600, SERIAL_8N2);           // RS232 connection speed to transceiver, NOTE 2 stopbits
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
        get_response();                   // receive the present (power) setting from radio
        prevpwr = CAT_buffer;             // prevpwr holds original power setting to restore later
        send_command(get_Mode);           // send the request Mode Command to radio, call send routine VFO A
        get_response();                   // receive the present (mode) setting from radio VFO A
        prevmode = CAT_buffer;            // prevmode holds original mode setting to restore later VFO A
        send_command(get_Mode_Split);     // send the request Mode Command to radio, call send routine VFO B
        get_response();                   // receive the present (mode) setting from radio VFO B
        prevmodesplit = CAT_buffer;       // prevmode holds original mode setting to restore later VFO B
        if (tune == true) {               // valid power/mode info is received from radio, we can set tune mode & power now
          send_command("MD0B;");          // set mode to FM-N, call send routine VFO A
          send_command("MD1B;");          // set mode to FM-N, call send routine VFO B
          send_command(set_tune_pwr);     // send the tune power to the radio
          delay(50);                      // wait before starting to transmit (20 mS after CAT has been sent)
          send_command("MX1;");           // MOX on, transmitting, now do your tuning
          tune = false;                   // tuning flag reset, be ready for next button press.
        }
      }
      buttonpress = true;                 // Tune button was pressed and now we 
      buttonrelease = true;               // do Tune button release
    }
    if ((val == LOW) && (buttonrelease == true)) {          // now the Tune button has been released
      send_command("MX0;");                                 // MOX off, stop transmitting, stop tune signal
      delay(35);                                            // wait until TX is switched off before sending CAT commands.
      Serial.print(prevmode);                               // restore original mode setting to the radio, mode VFO A 
      Serial.print(prevmodesplit);                          // restore original mode setting to the radio, mode VFO B 
      Serial.print(prevpwr);                                // restore original power setting to the radio
      buttonrelease = false;                                // reset flag, Tune button was released
      buttonpress = false;                                  // reset flag, Tune button not pressed anymore
    }
  }
}


void send_command(String CAT_command)                       // this routine sends the CAT command string to the radio

{
  do
  { t = Serial.read();                                     // this is to empty the input buffer when starting reading data from radio,
  } while (Serial.available() > 0);                        // buffer contains erratic data when the radio is switched on (fix power up sequence)
  Serial.print(CAT_command);                               // send a CAT command to the radio
}


void get_response()                                         // this routine receives the CAT command response from the radio
{

  // set a timeout value for if we do not get an answer from the radio, time out is non blocking
  current_millis = millis();                                // get the current time
  timeout = current_millis + timeout_delay;                 // calculate the timeout time

  // check for millis() rollover condition - the Arduino millis() counter rolls over about every 47 days
  if (timeout < current_millis)                             // we've calculated the timeout during a millis() rollover event
  {
    timeout = timeout_delay;              // go ahead and calculate as if we've rolled over already (adds a few millis to the timeout delay)
  }

  // start to receive CAT response from the radio
  receiving = true;                                         // start receiving
  CAT_buffer = "";                                          // clear CAT buffer
  do
  {
    if (millis() > timeout)                                 // no data received within timeout delay
    {
      // there is a time out - exit thru break
      receiving = false;                                    // clear receive flag
      CAT_buffer = "";                                      // clear buffer
      tune = false;                                         // do not start tuning, because no info received
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
