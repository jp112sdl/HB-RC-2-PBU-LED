//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2019-08-26 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <SPI.h>
#include <AskSinPP.h>
#include <LowPower.h>
#include <Register.h>
#include <MultiChannelDevice.h>
#include <FastLED.h>
#include <Remote.h>
#include <Switch.h>

//Pin Definitionen
#define CONFIG_BUTTON_PIN 8
#define WSLED_PIN         9      //GPIO Pin LED Anschluss
#define ONBOARD_LED_PIN1  A2
#define ONBOARD_LED_PIN2  A1
#define CC1101_CS         10
#define CC1101_GDO0       2
#define BTN1_PIN          5
#define BTN2_PIN          6

//Einstellungen für die RGB LEDs
#define WSNUM_LEDS    4          //Anzahl angeschlossener LEDs
#define WSLED_TYPE    WS2812B    //LED Typ
#define WSCOLOR_ORDER GRB        //Farbreihenfolge

uint8_t dim1level(0);
uint8_t dim2level(0);
uint8_t col1val(0);
uint8_t col2val(0);
CRGB leds[WSNUM_LEDS];

#include "RGBLEDChannel.h"

#define PEERS_PER_RGB_CHANNEL  8
#define PEERS_PER_RC_CHANNEL   12

using namespace as;

const struct DeviceInfo PROGMEM devinfo = {
  {0xF3, 0x30, 0x00},     // Device ID
  "JPRC2LED01",           // Device Serial
  {0xF3, 0x30},           // Device Model
  0x10,                   // Firmware Version
  as::DeviceType::Dimmer, // Device Type
  {0x01, 0x00}            // Info Bytes
};

typedef AskSin<DualStatusLed<ONBOARD_LED_PIN1, ONBOARD_LED_PIN2>, NoBattery, Radio<LibSPI<CC1101_CS>, CC1101_GDO0>> Hal;
Hal hal;

typedef RemoteChannel<Hal, PEERS_PER_RC_CHANNEL, List0> RemoteChannelType;
typedef RGBLEDChannel<Hal, PEERS_PER_RGB_CHANNEL,List0> RGBLEDChannelType;

class RCLEDDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, List0>, 6, List0> {
  public:
    VirtChannel<Hal, RemoteChannelType , List0> c1, c2;
    VirtChannel<Hal, RGBLEDChannelType , List0> c3, c4, c5, c6;
  public:
    typedef ChannelDevice<Hal, VirtBaseChannel<Hal, List0>, 6, List0> DeviceType;
    RCLEDDevice (const DeviceInfo& info, uint16_t addr) : DeviceType(info, addr) {
      DeviceType::registerChannel(c1, 1);
      DeviceType::registerChannel(c2, 2);
      DeviceType::registerChannel(c3, 3);
      DeviceType::registerChannel(c4, 4);
      DeviceType::registerChannel(c5, 5);
      DeviceType::registerChannel(c6, 6);
    }
    virtual ~RCLEDDevice () {}

    RemoteChannelType& channel (uint8_t num)  {
      switch (num) {
        case 1:
          return c1;
          break;
        case 2:
          return c2;
          break;
      }
      return c1;
    }

    RGBLEDChannelType& Dim1Channel ()  {
      return c3;
    }
    RGBLEDChannelType& Col1Channel ()  {
      return c4;
    }
    RGBLEDChannelType& Dim2Channel ()  {
      return c5;
    }
    RGBLEDChannelType& Col2Channel ()  {
      return c6;
    }

    virtual void configChanged () {
      DeviceType::configChanged();
      for (int i = 0; i < WSNUM_LEDS; i++) {
        leds[i] = CRGB::Black;
      }
      FastLED.addLeds<WSLED_TYPE, WSLED_PIN, WSCOLOR_ORDER>(leds, WSNUM_LEDS);
      FastLED.setBrightness(255);
    }

    void init (Hal& hal) {
      DeviceType::init(hal);

      Dim1Channel().setLevel(0, 0, 0xffff);
      Dim2Channel().setLevel(0, 0, 0xffff);
      Col1Channel().setColor(0);
      Col2Channel().setColor(0);

      Dim1Channel().changed(true);
      Dim2Channel().changed(true);
      Col1Channel().changed(true);
      Col2Channel().changed(true);
    }

    void updateLedValues() {
      uint8_t ledsHalf = WSNUM_LEDS / 2;
      for (uint8_t i = 0; i < WSNUM_LEDS; i++) {
        if (i < ledsHalf)
          leds[i] = CHSV((col1val * 1275L) / 1000, (col1val <  200) ? 255 : 0, dim1level);
        else
          leds[i] = CHSV((col2val * 1275L) / 1000, (col2val <  200) ? 255 : 0, dim2level);
      }
      FastLED.show();
    }
};

RCLEDDevice sdev(devinfo, 0x20);
ConfigButton<RCLEDDevice> cfgBtn(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  remoteISR(sdev, 1, BTN1_PIN);
  remoteISR(sdev, 2, BTN1_PIN);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    hal.activity.savePower<Idle<true>>(hal);
  }
  sdev.updateLedValues();
}
