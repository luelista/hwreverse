// source: https://github.com/Janesak1977/RFIDCloner/blob/main/RFIDCloner.ino

// RFID Cloner for HID ISOprox II card 

#include <SPI.h>
#include "pitches.h"

// DEBUG Compile
//#define dbg

#define VERSION   "0.85"

#define Fc        125000    // cycle frequency [Hz]
#define Tc        8         // cycle [us]
#define d0        24        // data 0 coding time in Tc units
#define d1        54        // data 1 coding time in Tc units
#define WriteGap  19        // Write gap time in Tc units
#define StartGap  30        // Start gap time in Tc units

#define FREQGEN   3			    // output of Timer2 OC2B (Arduino pin D3)
//#define LED       13        // LED on pin 13 on UNO
//#define LEDPORT   PORTB
//#define LEDPIN    PORTB5
#define BTNUP     A0        // Upper Button
#define BTNDN     A1        // Lower Button
#define BUZZER    6         // Buzzer

#define BTNPORT   PINC     // arduino A0,A1 -> PIN0,PINC1

//#define LEDON()   PORTB |= _BV(PORTB5)
//#define LEDOFF()  PORTB &= ~(_BV(PORTB5))  

#define T5577CFG  0x00107060    // T5577 configuration for emulating HID card is 0x00107060 (0b00000000000100000111000001100000), Modulation=FKS2a, DataBitRate=RF/50, MaxBlock=3

#define ENA125kHz TCCR2A |= _BV(COM2B0)   // Enable generating 125kHz frequency
#define DIS125kHz TCCR2A &= 0b11001111    // Disable generating 125kHz frequency

#define DISINCAPTINT TIMSK1 &= ~(1<<ICIE1)   // Disables Input capture interrupt

#define BITSTREAMARRAYSIZE 550
//#define BITSTREAMARRAYSIZE 800

// OLED Display defines
#define OLEDENABLE

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
// Declaration for SSD1306 display connected using software SPI (default case):
#define OLED_DC     9
#define OLED_CS     10
#define OLED_RST    7

#ifdef OLEDENABLE
#include "SH1106.h"
#include "glcdfont.h"
#endif

typedef enum {Start, CardReaded, ErrNoReadedCard, Cloning, CardCloned} states_t;

uint8_t T5577Clear[12] = {0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55,0x55};

bool OrigCardReaded = false;
uint16_t CurrPeriod = 0;
uint16_t PrevVal = 0;
uint8_t BitstreamArray[BITSTREAMARRAYSIZE];
volatile uint8_t* BitstreamPtrStart;
volatile uint8_t* CurrBitstreamPtr;
volatile uint8_t* BitstreamPtrEnd;
uint8_t reduced_array[90];
uint8_t final_code[45];
uint8_t final_code_binary[6];
uint8_t manchester_code_binary[12];   // manchester code in binary form
uint8_t bitstream_index = 0;        // points to actual byte in bitstream array
uint16_t bitinbitstream = 0;         // point to actual bit in bistream array
volatile uint16_t Index = 0;
volatile bool EnableStore = false;
volatile bool Readed = false;
volatile uint8_t HaveStart = 0;
volatile uint8_t CountofONEs = 0;
uint16_t start_sequence;
uint16_t end_sequence;
uint16_t cardnumber = 0;
uint8_t lastBtnState = 0;
uint8_t buttonState;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers [ms]
uint8_t buttons = 0;
states_t State = Start;;

volatile uint16_t DbgPtrVal1 = 0;
volatile uint16_t DbgPtrVal2 = 0;
volatile uint16_t DbgPtrVal3 = 0;
volatile uint8_t  DbgVal = 0;
volatile uint8_t  Dbg15moreONEs = 0;





// each time the input capture interrupt is triggered, a falling edge was detected store the value
ISR(TIMER1_CAPT_vect)
{ 
  uint16_t CurrVal;
  uint16_t period;
  uint8_t actualbit = 255;

  CurrVal = ICR1;
  period = CurrVal - PrevVal;

  /*
  // for FSK log 0 period is 64us(1024 TMR1 ticks), for FSK log 1 period is 80us(1280)
  // middle is 72us(1152), so everything below 1152 is FSKlog0 and everything above 1152 is FSKlog1

  if ((period >= 960) && (period <= 1136))    // if period >= 60us and period <= 71us we have FSKlog0
  {
    actualbit = 0;
    bitstream[bitinbitstream / 8] =<< 1;    // "shift in" 0 into bitstream array
    
  }
  else
  if ((period >= 1168 ) && (period <= 1344))    // if period >= 73us and period <= 84us we have FSKlog0
  {
    actualbit = 1;
    bitstream[bitinbitstream / 8] = (bitstream[bitinbitstream / 8] | 0x01) << 1;    // "shift in" 1 into bitstream array
  }

  bitinbitstream++;
  */


  if ((period >= 960) && (period <= 1152))    // if period >= 60us and period <= 72us we have FSKlog0
  {
    actualbit = 0;
    //LEDOFF();
  }
  else
  if ((period >= 1152 ) && (period <= 1344))    // if period >= 72us and period <= 84us we have FSKlog1
  {
    actualbit = 1;
    //LEDON();
  }
  else
    actualbit = 255;  


  if (EnableStore == true)
  { 
    //PeriodValues[Index++] = period;
    //BitstreamArray[Index++] = actualbit; 

    if (actualbit == 1)   // ONE
    {
          if (CountofONEs == 14)
          {
            HaveStart = 1;        // succesfully received Start (14 ones in row)
          }
          
          if ((HaveStart == 1) && (CountofONEs >= 15))      // if we have more than 15 ONE's in a row, start over
          {
             CountofONEs = 0;
             HaveStart = 0;
             CurrBitstreamPtr = BitstreamPtrStart;    // Set pointer to start of buffer
          }
          else          
          {
            CountofONEs++;            
            *CurrBitstreamPtr = actualbit;   // Store actual bit
            CurrBitstreamPtr++; 
          }
    }
    else    
    if (actualbit == 0)   // ZERO
    {
      CountofONEs = 0;

      if (HaveStart == 1)               // Store zeros only if we received correct Start
      {
        *CurrBitstreamPtr = actualbit;
        CurrBitstreamPtr++;        
      }
      else
        CurrBitstreamPtr = BitstreamPtrStart;   // else reset pointer to buffer start
    }

    if (CurrBitstreamPtr > BitstreamPtrEnd)
    {
      Readed = true;
      EnableStore = false;
      HaveStart = 0;
      CountofONEs = 0;      
      CurrBitstreamPtr = BitstreamPtrStart;
    }
  }

  PrevVal = CurrVal;
}




void TxBitRfid(byte data)
{
  if (data & 1)
  {
    delayMicroseconds(d1*Tc);
  }
  else
  {
    delayMicroseconds(d0*Tc);
  }
    
  rfidGap(WriteGap*Tc);                       //write gap
}


void TxByteRfid(byte data)
{
  for (byte n_bit = 0; n_bit < 8; n_bit++)
  {
    TxBitRfid(data & 1);
    data = data >> 1;                   // move on to the next bit
  }
}


void rfidGap(unsigned int tm)
{
  TCCR2A &= 0b11001111;                 // disable PWM COM2B
  //LEDON();
  delayMicroseconds(tm);
  TCCR2A |= _BV(COM2B0);                // Enable PWM COM2B (pin D3)
  //LEDOFF();
}


bool sendOpT5557(byte opCode, unsigned long password = 0, byte lockBit = 0, unsigned long data = 0, byte blokAddr = 1)
{
  TxBitRfid(opCode >> 1);
  TxBitRfid(opCode & 1);                // send opcode

  if (opCode == 0b00)
    return true;
    
  TxBitRfid(lockBit & 1);               // lockbit 0
  if (data != 0)
  {
    for (byte i = 0; i < 32; i++) 
    {
      TxBitRfid((data >> (31-i)) & 1);
    }
  }
  TxBitRfid(blokAddr >> 2);
  TxBitRfid(blokAddr >> 1);
  TxBitRfid(blokAddr & 1);              // block address to write
  delay(4);                             // wait for data to be written
  return true;
}


bool write2rfidT5557(uint8_t* buf)
{
  bool result;
  uint32_t data32;

  delay(6);
  for (uint8_t k = 0; k < 3; k++)                                    // send data
  {
    data32 = (uint32_t)buf[0 + (k<<2)] << 24 | (uint32_t)buf[1 + (k<<2)] << 16 | (uint32_t)buf[2 + (k<<2)] << 8 | (uint32_t)buf[3 + (k<<2)];
    rfidGap(30 * 8);                                                 // start gap
    sendOpT5557(0b10, 0, 0, data32, k+1);                            // write 32 bits of data to blok k
    //Serial.write('*');
    delay(6);
  }
  delay(6);
  rfidGap(30 * 8);          // start gap
  sendOpT5557(0b00);        // reset opcode
  return 1;
}


/* Function: Find Start Code
   To find the start code, we must look for a sequence of 15 to 18 1's.
   We run through the bit stream received from the card and look for 1's.
   Anytime there is a 0, we reset the counter. Eventually the counter 
   will get to 15 or more and then we have detected the start sequence.
   Since we know there are 540 bits in a period of the card response, we
   know the end of the sequence is 539 bits later.
*/
/*
void find_start_code(uint8_t* BitstreamPtr)
{
    int i = 0;
    char sequence = 0;
    char count = 0;
    start_sequence = 0;
    end_sequence = 0;
    while ((i < BITSTREAMARRAYSIZE))
    {
        //if (BitstreamArray[i] == 1)
        if (*(BitstreamPtr+i) == 1)
        {
            sequence = 1;
            count++;        
        }
        else
        {
            if ((sequence == 1) && (count >= 15))
            {
                start_sequence = i - count;
                end_sequence = start_sequence + 539;
                i = BITSTREAMARRAYSIZE + 1;
            }
            sequence = 0;
            count = 0;
        }
        
        i++;
    }
    
    if (i == BITSTREAMARRAYSIZE)
      start_sequence = -1;
}
*/

/* Function: FSK decode
   Once we have the 540 bit card response, we need to remove
   the redundancy. The code looks something similar to:
   00000111111000000111110000000000001111110000011111...
   We recognize 5 to 6 bits as a single bit and a stream of 
   10, 11, or 12 bits is two bits. Thus the code above would
   decode to 010100101. There are 90 bits in the reduced form. 
   This function transforms the redundant bit stream to the 
   reduced form by detecting sequences of 5/6 and 10/11/12 
   and creating a new array.
   Returns: 0 - OK, 1 - Error
*/

uint8_t FSK_decode(volatile uint8_t* BitstreamPtr)
{
    uint16_t i = 0;
    int8_t j = -1;
    uint8_t count;
    uint8_t value;
    uint8_t bit_position = 1;   // 6 upper bytes are for header 0b000111
    uint8_t byte_position = 0;

    //Serial.print(F("Reducing sequence, starting at:"));Serial.println(i);
    while (i <= 539)
    {
      count = 0;
      value = *(BitstreamPtr + i);
      
      //Serial.print(F("Position: "));Serial.print(i);Serial.print(", ");

      if (j == 90)
      {
        //reduced_array[j] = value;
        i = 539 + 1;
      }
      else
      {
        //Serial.print(F("counting equal values: "));
        while (value == *(BitstreamPtr + i))
        {
            count++;
            i++;
            //Serial.print(value);
        }
        //Serial.print(", "); Serial.print(count);
        
        if (j == -1)
        {
          //Serial.println(F(", no write to outbuf"));
          j++;
        }
        else
        //if ((count == 5) || (count == 6))
        if ((count == 5) || (count == 6) || (count == 7))
        {
          //Serial.print(F(", writing ")); Serial.print(value);Serial.print(F(" to outbuf at pos: "));Serial.println(j);
          reduced_array[j++] = value;
          if (value == 1)
            manchester_code_binary[byte_position] |= (1<<bit_position);
          else
            manchester_code_binary[byte_position] &= ~(1<<bit_position);
          bit_position--;
          if (bit_position == 0xFF)
          {
            bit_position = 7;
            byte_position++;
            if (byte_position >= 12)
              byte_position = 12;     // this shall not occur, but just in case
          }
        }
        else
        //if ((count == 10) || (count == 11) || (count == 12))
        if ((count == 10) || (count == 11) || (count == 12) || (count == 13))
        {
          //Serial.print(F(", writing ")); Serial.print(value);Serial.print(value);Serial.print(F(" to outbuf at pos: "));Serial.println(j);
          reduced_array[j++] = value;
          if (value == 1)
            manchester_code_binary[byte_position] |= (1<<bit_position);
          else
            manchester_code_binary[byte_position] &= ~(1<<bit_position);
          bit_position--;
          if (bit_position == 0xFF)
          {
            bit_position = 7;
            byte_position++;
            if (byte_position >= 12)
              byte_position = 12;     // this shall not occur, but just in case
          }
          reduced_array[j++] = value;
          if (value == 1)
            manchester_code_binary[byte_position] |= (1<<bit_position);
          else
            manchester_code_binary[byte_position] &= ~(1<<bit_position);
          bit_position--;
          if (bit_position == 0xFF)
          {
            bit_position = 7;
            byte_position++;
            if (byte_position >= 12)
              byte_position = 12;     // this shall not occur, but just in case
          }          
        }
        else
        {
          //Serial.println(F("Error in sequence"));
          return 1;
        }
      }
    }
  return 0;       
}


/* Function: Manchester Decoder
   This function translates the reduced bit stream that
   is Manchester coded to raw data. This final bit stream
   is the code we used to identify different cards.
*/

void Manchester_decode(void)
{
  unsigned char i;
  unsigned char j = 0;
  uint8_t bit_position = 4;   // startinf from bit 4 in byte 0
  uint8_t byte_position = 0;
  
  //Serial.println("Manchester data decoding:");

  for (i = 0; i < 90; i += 2)
  {
    if((reduced_array[i] == 0) && (reduced_array[i+1] == 1))
    {
      // Serial.print("INpos: "); Serial.print(i); Serial.print(", OUTpos: "); Serial.print(j); Serial.println(", 0");
      final_code[j++] = 0;
      final_code_binary[byte_position] &= ~(1<<bit_position);
    }
    else
    {
      //Serial.print("INpos: "); Serial.print(i); Serial.print(", OUTpos: "); Serial.print(j); Serial.println(", 1");
      final_code[j++] = 1;
      final_code_binary[byte_position] |= (1<<bit_position);
    }

    bit_position--;
    if (bit_position == 0xFF)
    {
      bit_position = 7;
      byte_position++;
      if (byte_position >= 5)
        byte_position = 5;     // this shall not occur, but just in case
    }
  }
}



void InitTimer1()
{
  // CTC mode 4; TOP = OCR1A, input capture set to rising edge select (ICES1=1)
  TCCR1A = 0x00;
  TCCR1B = 0x00;
  TCCR1B = (1<<WGM12) | (1<<ICES1);
  // 65535 cycles to overflow at CLK/1 prescaler, maximum possible bit offset
  OCR1A = 0xFFFF;
  // enable input capture interrupt on pin8
  TIMSK1 |= (1<<ICIE1);
  // clear the input capture flag after enabling interrupts to avoid a false interrupt call
  TIFR1 |= (1<<ICF1);
  // Clear Timer1
  TCNT1 = 0;
  // CLK/1 prescaler, count period is 62.5ns(16MHz crystal), Start counting
  TCCR1B |= (1<<CS10);  
}


void InitTimer2()
{
  // Setup 125kHz generator
  pinMode(FREQGEN, OUTPUT);
  TCCR2A = 0b00010010;
  TCCR2B = 0b00000001;                 // Timer2: CTC (mode 2), Toggle OC2B(PIN PD3) on compare match, TOP = OCR2A, divider = 1 (16 MHz, 62.5ns)
  DIS125kHz;
  OCR2A = 63;                         // 63 ticks per period. Frequency on COM2B (PIN PD3) 16000/64/2 = 125 kHz
}



void ErrorBeep()
{
  for (int j=0; j <2; j++)
  {
    for (int i=1000; i<2000; i=i*1.1)
    {
      tone(BUZZER, i); delay(10);
    }
    delay(50);
    
    for (int i=1000; i>500; i=i*1.9)
    {
      tone(BUZZER, i); delay(10);
    }
    delay(50);
  }
  noTone(BUZZER);
}


void ReadHIDCard()
{
  uint16_t prevcardnumber = 0;
  uint8_t readedcount = 0;
  uint8_t i;
  
  OrigCardReaded = false;
  Readed = false;

  for (uint8_t i=0;i<90;i++)
      reduced_array[i] = 0;
  for (uint8_t i=0;i<12;i++)
      manchester_code_binary[i] = 0;
  for (uint8_t i=0;i<6;i++)
      final_code_binary[i] = 0;

  InitTimer1();   // Initialize Timer1
  InitTimer2();   // Initialize 125kHz generator
  
  
  BitstreamPtrStart = CurrBitstreamPtr = BitstreamArray;
  BitstreamPtrEnd = BitstreamPtrStart + BITSTREAMARRAYSIZE;

  for (uint16_t i=0;i<BITSTREAMARRAYSIZE;i++)
      BitstreamArray[i] = 0;
  
  ENA125kHz;
  delay(100);
  sei();
  EnableStore = true;
  
  while(1)    // repeat until we readed card 3 times
  {
    if (Readed == true)
    {
      /*
      Serial.print(F("RAW card data: "));
      for (uint16_t a = 0; a < BITSTREAMARRAYSIZE; a++)
        Serial.print(BitstreamArray[a], HEX);  
      Serial.println();
      */

      uint8_t retval = FSK_decode(BitstreamPtrStart);

      //if (retval == 0)
      //{
        /*
        Serial.print(F("Manchester data:0x"));
        for (i = 0; i < 12; i++)
          Serial.print(manchester_code_binary[i], HEX);
        Serial.println();          
        */

        Manchester_decode();

        /*
        Serial.print(F("Card data: 0x"));
        for (i = 0; i < 6; i++)
          Serial.print(final_code_binary[i], HEX);
        Serial.println();
        */

        cardnumber = (uint16_t)final_code_binary[4] << 8 | (uint16_t)final_code_binary[5];
        if (prevcardnumber != cardnumber)
          readedcount = 0;    // reset readed counter
        else
        {
          if (readedcount == 3)
            break;              // exit loop if we readed card 3 times           
        }
        prevcardnumber = cardnumber;
        readedcount++;              
      /*}
      else
      {
        readedcount = 0;    // reset readed counter            
      } */
      
      
      for (uint16_t i=0;i<BITSTREAMARRAYSIZE;i++)
        BitstreamArray[i] = 0;

      for (uint8_t i=0;i<90;i++)
        reduced_array[i] = 0;
        
      for (uint8_t i=0;i<12;i++)
        manchester_code_binary[i] = 0;
        
      for (uint8_t i=0;i<6;i++)
        final_code_binary[i] = 0;

      /*  
      do
      {
        
      } while (Serial.availableForWrite() < 63);
      */    
      CurrBitstreamPtr = BitstreamPtrStart;
      Readed = false;
      EnableStore = true;
    }
  }

  DISINCAPTINT;
  TIFR1 |= (1<<ICF1); // clear the input capture
  //LEDOFF(); 

  Serial.print(F("Card readed, ID:")); Serial.print(cardnumber);
  Serial.print(F(", RAW data: 0x"));
  for (i = 0; i < 6; i++)
  {
    Serial.print(final_code_binary[i], HEX);
  }

  Serial.print(F(", Manchester data:0x"));
  for (i = 0; i < 12; i++)
  {
    Serial.print(manchester_code_binary[i], HEX);
  }
  Serial.println();

  OrigCardReaded = true;
  DIS125kHz;
  
  for (uint16_t a = 400; a < 6000; a = a * 1.5)
  {
    tone(BUZZER, a);
    delay(20);
  }

  noTone(BUZZER);
}


void CloneToT5557()
{
  InitTimer2();   // Initialize 125kHz generator
  ENA125kHz;
  delay(100);
  
  rfidGap(StartGap * Tc);                     // Start gap  
  uint32_t T5577_config = T5577CFG;           // Modulation=FKS2a, DataBitRate=RF/50, MaxBlock=3
  sendOpT5557(0b10, 0, 0, T5577_config, 0);   // Write to page 0, block 0 (Configuration data)
  //Serial.println("Config writed to T5577.");
  DIS125kHz;
  delay(300);
  ENA125kHz;
  delay(100);
  
  write2rfidT5557(manchester_code_binary);
  DIS125kHz;

  Serial.println(F("Card succesfully cloned :-)"));
  for (uint16_t i = 1500; i < 6000; i = i * 1.2)
  {
    tone(BUZZER, i);
    delay(10);
  }
  noTone(BUZZER);
}


void ClearT5557()
{
  InitTimer2();   // Initialize 125kHz generator
  ENA125kHz;
  delay(100);
  
  rfidGap(StartGap * Tc);                     // Start gap  
  uint32_t T5577_config = T5577CFG;           // Modulation=FKS2a, DataBitRate=RF/50, MaxBlock=3
  sendOpT5557(0b10, 0, 0, T5577_config, 0);   // Write to page 0, block 0 (Configuration data)
  //Serial.println("Config writed to T5577.");
  DIS125kHz;
  delay(300);
  ENA125kHz;
  delay(100);
  
  write2rfidT5557(T5577Clear);
  DIS125kHz; 
}

void ReadButtons()
{
    uint8_t reading = BTNPORT & 0x03;    // Mask only bit 0 (arduino A0 pin) of BTNPORT
    uint8_t retval;
    
    if (lastBtnState != reading)
      lastDebounceTime = millis();

    if ((millis() - lastDebounceTime) > debounceDelay)
    {
      // whatever the reading is at, it's been there for longer than the debounce
      // delay, so take it as the actual current state:

      // if the button state has changed:
      if (reading != buttonState)
        buttonState = reading;

      if ((buttonState & 0x01) == 0)     // inverted, LOW when button is pressed
      {
        buttons |= 0x01;
        //Serial.write('|');
      }
      else
      if ((buttonState & 0x02) == 0)
      {
        buttons |= 0x02;
        //Serial.write('|');
      }
    }

    lastBtnState = reading;
    return;
}



#ifdef OLEDENABLE
SH1106Lib display = SH1106Lib(OLED_DC, OLED_RST, OLED_CS, 4000000UL);
#endif

void setup()
{
  //pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BTNUP, INPUT_PULLUP);
  pinMode(BTNDN, INPUT_PULLUP);
  
  Serial.begin(115200);
  Serial.flush();
  Serial.print(F("HID ISOprox II card cloner v")); Serial.println(VERSION);
  Serial.println(F("Place card to coil a press button or send 'r' to serial console to read card"));
  Readed = false;

#ifdef OLEDENABLE
   // Start OLED
  display.initialize();
  display.setFont(font, 5, 7);
  display.setCursor(10, 20);
  display.print(F("HID ISOprox II card"));
  display.setCursor(25, 33);
  display.print(F("CLONER v.")); display.print(VERSION);
  delay(2000);
  display.clearDisplay();
  display.setCursor(5, 20);
  display.print(F("Place card on CLONER"));
  display.setCursor(12, 33);
  display.print(F("and press button"));
#endif
}



void loop()
{
  uint8_t i = 0;
  uint8_t ch = 0;

  if (Serial.available() > 0)
  {
    ch = Serial.read();
  }

  ReadButtons();

  switch(State)
  {
    case Start:
      if (buttons & 0x01)
      {
        buttons = 0;        
        ReadHIDCard();  
        if (OrigCardReaded == true)
        {
          #ifdef OLEDENABLE        
          display.clearDisplay();
          display.setCursor(0, 0);
          display.print(F("CardID: ")); display.print(cardnumber);
          display.setCursor(0, 8);
          display.print(F("RAW: 0x"));
          for (i = 0; i < 6; i++)
            display.print(final_code_binary[i], HEX);
          display.setCursor(0, 16);
          display.print(F("---------------------"));
          
          display.setCursor(0, 43);
          display.print(F("<- CLONE"));
          display.setCursor(0, 56);
          display.print(F("<- READ NEW"));
          #endif
          State = CardReaded;
          delay(100);          
        }
      }
    break;

    case CardReaded:
      if (buttons & 0x01)
      {
        buttons = 0;
        if (OrigCardReaded == true)
        {
          #ifdef OLEDENABLE
          display.fillPage(3, 0x00);
          display.fillPage(4, 0x00);
          display.fillPage(5, 0x00);
          display.fillPage(6, 0x00);
          display.fillPage(7, 0x00);
          display.setCursor(0, 40);
          display.print(F("  Place card to be   "));
          display.setCursor(0, 48);
          display.print(F("  cloned on CLONER   "));
          display.setCursor(0, 56);
          display.print(F("  and press button   "));
          #endif
           State = Cloning;
           delay(100);
        }
        else
        {
          ErrorBeep();
          #ifdef OLEDENABLE
          display.fillPage(3, 0x00);
          display.fillPage(4, 0x00);
          display.fillPage(5, 0x00);
          display.fillPage(6, 0x00);
          display.fillPage(7, 0x00);
          display.setCursor(0, 40);
          display.print(F(" No orig card readed "));
          display.setCursor(0, 56);
          display.print(F("<- BACK"));
          #endif
          Serial.println(F("No card readed yet!"));
          State = ErrNoReadedCard;
          delay(100);
        }
      }
      else if (buttons & 0x02)
      {
        #ifdef OLEDENABLE
        display.clearDisplay();
        display.setCursor(5, 20);
        display.print(F("Place card on CLONER"));
        display.setCursor(12, 33);
        display.print(F("and press button"));
        #endif 
        State = Start;
        buttons = 0;
        delay(100);
      }
    break;

    case ErrNoReadedCard:
      if (buttons & 0x01)
      {
        buttons = 0;
        #ifdef OLEDENABLE
        display.clearDisplay();
        display.setCursor(5, 20);
        display.print(F("Place card on CLONER"));
        display.setCursor(12, 33);
        display.print(F("and press button")); 
        #endif       
        State = Start;
        delay(100);
      }          
    break; 

    case Cloning:
      if (buttons & 0x01)
      {
        buttons = 0;
        manchester_code_binary[0] |= 0b00011100;    // Set HID preamble
        Serial.print(F("Cloning 0x"));
        for (i = 0; i < 12; i++)
          Serial.print(manchester_code_binary[i], HEX);

        Serial.println(F(" to T5577."));
        display.fillPage(3, 0x00);
        display.fillPage(4, 0x00);
        display.fillPage(5, 0x00);
        display.fillPage(6, 0x00);
        display.fillPage(7, 0x00);
        display.setCursor(0, 32);
        display.print(F("     Cloning...      "));
        CloneToT5557();
        
        #ifdef OLEDENABLE
        display.fillPage(3, 0x00);
        display.fillPage(4, 0x00);
        display.fillPage(5, 0x00);
        display.fillPage(6, 0x00);
        display.fillPage(7, 0x00);
        display.setCursor(0, 32);
        display.print(F("     CARD CLONED     "));
        display.setCursor(0, 56);
        display.print(F("<- BACK"));
        #endif
        buttons = 0;
        State = CardCloned;
        delay(100);
      }
    break;

    case CardCloned:
        if (buttons & 0x01)
        {
          buttons = 0; 
          #ifdef OLEDENABLE        
          display.clearDisplay();
          display.setCursor(0, 0);
          display.print(F("CardID: ")); display.print(cardnumber);
          display.setCursor(0, 8);
          display.print(F("RAW: 0x"));
          for (i = 0; i < 6; i++)
            display.print(final_code_binary[i], HEX);
          display.setCursor(0, 16);
          display.print(F("---------------------"));
          
          display.setCursor(0, 43);
          display.print(F("<- CLONE"));
          display.setCursor(0, 56);
          display.print(F("<- READ NEW"));
          #endif
          State = CardReaded;
          delay(100);
        }
    break;
    
  }  
  

   
  //ClearT5557();
  //Serial.println(F("T5577 card cleared!"));
}
