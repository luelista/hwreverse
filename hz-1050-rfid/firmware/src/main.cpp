/*
 * Port B3: 125kHz square wave generation using alternate function OC2 (Timer/Counter2 Output Compare Match Output)
 */

#include <Arduino.h>

void setup()
{
  // initialize LED digital pin as an output.
  pinMode(PIN_PD2, OUTPUT);

  // Initialize Pin B3: 125kHz square wave generation using 
  // alternate function OC2 (Timer/Counter2 Output Compare Match Output)
  pinMode(PIN_PB3, OUTPUT);

  ASSR &= ~(1<<AS2);   // Reset AS2: Timer/Counter2 clocked from main system IO clock
  
  TCCR2 = 
    //Mode 2: WGM21=1  WGM20=0   CTC (Clear Timer On Compare Match)
    (1<<WGM21) | (0<<WGM20) |

    //COM21=0  COM20=1 Toggle OC2 on Compare Match
    (0<<COM21) | (1<<COM20) |

    //Clock Select:  CS22=0  CS21=0  CS20=1   No prescaling
    (0<<CS22) | (0<<CS21) | (1<<CS20);

  OCR2 = 32;    // = 8 MHz / 125kHz 



}

void loop()
{
  // turn the LED on (HIGH is the voltage level)
  digitalWrite(PIN_PD2, HIGH);
  // wait for a second
  delay(1000);
  // turn the LED off by making the voltage LOW
  digitalWrite(PIN_PD2, LOW);
   // wait for a second
  delay(1000);
}
