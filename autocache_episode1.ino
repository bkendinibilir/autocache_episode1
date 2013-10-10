#include <Wire.h>
#include <FMTX.h>
#include <DS1307new.h>
#include <avr/sleep.h>

// GPIO Ports
#define TRANS_OUT 12
#define MIC_IN    17
#define WAV_CLK   18
#define WAV_DAT   20
#define LIGHT_IN  19

// reset to first stage after 30mins
#define APP_TIMEOUT 1800000
// 1s sound detection
#define MIC_RECORD 250
// trigger after 50 sound detections in MIC_RECORD time
#define MIC_TRESHOLD 13

elapsedMillis sinceAnyAction;
elapsedMillis sinceMicDetect;

int state = 0;
int mic_count = 0;

//------------------------------------------------

void idle() {
  set_sleep_mode(SLEEP_MODE_IDLE);
  noInterrupts();
  digitalWrite(2, LOW);
  sleep_enable();
  interrupts();
  sleep_cpu();
  sleep_disable();
}

//------------------------------------------------

void fm_init(void) 
{
  u8 reg = fmtx_read_reg(0x0B);
  fmtx_write_reg(0x0B, reg &= 0xFB);
}

void fm_disable(void) 
{
  u8 reg = fmtx_read_reg(0x0B);
  //fmtx_write_reg(0x0B, reg |= 0x20);  
  fmtx_write_reg(0x0B, reg |= 0xA0);  
}

void fm_enable(void) 
{
  u8 reg = fmtx_read_reg(0x0B);
  //fmtx_write_reg(0x0B, reg &= 0xDF);
  fmtx_write_reg(0x0B, reg &= 0x5F);
}

//------------------------------------------------

void wav_init(void)
{
  pinMode(WAV_CLK, OUTPUT); 
  pinMode(WAV_DAT, OUTPUT);
        
  digitalWrite(WAV_CLK, HIGH);
  digitalWrite(WAV_DAT, HIGH);
  
  delay(1000);
}

void wav_send(int data)
{
  digitalWrite(WAV_CLK, LOW);
  delay(2);
  for (int i=15; i>=0; i--)
  { 
    delayMicroseconds(50);
    if((data>>i)&0x0001 >0)
      {
        digitalWrite(WAV_DAT, HIGH);
      }
    else
       {
         digitalWrite(WAV_DAT, LOW);
       }
    delayMicroseconds(50);
    digitalWrite(WAV_CLK, HIGH);
    delayMicroseconds(50);
    
    if(i>0)
    digitalWrite(WAV_DAT, LOW);
    else
    digitalWrite(WAV_DAT, HIGH);
    delayMicroseconds(50);
    
    if(i>0)
    digitalWrite(WAV_CLK, LOW);
    else
    digitalWrite(WAV_CLK, HIGH);
  }
  
  delay(20); 
}

void wav_playno(unsigned int no) 
{
  if (no < 512) {
    wav_send(no);
  }
}

void wav_playpause(void)
{
  wav_send(0xfffe);
}

void wav_stop(void)
{
  wav_send(0xffff);
}

void wav_volume(unsigned char vol) 
{
  if (vol > 7) {
    vol = 7;
  } 
  wav_send(0xfff0 + vol);
}

//------------------------------------------------

void play_radio(unsigned int file_no, unsigned int time)
{
  // switch on power of wav- and fm-module
  digitalWrite(TRANS_OUT, true);
  delay(250);

  // init wav module
  wav_init();  
  wav_volume(5);
  
  // init fm module
  fmtx_init(91.7, EUROPE);
  fmtx_set_rfgain(15);
  fm_init();  

  // play wave file
  wav_playno(file_no);
  
  // hack for loop: wait time seconds
  for(int i = 0; i <= time; i++) {
    delay(1000);
  }
  
  // disable rf of fm-module and stop playing
  fm_disable();
  wav_stop();
  
  // switch off power of wav- and fm-module
  digitalWrite(TRANS_OUT, false);
  
  sinceAnyAction = 0;
}

void debug_time(void)
{
  Serial.print(RTC.hour);
  Serial.print(":");
  Serial.print(RTC.minute);
  Serial.print(":");
  Serial.print(RTC.second);
  Serial.println("");
}

void setup(void)
{ 
  // deactivate USB
  //Serial.end();
  Serial.begin(9600);
  
  // set all GPIOs to output
  for (int i=0; i<46; i++) {
    pinMode(i, OUTPUT);
  }
  
  pinMode(TRANS_OUT, OUTPUT);

  // switch off wave- and fm-module
  digitalWrite(TRANS_OUT, false);
  wav_init();
  
  // activate input for mic
  pinMode(MIC_IN, INPUT); 
  
  Serial.println("init completed, recording...");    
}

void loop(void)
{
  //RTC.getTime();
  //debug_time(); 
  //if(RTC.second > 55) {
  //  Serial.println("Playing file 0 for 10 seconds.");
  //  play_radio(0, 10000);
  //}
  
  if(state > 0 && sinceAnyAction > APP_TIMEOUT) {
    // reset to stage 0
    Serial.println("reset to stage 0.");    
    sinceAnyAction = 0;
    state = 0;
  }
  
  // count sound detection
  boolean mic = digitalRead(MIC_IN);
  if(mic == false) {
    mic_count++;
  }
      
  if (sinceMicDetect > MIC_RECORD) {
  //  Serial.print("light: ");
  //  Serial.println(analogRead(LIGHT_IN));  
      
    if (mic_count > MIC_TRESHOLD) {
      Serial.print("mic: ");
      Serial.println(mic_count);
      
      switch(state) {
        case 0:
          Serial.println("Playing file 0 for 70 seconds.");    
          play_radio(0, 70);
          state++;
          break;
        case 1:
          Serial.println("Playing file 1 for 27 seconds.");    
          play_radio(1, 27);
          state++;
          break;
        case 2:
          Serial.println("Playing file 2 for 31 seconds.");    
          play_radio(2, 31);
          state = 0;
          break;
      }
      
    } else {
      if(mic_count > 1) {
        Serial.print("mic: ");
        Serial.println(mic_count);
      }
    }
    mic_count = 0;
    sinceMicDetect = 0;
  }
  delay(1);
}
