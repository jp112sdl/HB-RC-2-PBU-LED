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
#include <Dimmer.h>

//Pin Definitionen
#define CONFIG_BUTTON_PIN 8
#define WSLED_PIN         9      //GPIO Pin LED Anschluss
#define ONBOARD_LED_PIN1  14
#define ONBOARD_LED_PIN2  15
#define CC1101_CS         10
#define CC1101_GDO0       2
#define BTN1_PIN          16
#define BTN2_PIN          17

//Einstellungen für die RGB LEDs
#define WSNUM_LEDS    16          //Anzahl angeschlossener LEDs
#define WSLED_TYPE    WS2812B    //LED Typ
#define WSCOLOR_ORDER GRB        //Farbreihenfolge

uint8_t dim1level(0);
uint8_t dim2level(0);
uint8_t col1val(0);
uint8_t col2val(0);
CRGB leds[WSNUM_LEDS];


#define PEERS_PER_RGB_CHANNEL  3
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
typedef DimmerChannel<Hal, PEERS_PER_RGB_CHANNEL,List0> RGBLEDChannelType;
typedef DimmerAndRemoteDevice<Hal, RGBLEDChannelType, RemoteChannelType, 4, 1, 2, List0> RCLEDDevice;

// we need no PWM class
class DummyPWM {
public:
  void init(uint8_t __attribute__ ((unused)) pwm) {}
  void set(uint8_t __attribute__ ((unused)) pwm) {}
};

template<class HalType,class DimmerType,class PWM>
class RGBControl : public DimmerControl<HalType,DimmerType,PWM> {
public:
  typedef DimmerControl<HalType,DimmerType,PWM> BaseControl;
  RGBControl (DimmerType& dim) : BaseControl(dim) {
    for (int i = 0; i < WSNUM_LEDS; i++) {
      leds[i] = CRGB::Black;
    }
    FastLED.addLeds<WSLED_TYPE, WSLED_PIN, WSCOLOR_ORDER>(leds, WSNUM_LEDS);
    FastLED.setBrightness(255);
  }

  virtual ~RGBControl () {}

  virtual void updatePhysical () {
    // first calculate all physical values of the dimmer channels
    BaseControl::updatePhysical();
    // set brightness and color to LEDs
    uint8_t ledprocombi = WSNUM_LEDS / this->physicalCount() * 2;
    uint8_t ledidx = 0;
    for( uint8_t i=0; i<this->physicalCount();  ) {
      uint8_t dimlevel = this->physical[i++];
      uint8_t collevel = this->physical[i++];
      CRGB ledvalue = CHSV((collevel * 1275L) / 1000, (collevel <  200) ? 255 : 0, dimlevel);
      // set to real LEDs
      for( uint8_t y=0; y<ledprocombi; ++y) {
        leds[ledidx++] = ledvalue;
      }
    }
    FastLED.show();
  }
};

RCLEDDevice sdev(devinfo, 0x20);
RGBControl<Hal,RCLEDDevice,DummyPWM> control(sdev);
ConfigButton<RCLEDDevice> cfgBtn(sdev);

void setup () {
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  control.init(hal,0,0,0,0); // all 4 PWM pins are 0
  remoteChannelISR(sdev.remoteChannel(1), BTN1_PIN);
  remoteChannelISR(sdev.remoteChannel(2), BTN2_PIN);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
    hal.activity.savePower<Idle<true>>(hal);
  }
}

