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

#define WSNUM_LEDS    4          //Anzahl angeschlossener LEDs
#define WSLED_PIN     7          //GPIO Pin LED Anschluss
#define WSLED_TYPE    WS2812B    //LED Typ
#define WSCOLOR_ORDER GRB        //Farbreihenfolge

#define CONFIG_BUTTON_PIN 8
#define ONBOARD_LED_PIN   4
#define CC1101_CS         10
#define CC1101_GDO0       2
#define BTN1_PIN          9
#define BTN2_PIN          6

uint8_t dim1level(0);
uint8_t dim2level(0);
uint8_t col1val(0);
uint8_t col2val(0);
CRGB leds[WSNUM_LEDS];

#include "RGBLEDChannel.h"

#define PEERS_PER_CHANNEL 3

#define remISR(device,chan,pin) class device##chan##ISRHandler { \
    public: \
      static void isr () { device.remoteChannel(chan).irq(); } \
  }; \
  device.remoteChannel(chan).button().init(pin); \
  if( digitalPinToInterrupt(pin) == NOT_AN_INTERRUPT ) \
    enableInterrupt(pin,device##chan##ISRHandler::isr,CHANGE); \
  else \
    attachInterrupt(digitalPinToInterrupt(pin),device##chan##ISRHandler::isr,CHANGE);

using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xF3, 0x30, 0x00},     // Device ID
  "JPRC2LED01",           // Device Serial
  {0xF3, 0x30},           // Device Model
  0x10,                   // Firmware Version
  as::DeviceType::Dimmer, // Device Type
  {0x01, 0x00}            // Info Bytes
};

/**
   Configure the used hardware
*/
typedef AskSin<StatusLed<ONBOARD_LED_PIN>, NoBattery, Radio<LibSPI<CC1101_CS>, CC1101_GDO0>> Hal;
Hal hal;

DEFREGISTER(Ws28xxReg0, MASTERID_REGS)
class Ws28xxList0 : public RegList0<Ws28xxReg0> {
  public:
    Ws28xxList0(uint16_t addr) : RegList0<Ws28xxReg0>(addr) {}
    void defaults () {
      clear();
    }
};

DEFREGISTER(RemoteReg1, CREG_LONGPRESSTIME, CREG_AES_ACTIVE, CREG_DOUBLEPRESSTIME)
class RemoteList1 : public RegList1<RemoteReg1> {
  public:
    RemoteList1 (uint16_t addr) : RegList1<RemoteReg1>(addr) {}
    void defaults () {
      clear();
      longPressTime(1);
      // aesActive(false);
      // doublePressTime(0);
    }
};
class RemoteChannelType : public Channel<Hal, RemoteList1, EmptyList, DefList4, PEERS_PER_CHANNEL, Ws28xxList0>, public Button {
  private:
    uint8_t       repeatcnt;

  public:
    typedef Channel<Hal, RemoteList1, EmptyList, DefList4, PEERS_PER_CHANNEL, Ws28xxList0> BaseChannel;

    RemoteChannelType () : BaseChannel() {}
    virtual ~RemoteChannelType () {}

    Button& button () {
      return *(Button*)this;
    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }

    virtual void state(uint8_t s) {
      DHEX(BaseChannel::number());
      Button::state(s);
      RemoteEventMsg& msg = (RemoteEventMsg&)this->device().message();
      msg.init(this->device().nextcount(), this->number(), repeatcnt, (s == longreleased || s == longpressed), false);
      if ( s == released || s == longreleased) {
        this->device().sendPeerEvent(msg, *this);
        repeatcnt++;
      }
      else if (s == longpressed) {
        this->device().broadcastPeerEvent(msg, *this);
      }
    }

    uint8_t state() const {
      return Button::state();
    }

    bool pressed () const {
      uint8_t s = state();
      return s == Button::pressed || s == Button::debounce || s == Button::longpressed;
    }
};


typedef RGBLEDChannel<Hal, PEERS_PER_CHANNEL, Ws28xxList0> RGBLEDChannelType;

class RCLEDDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, Ws28xxList0>, 6, Ws28xxList0> {
  public:
    VirtChannel<Hal, RemoteChannelType , Ws28xxList0> c1, c2;
    VirtChannel<Hal, RGBLEDChannelType , Ws28xxList0> c3, c4, c5, c6;
  public:
    typedef ChannelDevice<Hal, VirtBaseChannel<Hal, Ws28xxList0>, 6, Ws28xxList0> DeviceType;
    RCLEDDevice (const DeviceInfo& info, uint16_t addr) : DeviceType(info, addr) {
      DeviceType::registerChannel(c1, 1);
      DeviceType::registerChannel(c2, 2);
      DeviceType::registerChannel(c3, 3);
      DeviceType::registerChannel(c4, 4);
      DeviceType::registerChannel(c5, 5);
      DeviceType::registerChannel(c6, 6);
    }
    virtual ~RCLEDDevice () {}

    RemoteChannelType& remoteChannel (uint8_t num)  {
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
  remISR(sdev, 1, BTN1_PIN);
  remISR(sdev, 2, BTN2_PIN);
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
