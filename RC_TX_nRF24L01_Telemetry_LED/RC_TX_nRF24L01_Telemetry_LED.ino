
#include <SPI.h>      //Arduino standard library
#include <RF24.h>     //https://github.com/nRF24/RF24
#include <nRF24L01.h>

//free pins
//pin            3
//pin            4
//pin            5
//pin            6
//pin            7
//pin            8
//pin            A5
//pin            A6

//pins for pots, joysticks
#define pot1     A0
#define pot2     A1
#define pot3     A2
#define pot4     A3
#define pot5     A4

//LED RX, TX battery and RF on/off
#define led      2

//input TX battery
#define inTXbatt A7

//pins for nRF24L01
#define CE       9
#define CSN      10

//hardware SPI
//----- MOSI     11 
//----- MISO     12 
//----- SCK      13 

RF24 radio(CE, CSN); //setup CE and CSN pins

const byte addresses[][6] = {"tx001", "rx002"};

//************************************************************************************************************************************************************************
//this structure defines the sent data in bytes (structure size max. 32 bytes), values ​​(min = 1000us, mid = 1500us, max = 2000us) ****************************************
//************************************************************************************************************************************************************************
struct packet
{
  unsigned int steering = 1500;
  unsigned int throttle = 1500;
  unsigned int ch3      = 1500;
  unsigned int ch4      = 1500;
  unsigned int ch5      = 1500;
};
packet rc_data; //create a variable with the above structure

//************************************************************************************************************************************************************************
//this struct defines data, which are embedded inside the ACK payload ****************************************************************************************************
//************************************************************************************************************************************************************************
struct ackPayload
{
  float RXbatt;
};
ackPayload payload;

//************************************************************************************************************************************************************************
//read pots, joysticks ***************************************************************************************************************************************************
//************************************************************************************************************************************************************************
int valmin = 342; //joystick path compensation for 1000us to 2000us Hitec Ranger 2AM
int valmax = 1023 - valmin;

void read_pots()
{
/*
 * Read all analog inputs and map them to one byte value
 * Normal:    rc_data.ch1 = map(analogRead(A0), 0, 1023, 1000, 2000);
 * Reversed:  rc_data.ch1 = map(analogRead(A0), 0, 1023, 2000, 1000);
 * Convert the analog read value from 0 to 1023 into a byte value from 1000us to 2000us
 */ 

  rc_data.steering = map(analogRead(pot1), valmin, valmax, 1000, 2000);
  rc_data.steering = constrain(rc_data.steering, 1000, 2000);
  
  rc_data.throttle = map(analogRead(pot2), valmin, valmax, 1000, 2000);
  rc_data.throttle = constrain(rc_data.throttle, 1000, 2000);
 
  rc_data.ch3 = map(analogRead(pot3), 0, 1023, 1000, 2000);
  rc_data.ch3 = constrain(rc_data.ch3, 1000, 2000);
   
  rc_data.ch4 = map(analogRead(pot4), 0, 1023, 1000, 2000);
  rc_data.ch4 = constrain(rc_data.ch4, 1000, 2000);
  
  rc_data.ch5 = map(analogRead(pot5), 0, 1023, 1000, 2000);
  rc_data.ch5 = constrain(rc_data.ch5, 1000, 2000);

//  Serial.println(rc_data.throttle); //print value ​​on a serial monitor  
}

//************************************************************************************************************************************************************************
//initial main settings **************************************************************************************************************************************************
//************************************************************************************************************************************************************************
void setup()
{ 
//  Serial.begin(9600);

  pinMode(led, OUTPUT);
  pinMode(inTXbatt, INPUT);
  
  //define the radio communication
  radio.begin();
  radio.setAutoAck(true);          //ensure autoACK is enabled (default true)
  radio.enableAckPayload();        //enable custom ack payloads on the acknowledge packets
  radio.enableDynamicPayloads();   //enable dynamically-sized payloads
  radio.setRetries(5, 5);          //set the number and delay of retries on failed submit (max. 15 x 250us delay (blocking !), max. 15 retries)

  radio.setChannel(76);            //which RF channel to communicate on (0-125, 2.4Ghz + default 76 = 2.476Ghz) 
  radio.setDataRate(RF24_250KBPS); //RF24_250KBPS (fails for units without +), RF24_1MBPS, RF24_2MBPS
  radio.setPALevel(RF24_PA_MIN);   //RF24_PA_MIN (-18dBm), RF24_PA_LOW (-12dBm), RF24_PA_HIGH (-6dbm), RF24_PA_MAX (0dBm)

  radio.stopListening();           //set the module as transmitter. Stop listening for incoming messages, and switch to transmit mode
  
  radio.openWritingPipe(addresses[1]);    //open a pipe for writing via byte array. Call "stopListening" first
  radio.openReadingPipe(1, addresses[0]); //open all the required reading pipes
}

//************************************************************************************************************************************************************************
//program loop ***********************************************************************************************************************************************************
//************************************************************************************************************************************************************************
void loop()
{ 
  receive_time();
  send_and_receive_data();
                                                            
  read_pots();

  battery_voltage();

} //end program loop

//************************************************************************************************************************************************************************
//after losing RF data or turning off the RX, gain time and the LED flashing *********************************************************************************************
//************************************************************************************************************************************************************************
unsigned long lastRxTime = 0;

void receive_time()
{
  if(millis() >= lastRxTime + 1000) //1000 (1second)
  {
    RFoff_indication();
  }
}

//************************************************************************************************************************************************************************
//send and receive data **************************************************************************************************************************************************
//************************************************************************************************************************************************************************
void send_and_receive_data()
{
  if (radio.write(&rc_data, sizeof(packet)))
  {
    if (radio.isAckPayloadAvailable())
    {
      radio.read(&payload, sizeof(ackPayload));
                                            
      lastRxTime = millis(); //at this moment we have received the data
      RXbatt_indication();                                          
    }                              
  } 
}

//************************************************************************************************************************************************************************
//input measurement TX battery voltage 1S LiPo (4.2V) < 3.3V = LED flash at a interval of 200ms. Battery OK = LED TX is lit **********************************************
//************************************************************************************************************************************************************************
unsigned long ledTime = 0;
int ledState, detect;
float TXbatt;

void battery_voltage()
{ 
  //------------------------------ TX battery --
  TXbatt = analogRead(inTXbatt) * (4.2 / 1023);

  //---------------- monitored voltage
  detect = TXbatt <= 3.3;

  if (detect)
  {
    TXbatt_indication();
  }
  
//  Serial.println(TXbatt); //print value ​​on a serial monitor 
}

//-----------------------------------------------------------
void TXbatt_indication()
{
  if (millis() >= ledTime + 200) //1000 (1second)
  {
    ledTime = millis();
    
    if (ledState)
    {
      ledState = LOW;
    }
    else
    {
      ledState = HIGH;
    }   
    digitalWrite(led, ledState);
  }
}

//************************************************************************************************************************************************************************
//after receiving RF data, the monitored RX battery is activated ********************************************************************************************************* 
//RX battery voltage 1S LiPo (4.2V) < 3.3V = LEDs TX, RX flash at a interval of 500ms. Battery OK = LEDs TX, RX is lit ***************************************************
//************************************************************************************************************************************************************************
void RXbatt_indication()
{ 
  //------------------------ monitored voltage
  detect = payload.RXbatt <= 3.3;
  
  if (millis() >= ledTime + 500) //1000 (1second)
  {
    ledTime = millis();
    
    if (ledState >= !detect + HIGH)
    {
      ledState = LOW;
    }
    else
    {
      ledState = HIGH;
    }   
    digitalWrite(led, ledState);
  }
}

//************************************************************************************************************************************************************************
//when TX is switched on and RX is switched off, or after the loss of RF data = LED TX flash at a interval of 100 ms. Normal mode = LED TX is lit ************************
//************************************************************************************************************************************************************************
void RFoff_indication()
{
  if (millis() >= ledTime + 100) //1000 (1second)
  {
    ledTime = millis();
    
    if (ledState)
    {
      ledState = LOW;
    }
    else
    {
      ledState = HIGH;
    }   
    digitalWrite(led, ledState);
  }
}
    
