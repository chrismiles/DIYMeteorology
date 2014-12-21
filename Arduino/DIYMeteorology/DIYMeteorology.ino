// DIYMeteorology by Chris Miles 2014
// Copyright (c) 2014 Chris Miles
//
// License: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// 
// Originally derived from code by:
//   Oregon V2 decoder - Dominique Pierre
//   Oregon V3 decoder - Dominique Pierre
//   2010-04-11 <jcw@equi4.com> http://opensource.org/licenses/mit-license.php


class DecodeOOK {
protected:
    byte total_bits, bits, flip, state, pos, data[25];

    virtual char decode (word width) =0;
    
public:

    enum { UNKNOWN, T0, T1, T2, T3, OK, DONE };

    DecodeOOK () { resetDecoder(); }

    bool nextPulse (word width) {
        if (state != DONE)
        
            switch (decode(width)) {
                case -1: resetDecoder(); break;
                case 1:  done(); break;
            }
        return isDone();
    }
    
    bool isDone () const { return state == DONE; }

    const byte* getData (byte& count) const {
        count = pos;
        return data; 
    }
    
    void resetDecoder () {
        total_bits = bits = pos = flip = 0;
        state = UNKNOWN;
    }
    
    // add one bit to the packet data buffer
    
    virtual void gotBit (char value) {
        total_bits++;
        byte *ptr = data + pos;
        *ptr = (*ptr >> 1) | (value << 7);

        if (++bits >= 8) {
            bits = 0;
            if (++pos >= sizeof data) {
                resetDecoder();
                return;
            }
        }
        state = OK;
    }
    
    // store a bit using Manchester encoding
    void manchester (char value) {
        flip ^= value; // manchester code, long pulse flips the bit
        gotBit(flip);
    }
    
    // move bits to the front so that all the bits are aligned to the end
    void alignTail (byte max =0) {
        // align bits
        if (bits != 0) {
            data[pos] >>= 8 - bits;
            for (byte i = 0; i < pos; ++i)
                data[i] = (data[i] >> bits) | (data[i+1] << (8 - bits));
            bits = 0;
        }
        // optionally shift bytes down if there are too many of 'em
        if (max > 0 && pos > max) {
            byte n = pos - max;
            pos = max;
            for (byte i = 0; i < pos; ++i)
                data[i] = data[i+n];
        }
    }
    
    void reverseBits () {
        for (byte i = 0; i < pos; ++i) {
            byte b = data[i];
            for (byte j = 0; j < 8; ++j) {
                data[i] = (data[i] << 1) | (b & 1);
                b >>= 1;
            }
        }
    }
    
    void reverseNibbles () {
        for (byte i = 0; i < pos; ++i)
            data[i] = (data[i] << 4) | (data[i] >> 4);
    }
    
    void done () {
        while (bits)
            gotBit(0); // padding
        state = DONE;
    }
};

// 433 MHz decoders


class OregonDecoderV2 : public DecodeOOK {
public:
    OregonDecoderV2() {}
    
    // add one bit to the packet data buffer
    virtual void gotBit (char value) {
        if(!(total_bits & 0x01))
        {
            data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
        }
        total_bits++;
        pos = total_bits >> 4;
        if (pos >= sizeof data) {
            resetDecoder();
            return;
        }
        state = OK;
    }
    
    virtual char decode (word width) {
        if (200 <= width && width < 1200) {
            byte w = width >= 700;
            switch (state) {
                case UNKNOWN:
                    if (w != 0) {
                        // Long pulse
                        ++flip;
                    } else if (32 <= flip) {
                        // Short pulse, start bit
                        flip = 0;
                        state = T0;
                    } else {
                      // Reset decoder
                        return -1;
                    }
                    break;
                case OK:
                    if (w == 0) {
                        // Short pulse
                        state = T0;
                    } else {
                        // Long pulse
                        manchester(1);
                    }
                    break;
                case T0:
                    if (w == 0) {
                      // Second short pulse
                        manchester(0);
                    } else {
                        // Reset decoder
                        return -1;
                    }
                    break;
            }
        } else {
            return -1;
        }
        return total_bits == 160 ? 1: 0;
    }
};

class OregonDecoderV3 : public DecodeOOK {
public:
    OregonDecoderV3() {}
    
    // add one bit to the packet data buffer
    virtual void gotBit (char value) {
        data[pos] = (data[pos] >> 1) | (value ? 0x80 : 00);
        total_bits++;
        pos = total_bits >> 3;
        if (pos >= sizeof data) {
            resetDecoder();
            return;
        }
        state = OK;
    }
    
    virtual char decode (word width) {
        if (200 <= width && width < 1200) {
            byte w = width >= 700;
            switch (state) {
                case UNKNOWN:
                    if (w == 0)
                        ++flip;
                    else if (32 <= flip) {
                        flip = 1;
                        manchester(1);
                    } else
                        return -1;
                    break;
                case OK:
                    if (w == 0)
                        state = T0;
                    else
                        manchester(1);
                    break;
                case T0:
                    if (w == 0)
                        manchester(0);
                    else
                        return -1;
                    break;
            }
        } else {
            return -1;
        }
        return  total_bits == 80 ? 1: 0;
    }
};

OregonDecoderV2 orscV2;
OregonDecoderV3 orscV3;

#define PORT 2

volatile word pulse;

// Serial activity LED
const int serialLED = 7;
// Sensor activity LED
const int sensorLED = 6;

// Serial variables
const int serialLength = 32; // size of the serial buffer
char serialString[serialLength];
byte serialIndex = 0;

char lasttemp[6] = "0.0";
char lasthumid[3] = "0";
long lasttime = 0;

#if defined(__AVR_ATmega1280__)
void ext_int_1(void) {
#else
ISR(ANALOG_COMP_vect) {
#endif
    static word last;
    // determine the pulse length in microseconds, for either polarity
    pulse = micros() - last;
    last += pulse;
}

void parseData (const char* s, class DecodeOOK& decoder)
{
    byte pos;
    const byte* data = decoder.getData(pos);
    
    /*
    Serial.print("DATA {");
    Serial.print(data[0], HEX);
    Serial.print(", ");
    Serial.print(data[1], HEX);
    Serial.print(", ");
    Serial.print(data[2], HEX);
    Serial.println("}");
    */
    
    // Nibble 0: sync
    char sync = data[0] & 0x0F;
    if (sync != 0x0A)
    {
      Serial.print("!!!! Unexpected Sync: 0x");
      Serial.println(sync, HEX);
      // TODO: error handler
    }
    else
    {
      // Sync OK, read Sensor ID
      
      // Nibble 1-4: sensorID
      int sensorID = (data[0] & 0xF0) << 8;
      sensorID += (data[1] & 0x0F) << 8;
      sensorID += (data[1] & 0xF0);
      sensorID += (data[2] & 0x0F);
  

      decodeMessageForSensorID(sensorID, data);
    }
    decoder.resetDecoder();
}

void decodeMessageForSensorID(int sensorID, const byte* data)
{
  if (sensorID == 0x1D20)
  {
    // Temperature/Barometer: THGR122NX
    decodeMessageForTHGR122NX(data);
  }
  else if (sensorID == 0xF824 || sensorID == 0xF8B4 || ((sensorID & 0x0CCC) == 0x0CCC) || ((sensorID & 0xFFFF) == 0xEC40))
  {
    // Temperature/Barometer: THGR122NX, THGN801, THGR810, THGR810
    //   THGR328N is xCCC (last 12 bits only)
    
  }
  else if (sensorID == 0x2D10)
  {
    // Rain gauge
    decodeMessageForRGR968(data);
  }
  else
  {
    Serial.print("!!! No decoder implemented for SensorID: ");
    Serial.print(sensorID, HEX);
    Serial.print(" ");
    Serial.println(sensorID, BIN);
  }
}

void decodeMessageForTHGR122NX(const byte* data)
{
  // Temperature/Barometer
  // SensorID: 0x1D20
  Serial.print(" --> THGR122NX");
  decodeMessageForOregonTemperature(data);
}

void decodeMessageForOregonTemperature(const byte* data)
{
  // Message Length: 17 nibbles
  
  // TODO: channel is shared by all v2.1/3.0 sensors
//  int channel = 1 << ((int)(data[2] >> 4) - 1);
  // Equivalent of log(v, 2) + 1
  int channel = (log((int)(data[2] >> 4)) / log(2)) + 1;
  
  Serial.print(" channel=");
  Serial.print(channel);
  Serial.print("   ");
  
  float temperature = ((data[5] >> 4) * 10) + (data[5] & 0xf) + ((data[4] >> 4) / 10.0f);
  float humidity = ((data[7] & 0xf) * 10) + (data[6] >> 4);

  if ((data[6] & 0xf) != 0)
  {
    temperature = -temperature;
  }
  
  // TODO: checksum is shared by all v2.1/3.0 sensors
  //checksum = (nibble[n-4] << 4) + nibble[n-3]);
  int checksum = 
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C, Humidity: ");
  Serial.print(humidity);
  Serial.print("%\n");
  
  // TODO: battery low flag
  // TODO: checksum

  Serial.print("D,THGR122NX,");
  Serial.print(channel);
  Serial.print(",");
  Serial.print(temperature);
  Serial.print(",");
  Serial.print(humidity);
  Serial.print("\n");

//  // Get the temperature.
//  char temp[6];
//  char *tempptr = &temp[0];
//  // 14th nibble indicates sign. non-zero for -ve
//  if ((int)(data[6] & 0x0F) != 0) {
//  	*tempptr = '-';
//  	tempptr++;
//  }
//  sprintf(tempptr, "%02x", (int)(data[5]));
//  tempptr = tempptr + 2;
//  *tempptr = '.';
//  tempptr++;
//  sprintf(tempptr, "%x", (int)(data[4] >> 4));
//  
//  // Get the humidity.
//  char humid[3];
//  char *humidptr = &humid[0];
//  sprintf(humidptr, "%x", (int)(data[7] & 0x0F));
//  humidptr++;
//  sprintf(humidptr, "%x", (int)(data[6] >> 4));
//  humid[2] = '\0';
//  
//  strcpy(lasttemp, temp);
//  strcpy(lasthumid, humid);
//  lasttime = millis();
//  
//  dumpWeatherData();


}

void decodeMessageForRGR968(const byte* data)
{
  Serial.print(" --> RGR968");
  
  int channel = (log((int)(data[2] >> 4)) / log(2)) + 1;
  
  Serial.print(" channel=");
  Serial.print(channel);
  Serial.print("   ");

  // Get the rain rate.
  float rain_rate = ((data[5] >> 4) * 10) + (data[5] & 0xf) + ((data[4] >> 4) / 10.0f);
  float total_rain = ((data[8] & 0xf)*1000) + ((data[7] >> 4) * 100) + ((data[7] & 0xf) * 10) + (data[6] >> 4) + ((data[6] & 0xf) / 10.0f);
  
  Serial.print("Rain rate: ");
  Serial.print(rain_rate);
  Serial.print(" mm/hour, Rain total: ");
  Serial.print(total_rain);
  Serial.print(" mm\n");

  // TODO: battery low flag
  // TODO: checksum

}


void readSerial() {
  while ((Serial.available() > 0) && (serialIndex < serialLength-1)) {
    digitalWrite(serialLED, HIGH);
    char serialByte = Serial.read();
    if (serialByte != ';') {
      serialString[serialIndex] = serialByte;
      serialIndex++;
    }
    if (serialByte == ';' or serialIndex == (serialLength-1)) {
      parseSerial();
      serialIndex = 0;
      memset(&serialString, 0, serialLength);
    }
  }
  digitalWrite(serialLED, LOW);
}

void parseSerial() {
  if (strcmp(serialString, "get") == 0) {
    dumpWeatherData();
  }
}

void dumpWeatherData()
{
    long age = (millis() - lasttime) / 1000;
    // Format is "channel,temp,humidity,age;"
    Serial.print("Temperature: ");
    Serial.print(lasttemp);
    Serial.print("C, ");
    Serial.print(lasthumid);
    Serial.print("%, ");
    Serial.print(age);
    Serial.print(";\n");
}

void setup ()
{
  Serial.begin(115200);
  pinMode(serialLED, OUTPUT);
  digitalWrite(serialLED, LOW);
  pinMode(sensorLED, OUTPUT);
  digitalWrite(serialLED, LOW);

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

#if !defined(__AVR_ATmega1280__)
    pinMode(13 + PORT, INPUT);  // use the AIO pin
    digitalWrite(13 + PORT, 1); // enable pull-up

    // use analog comparator to switch at 1.1V bandgap transition
    ACSR = _BV(ACBG) | _BV(ACI) | _BV(ACIE);

    // set ADC mux to the proper port
    ADCSRA &= ~ bit(ADEN);
    ADCSRB |= bit(ACME);
    ADMUX = PORT - 1;
#else
   attachInterrupt(1, ext_int_1, CHANGE);

   DDRE  &= ~_BV(PE5);
   PORTE &= ~_BV(PE5);
#endif

   Serial.println("\n[weatherstation initialised]");
}

void loop () {
  static int i = 0;
    cli();
    word p = pulse;
    
    pulse = 0;
    sei();

    //if (p != 0){ Serial.print(++i); Serial.print('\n');}
    
    if (p != 0) {
      digitalWrite(sensorLED, HIGH);
      if (orscV2.nextPulse(p)) {
	digitalWrite(sensorLED, HIGH);
	parseData("OSV2", orscV2);
	digitalWrite(sensorLED, LOW);
      }
      digitalWrite(sensorLED, LOW);
      
      //dumpWeatherData();
    }

    readSerial();
}
