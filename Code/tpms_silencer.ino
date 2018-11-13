#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define PIN_EN        A0   // PA0, Pin 13, Arduino 10
#define PIN_FSK       A1   // PA1, Pin 12, Arduino 9
#define PIN_ASK       A2   // PA2, Pin 11, Arduino 8

#define FSKLOW        (bitSet(PORTA, 1))
#define FSKHIGH       (bitClear(PORTA, 1))
#define FSKTOGGLE     (PORTA = PORTA ^ _BV(1))

#define PACKETSIZE    144

#define PACKET_DELAY  250   // Milliseconds between packets
#define WAKEUPCOUNT   4     // How make 8-second wakeups before transmit

const char PROGMEM packetOne[] = "111111001100110101010101010101010100101010110100110101001011001010101101010100101010101101010100110011010011010010101011010101001100110100101100";
const char PROGMEM packetTwo[] = "111111001011001100101011001100101010101101001011001011010101010101001010101011010101010100110100110011001010101101010101001100101010101010101100";
const char PROGMEM packetFour[] = "111111001100110010101010101011010010101010110100110101001011001101010010101011010101010101010100110011001100101101010101010011001100110101001100";
const char PROGMEM packetThree[] = "111111001011001010110011010010101100110101001011001010110100101101001101010100101010101011010100110011001011010010101011010101001101010010101100";

// Allocate the memory
char currentPacket[] = "111111001100110010101010101011010010101010110100110101001011001101010010101011010101010101010100110011001100101101010101010011001100110101001100";

volatile unsigned int currentPos;
volatile bool transmitting = false;
volatile int wakeupCounter = 0;


// Interrupt for 10khz for an 8MHz clock
void setupInterrupt8()
{
  noInterrupts();
  // Clear registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // 10000 Hz (8000000/((99+1)*8))
  OCR1A = 99;
  // CTC
  TCCR1B |= (1 << WGM12);
  // Prescaler 8
  TCCR1B |= (1 << CS11);
  // Output Compare Match A Interrupt Enable
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

// Interrupt for 10khz for a 16MHz clock
void setupInterrupt16()
{
  noInterrupts();
  // Clear registers
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // 10000 Hz (16000000/((24+1)*64))
  OCR1A = 24;
  // CTC
  TCCR1B |= (1 << WGM12);
  // Prescaler 64
  TCCR1B |= (1 << CS11) | (1 << CS10);
  // Output Compare Match A Interrupt Enable
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

void setup()
{
  // TODO: Unsure if INPUT or INPUT_PULLUP gives lowest power consumption.
  for (byte i = 0; i < 13; i++)
    pinMode(i, INPUT);

  power_adc_disable();

  pinMode(PIN_EN, OUTPUT);
  pinMode(PIN_FSK, OUTPUT);
  pinMode(PIN_ASK, OUTPUT);

  digitalWrite(PIN_ASK, HIGH);    // Failing to set ASK to high will result in a bad day. (No signal)

  disableTX();

#if F_CPU == 16000000L
  setupInterrupt16();
#elif F_CPU == 8000000L
  setupInterrupt8();
#else
#error CPU is not set to 16MHz or 8MHz!
#endif
}


ISR(TIMER1_COMPA_vect)
{
  if (transmitting)
  {
    if (currentPacket[currentPos] == '1')
      sendRawOne();
    else
      sendRawZero();

    if (++currentPos > PACKETSIZE)
    {
      transmitting = false;
      disableTX();
    }
  }

}

ISR(WDT_vect)
{
  wakeupCounter++;
  wdt_disable();  // disable watchdog
}

void sleepyTime()
{
  // Just in case
  disableTX();
  power_adc_disable();
  // clear various "reset" flags
  MCUSR = 0;
  // allow changes, disable reset
  WDTCSR = bit(WDE);
  // set interrupt mode and an interval 
  WDTCSR = bit(WDIE) | bit(WDP3) | bit(WDP0);    // set WDIE, and 8 seconds delay
  wdt_reset();  // pat the dog

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  noInterrupts();           // timed sequence follows
  sleep_enable();

  interrupts();             // guarantees next instruction executed
  sleep_cpu();
}

void sendPacket(const char *thePacket)
{
  memcpy_P(currentPacket, thePacket, PACKETSIZE);

  currentPos = 0;

  enableTX();
  delay(1);
  transmitting = true;
}

void loop()
{
  if (wakeupCounter > WAKEUPCOUNT)
  {
    wakeupCounter = 0;

    sendPacket((char *)packetOne);
    while (transmitting)
      __asm__("nop\n\t");
    delay(PACKET_DELAY);

    sendPacket(packetTwo);
    while (transmitting)
      __asm__("nop\n\t");
    delay(PACKET_DELAY);

    sendPacket(packetThree);
    while (transmitting)
      __asm__("nop\n\t");
    delay(PACKET_DELAY);

    sendPacket(packetFour);
    while (transmitting)
      __asm__("nop\n\t");
    delay(PACKET_DELAY);
  }

  sleepyTime();

}

void sendRawOne()
{
  FSKHIGH;
}

void sendRawZero()
{
  FSKLOW;
}


void enableTX()
{
  FSKLOW;
  //bitSet(PORTA, 0);
  digitalWrite(PIN_EN, HIGH);
}

void disableTX()
{
  digitalWrite(PIN_EN, LOW);
  //bitClear(PORTA, 0);
  FSKLOW;
}