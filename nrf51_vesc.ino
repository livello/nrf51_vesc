#include <Arduino.h>
#include <SPI.h>
#include "Adafruit_BLE.h"
#include "Adafruit_BluefruitLE_SPI.h"
//#include "Adafruit_BluefruitLE_UART.h"

#include "BluefruitConfig.h"

//#if SOFTWARE_SERIAL_AVAILABLE
//#include <SoftwareSerial.h>
//#endif

#include "vesc_packet.h"
#include "crc.h"
#include "vesc_buffer.h"
#include "vesc_datatypes.h"

#include "VescUart.h"
#include <Adafruit_NeoPixel.h>

#define DEBUG 1
#define PIN         5 // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS   32
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

VescUart UART;



#define PACKET_VESC            0
#define PACKET_BLE            1

/*=========================================================================
    APPLICATION SETTINGS

      FACTORYRESET_ENABLE       Perform a factory reset when running this sketch
     
                                Enabling this will put your Bluefruit LE module
                              in a 'known good' state and clear any config
                              data set in previous sketches or projects, so
                                running this at least once is a good idea.
     
                                When deploying your project, however, you will
                              want to disable factory reset by setting this
                              value to 0.  If you are making changes to your
                                Bluefruit LE device via AT commands, and those
                              changes aren't persisting across resets, this
                              is the reason why.  Factory reset will erase
                              the non-volatile memory where config data is
                              stored, setting it back to factory default
                              values.
         
                                Some sketches that require you to bond to a
                              central device (HID mouse, keyboard, etc.)
                              won't work at all with this feature enabled
                              since the factory reset will clear all of the
                              bonding data stored on the chip, meaning the
                              central device won't be able to reconnect.
    MINIMUM_FIRMWARE_VERSION  Minimum firmware version to have some new features
    MODE_LED_BEHAVIOUR        LED activity, valid options are
                              "DISABLE" or "MODE" or "BLEUART" or
                              "HWUART"  or "SPI"  or "MANUAL"
    -----------------------------------------------------------------------*/
#define FACTORYRESET_ENABLE         0
#define MINIMUM_FIRMWARE_VERSION    "0.6.6"
#define MODE_LED_BEHAVIOUR          "MODE"
/*=========================================================================*/

// Create the bluefruit object, either software serial...uncomment these lines
/*
  SoftwareSerial bluefruitSS = SoftwareSerial(BLUEFRUIT_SWUART_TXD_PIN, BLUEFRUIT_SWUART_RXD_PIN);

  Adafruit_BluefruitLE_UART ble(bluefruitSS, BLUEFRUIT_UART_MODE_PIN,
                      BLUEFRUIT_UART_CTS_PIN, BLUEFRUIT_UART_RTS_PIN);
*/

/* ...or hardware serial, which does not need the RTS/CTS pins. Uncomment this line */
// Adafruit_BluefruitLE_UART ble(BLUEFRUIT_HWSERIAL_NAME, BLUEFRUIT_UART_MODE_PIN);

/* ...hardware SPI, using SCK/MOSI/MISO hardware SPI pins and then user selected CS/IRQ/RST */
Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_CS, BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);

/* ...software SPI, using SCK/MOSI/MISO user-defined SPI pins and then user selected CS/IRQ/RST */
//Adafruit_BluefruitLE_SPI ble(BLUEFRUIT_SPI_SCK, BLUEFRUIT_SPI_MISO,
//                             BLUEFRUIT_SPI_MOSI, BLUEFRUIT_SPI_CS,
//                             BLUEFRUIT_SPI_IRQ, BLUEFRUIT_SPI_RST);






bool blePassMode = true;
long lastTime_COMM_GET_VALUES = 0;

long lastMainloopMillis = 0;
long thisMainLoopMillis = 0;
long lastBLEms = 0;
long lastDataMillis = 0;

bool sleepMode = false;




// A small helper
void error(const __FlashStringHelper*err) {
  Serial.println(err);
  while (1);
}


//=================================================================================================


void ble_send_buffer(unsigned char *data, unsigned int len) {
  // Serial.println("vesc -> ble");
  for (int i = 0; i < len; i++) {
    ble.write(data[i]);
    
    // Serial.print(data[i]);
    // Serial.print(",");
  }
  // Serial.println();
  UART.processReadPacket(data);
  Serial.println("rpm: ");
  Serial.println(UART.data.rpm);
}

void uart_send_buffer(unsigned char *data, unsigned int len) {
  // Serial.println("ble -> vesc");
  for (int i = 0; i < len; i++) {
    Serial1.write(data[i]);
    
    // Serial.print(data[i]);
    // Serial.print(",");
  }
  // Serial.println();
}


/**
   @brief  ble -> VESC
   these process function is called when the try_decode_packet() is succeed.
   @param  data :
   @param  len :
*/
void process_packet_ble(unsigned char *data, unsigned int len) {
  packet_send_packet(data, len, PACKET_VESC);
  lastDataMillis = thisMainLoopMillis;
}

/**
   @brief  VESC -> BLE

   @param  data :
   @param  len :
*/
void process_packet_vesc(unsigned char *data, unsigned int len) {
  packet_send_packet(data, len, PACKET_BLE);
  
}


/**************************************************************************/
/*!
    @brief  Sets up the HW an the BLE module (this function is called
            automatically on startup)
*/
/**************************************************************************/
void setup(void)
{
  packet_init(uart_send_buffer, process_packet_vesc, PACKET_VESC);
  packet_init(ble_send_buffer, process_packet_ble, PACKET_BLE);

  Serial.begin(115200);
  Serial1.begin(115200);
  Serial.println(F("Adafruit Bluefruit Command <-> Data Mode Example"));
  Serial.println(F("------------------------------------------------"));

  /* Initialise the module */
  Serial.print(F("Initialising the Bluefruit LE module: "));

  if ( !ble.begin(VERBOSE_MODE) )
  {
    error(F("Couldn't find Bluefruit, make sure it's in CoMmanD mode & check wiring?"));
  }
  Serial.println( F("OK!") );

  if ( FACTORYRESET_ENABLE )
  {
    /* Perform a factory reset to make sure everything is in a known state */
    Serial.println(F("Performing a factory reset: "));
    if ( ! ble.factoryReset() ) {
      error(F("Couldn't factory reset"));
    }
  }
  ble.sendCommandCheckOK("AT+GAPDEVNAME=CHANS_VESC");

  /* Disable command echo from Bluefruit */
  ble.echo(false);

  Serial.println("Requesting Bluefruit info:");
  /* Print Bluefruit information */
  ble.info();

  Serial.println(F("Please use Adafruit Bluefruit LE app to connect in UART mode"));
  Serial.println(F("Then Enter characters to send to Bluefruit"));
  Serial.println();

  ble.verbose(false);  // debug info is a little annoying after this point!


  Serial.println(F("******************************"));

  // LED Activity command is only supported from 0.6.6
  if ( ble.isVersionAtLeast(MINIMUM_FIRMWARE_VERSION) )
  {
    // Change Mode LED Activity
    Serial.println(F("Change LED activity to " MODE_LED_BEHAVIOUR));
    ble.sendCommandCheckOK("AT+HWModeLED=" MODE_LED_BEHAVIOUR);
  }
  ble.sendCommandCheckOK("AT+BAUDRATE=115200");


  // Set module to DATA mode
  Serial.println( F("Switching to DATA mode!") );
  ble.setMode(BLUEFRUIT_MODE_DATA);

  Serial.println(F("******************************"));


  // VESC Serial Port Set
  UART.setSerialPort(&Serial1);
  pixels.begin();

}



void loop(void)
{
  thisMainLoopMillis = millis();
  //  standalone mode without BLE
  if (thisMainLoopMillis - lastBLEms > 5000) {
    blePassMode  = false;
  }

  // sleep mode
  if (thisMainLoopMillis - lastDataMillis  > 10000) {
    sleepMode = true;
  }


  // HC12->BLE =========================================================
  while (Serial1.available())
  {
    packet_process_byte((unsigned char)Serial1.read(), PACKET_VESC);
    //ble.write(Serial.read());
    sleepMode = false;
  }

  // BLE->HC12 ===================================================================
  while ( ble.available() )
  {
    unsigned char c = ble.read();
    packet_process_byte(c, PACKET_BLE);
    lastBLEms = thisMainLoopMillis;

    lastDataMillis = thisMainLoopMillis;
    sleepMode = false;
    blePassMode = true;
  }


  spin(20);

}


// SPIN ======================================================================================

void spin(const int desiredMillis) {
  static long lastSpinTime;

  if (thisMainLoopMillis - lastSpinTime >  desiredMillis) {
    lastSpinTime = thisMainLoopMillis;
    looping_function();

    if (sleepMode)
    {
      sleep();
    }
  }
}


int brakeState = 0;
enum BRAKE_CASE  {NO_BRAKE, NEUTRAL, ACCELERATION, BRAKE_DECELERATION, BRAKE_REVERSE };
int before_brake_status = 0;

bool blePassModeFirstTime = true;

int ledCount = 0;
long lastRequestMillis;

void looping_function() {
  if (thisMainLoopMillis - lastDataMillis > 50 && !blePassMode && thisMainLoopMillis - lastRequestMillis > 50) {
    lastRequestMillis = thisMainLoopMillis;
    UART.requestVescGetValues();

#ifdef DEBUG
    Serial.println("request data manually");
    // Serial.println(thisMainLoopMillis - lastDataMillis);
#endif

  }

  if (blePassMode) {
    if (blePassModeFirstTime) {
      blePassModeFirstTime = false;
      setColor(100, 5, 5);
      pixels.show();
    }
  } else {
    blePassModeFirstTime = true;
  }

  //  check_brake();

}


void setColor(int r, int g, int b) {
  for (int i = 0; i < NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  }

}

void sleep() {
  Serial.println("Sleep Mode");
  for (int i = 0; i < 10; i ++) {
    setColor(i, i / 10, i / 10);
    pixels.show();
    delay(10);
  }
  for (int i = 10; i >= 0; i --) {
    setColor(i, i / 10, i / 10);
    delay(10);
    pixels.show();
  }
  delay(3000);
}

void enterBLEModeLED() {
}
