////////////////////////////////////////////////////////////////////////////////////////////////
//MPC500-MIDI CONTROL
////////////////////////////////////////////////////////////////////////////////////////////////
/*DESCRIPTION///////////////////////////////////////////////////////////////////////////////////
  ---------------------------------- 
  Teensy 3.2
  ----------------------------------
  11 Buttons
  6 Leds 
  4 Analog 
  1 Display
  ----------------------------------   
  MIDI IN  A/-
  MIDI OUT A/B
  ----------------------------------  
  4 Instrument Presets * 8 Configurable Parameters
////////////////////////////////////////////////////////////////////////////////////////////////
  "par" --> Parameter Name
  "typ" --> MIDI Type 0 = Control Change / 1 = ProgChange / 2 = Pitchbend / 3 = NoteOn
  "ctrl"--> CC Number
  "min" --> Mapping MIN
  "max" --> Mapping MAX
  "syt" --> Sysex Table & Display Format 0 = ON/OFF / 1 = VALUE
  "ch"  --> MIDI Channel
//////////////////////////////////////////////////////////////////////////////////////////////*/  
////////////////////////////////////////////////////////////////////////////////////////////////
// MIDI
#include <MIDI.h>
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1,  MIDIA);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial2,  MIDIB);
// DISPLAY
#include <Wire.h>
#include <U8x8lib.h>
U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(/* reset=*/ U8X8_PIN_NONE);
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
  //HARDWARE PINS/////////////////////////////////////////
  const int NIns = 4;
  const int NBt = 7;
  const int Bt[11] = {5,4,3,2,15,16,17,12,31,32,33};
  const int Led[6] = {11,8,7,6,14,13};
  const int NPot = 4;
  const int Pot[NPot] = {20,21,22,23};
  //INPUTS////////////////////////////////////////////////
  int InsCState[NIns] = {};
  int InsPState[NIns] = {};
  unsigned long lastDebounceTime[NBt] = {0};
  unsigned long debounceDelay = 50;
  int LEDstatus[NIns][NBt];
  int potCState[NPot] = {0};
  int potPState[NPot] = {0};
  int potVar = 0;
  const int TIMEOUT = 300;
  const int varThreshold = 10;
  boolean potMoving = true;
  unsigned long PTime[NPot] = {0}; 
  unsigned long timer[NPot] = {0};
  long CEnc[NIns] = {-999};
  long PEnc[NIns] = {-999};
  int increment;
  //GLOBALS/////////////////////////////////////////////
  int ins = 0;
  int n;
  int nvar;
  int v;
  int vi;
  int prc[NIns] = {1,1,1,1};
  int notemod[NIns] = {};
  char buf[50];
  const char *state;
  const char *s[] = {" OFF ", " ON  "};
  const char *mod; 
  const char *m[] = {"STEP", "HLVE", "FULL", ">>>>"}; 
  int mstate;
  //PARAMETERS//////////////////////////////////////////
  const char *par;
  String typ;
  String ctrl;
  String mn;
  String mx;
  String syt;
  String ch;
//MIDI CONFIG TABLES////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
  const char *i[] = {"MAIN", "DRM-X", "SAMP", "8-BIT"};
  //
  int A_PState[NIns][NBt] = {{0,0,1,0,1,0,0}, {0,0,1,0,1,0,0}, {0,0,0,0,0,0,0}, {0,0,0,0,0,0,0}};
  int A_CState[NIns][NBt];
  //                                        
  const char *prg[NIns][8][8] = {
  /*{"par", "typ", "ctrl", "min", "max", "syt", "ch"} */   
    {//INS-1 - MAIN
    {"BT1", "0", "1", "0", "127", "0", "1"},      {"BT2", "0", "2", "0", "127", "0", "1"},
    {"P01", "0", "3", "0", "127", "1", "1"},      {"P02", "0", "4", "0", "127", "1", "1"},
    {"P03", "0", "5", "0", "127", "1", "1"},      {"P04", "0", "6", "0", "127", "1", "1"},      
    {"P05", "0", "7", "0", "127", "1", "1"},      {"P06", "0", "8", "0", "127", "1", "1"}, },

    {//INS-2 - DRM-X
    {"BT1", "0", "1", "0", "127", "0", "2"},      {"BT2", "0", "2", "0", "127", "0", "2"},
    {"SR3", "0", "15", "0", "127", "1", "2"},     {"ST3", "0", "16", "0", "127", "1", "2"},
    {"SR1", "0", "3", "0", "127", "1", "2"},      {"ST1", "0", "4", "0", "127", "1", "2"},     
    {"SR2", "0", "9", "0", "127", "1", "2"},      {"ST2", "0", "10", "0", "127", "1", "2"}, },

   
    {//INS-3 - SAMP
    {"BT1", "0", "1", "0", "127", "0", "3"},      {"BT2", "0", "2", "0", "127", "0", "3"},
    {"STA", "0", "108", "0", "127", "1", "3"},    {"END", "0", "109", "0", "127", "1", "3"},
    {"SRT", "0", "102", "0", "127", "1", "3"},    {"CRU", "0", "103", "0", "127", "1", "3"},     
    {"SZE", "0", "106", "0", "127", "1", "3"},    {"SPD", "0", "107", "0", "127", "1", "3"}, },
    
    {//INS-4 - 8-BIT  
    {"BT1", "0", "1", "0", "127", "0", "4"},      {"BT2", "0", "2", "0", "127", "0", "4"},
    {"P01", "0", "3", "0", "127", "1", "4"},      {"PUL", "0", "70", "0", "2", "1", "4"},
    {"P03", "0", "5", "0", "127", "1", "4"},      {"P04", "0", "6", "0", "127", "1", "4"},     
    {"P05", "0", "7", "0", "127", "1", "4"},      {"P06", "0", "8", "0", "127", "1", "4"}  }
    };              
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void setup() { 
  // midi
  MIDIA.begin(MIDI_CHANNEL_OMNI);
  MIDIA.turnThruOff();
  MIDIB.begin();
  //MIDIB.turnThruOff();
  MIDIA.setHandleNoteOn(handleNoteOn);
  MIDIA.setHandleNoteOff(handleNoteOff);
  MIDIA.setHandleControlChange(handleControlChange);
  //MIDIA.setHandleProgramChange(handleProgramChange);  
  // display
  u8x8.begin(); 
  Wire.setClock(3400000);
  // grafics
  u8x8.setFont(u8x8_font_artossans8_u);
  u8x8.drawString(5, 5, "------"); 
  // pins
  for (int i = 0; i < NBt+NIns; i++) {
    pinMode(Bt[i], INPUT_PULLUP);
  }
  for (int i = 0; i < NIns+2; i++) {
    pinMode(Led[i], OUTPUT);
  }
  // startupconfig
  buttons();
  potentiometers();
  n = 0;
  instrument();
  //Serial.begin(9600); 
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void loop() { 
  buttons();
  potentiometers();
  MIDIA.read();    
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
//INPUT
void buttons() {
  //INSTRUMENT 0-3
  for (int i = 0; i < NIns; i++) { 
    InsCState[i] = digitalRead(Bt[i]);
    if (InsPState[i] != InsCState[i]) {
      if (InsCState[i] == 0) {
        ins = i;
        instrument();
      }   
      InsPState[i] = InsCState[i];  
    }
  }
  //BT 4-5-6
  for (int i = NIns; i < NIns+3; i++) {
    A_CState[ins][i-NIns] = digitalRead(Bt[i]);
    if ((millis() - lastDebounceTime[i-NIns]) > debounceDelay) {
      if (A_PState[ins][i-NIns] != A_CState[ins][i-NIns]) {
        lastDebounceTime[i-NIns] = millis();
        if (A_PState[ins][i-NIns] == HIGH) {
          n = i - NIns;
          if (LEDstatus[ins][i-NIns] == 0) {
            LEDstatus[ins][i-NIns] = 1;
            v = 127;
          } 
          else {
            LEDstatus[ins][i-NIns] = 0;
            v = 0; 
          }
          if (i - NIns < 2) {
            control();
          }
        }
        digitalWrite(Led[4], LEDstatus[ins][2]);
        A_PState[ins][i-NIns] = A_CState[ins][i-NIns];
      }  
    }    
  }
  //BT 7 // MODE-A
  A_CState[ins][3] = digitalRead(Bt[7]);
  if ((millis() - lastDebounceTime[3]) > debounceDelay) {
    if (A_PState[ins][3] != A_CState[ins][3]) {
      lastDebounceTime[3] = millis();
      if (A_PState[ins][3] == HIGH)  {
        mstate = mstate+1;
        if (mstate == 3) {
          mstate = 0; 
        }
      }
      mod = m[mstate];
      d_m();
      A_PState[ins][3] = A_CState[ins][3];  
    }           
  }        

  //BT 8 // MODE-B
  A_CState[ins][4] = digitalRead(Bt[8]);
  if ((millis() - lastDebounceTime[4]) > debounceDelay) {
    if (A_PState[ins][4] != A_CState[ins][4]) {
      lastDebounceTime[4] = millis();
      if (A_PState[ins][4] == HIGH)  {
        if (LEDstatus[ins][4] == 0) {
          LEDstatus[ins][4] = 1;
          par = "TRS";
          mod = m[mstate];
          sprintf(buf, "%+04d", notemod[ins]); 
        }
        else {
          LEDstatus[ins][4] = 0;
          par = "PRC";
          mod = m[3];
          sprintf(buf, "%+04d", prc[ins]); 
        }        
      }
      state = buf;
      d_p();
      d_m();
      d_s();
      digitalWrite(Led[5], LEDstatus[ins][4]);  
      A_PState[ins][4] = A_CState[ins][4];      
    }
  }

  //BT 9-10 // INCREMENT
  for (int i = 9; i < 11; i++) {
    A_CState[ins][i-NIns] = digitalRead(Bt[i]);
    if ((millis() - lastDebounceTime[i-NIns]) > debounceDelay) {
      if (A_PState[ins][i-NIns] != A_CState[ins][i-NIns]) {
        lastDebounceTime[i-NIns] = millis();
        if (A_PState[ins][i-NIns] == HIGH)  {
          LEDstatus[ins][i-NIns] = 1;
          incr();
        }
        else {
          LEDstatus[ins][i-NIns] = 0;
        } 
        A_PState[ins][i-NIns] = A_CState[ins][i-NIns];  
      }      
    }
  }
}  
void potentiometers() {
  for (int i = 0; i < NPot; i++) {                      
    potCState[i] = analogRead(Pot[i]);
    potVar = abs(potCState[i] - potPState[i]);                
    if (potVar > varThreshold) {                              
      PTime[i] = millis();                                    
    }
    timer[i] = millis() - PTime[i];                           
    if (timer[i] < TIMEOUT) {                                 
      potMoving = true;
    }
    else {
      potMoving = false;
    }
    if (potMoving == true) {                                  
      if (potPState[i] != potCState[i]) {
        n = i + 2;
        v = potCState[i];
        control();
        potPState[i] = potCState[i];                   
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
void instrument() { // TOGGLE INSTRUMENT
  for (unsigned i=0;i<NIns;i++) {
    digitalWrite(Led[i], LOW);
  }
  digitalWrite(Led[ins], HIGH);
  preset();
  d_i();
}

void control() { // MIDICONTROL
 
  if (n < 4) {
    nvar = n; 
  }
  if ((n > 3) &( n < 6)) {
    if (LEDstatus[ins][2] == 0) {
      nvar = n; 
    }
    if (LEDstatus[ins][2] == 1) {
      nvar = n + 2; 
    }  
  }
  par = prg[ins][nvar][0];
  typ = prg[ins][nvar][1];
  ctrl = prg[ins][nvar][2];
  mn = prg[ins][nvar][3];
  mx = prg[ins][nvar][4];
  syt = prg[ins][nvar][5];
  ch = prg[ins][nvar][6];
  
  int tinc = syt.toInt();
  int val = map (v, 0, 1023, mn.toInt(), mx.toInt()); 
  int mch = ch.toInt();   
  // CC
  if (typ.toInt() == 0) {
    int cc = ctrl.toInt();
    MIDIA.sendControlChange (cc, val, mch);
    MIDIB.sendControlChange (cc, val, mch);  
  }
  // NRPN
  if (typ.toInt() == 1) {
    MIDIA.sendProgramChange (val, mch);
    MIDIB.sendProgramChange (val, mch);
  }
  // SYSEX
  if (typ.toInt() == 2) {
    MIDIA.sendPitchBend (val, mch);
    MIDIB.sendPitchBend (val, mch);    
  }
  if (tinc == 0) {
    state = s[constrain(v,0,1)]; 
  }
  if (tinc == 1) {
    sprintf(buf, "%+04d", val);
    state = buf; 
  }
  mod = m[3];
  d_p();
  d_m();
  d_s();        
}

void preset() {
  digitalWrite(Led[4], LEDstatus[ins][2]);
  digitalWrite(Led[5], LEDstatus[ins][4]); 
}

void incr() { //INCREMENTBUTTONS   
  if (LEDstatus[ins][4] == 1) { // MODE TRANSPOSE 
    par = "TRS";
    if (mstate == 0) {
      if (LEDstatus[ins][5] == 1) {
        notemod[ins] = notemod[ins] + 1;
      }
      if (LEDstatus[ins][6] == 1) {
        notemod[ins] = notemod[ins] - 1;
      }
    }
    if (mstate == 1) {
      if (LEDstatus[ins][5] == 1) {
        notemod[ins] = notemod[ins] + 6;
      }
      if (LEDstatus[ins][6] == 1) {
        notemod[ins] = notemod[ins] - 6;
      }
    }
    if (mstate == 2) {
      if (LEDstatus[ins][5] == 1) {
        notemod[ins] = notemod[ins] + 12;
      }
      if (LEDstatus[ins][6] == 1) {
        notemod[ins] = notemod[ins] - 12;
      }
    }
    mod = m[mstate];  
    state = buf;
    sprintf(buf, "%+04d", notemod[ins]);
    d_m();
    d_p();
    d_s();
  }  
  if (LEDstatus[ins][4] == 0) { // MODE PROG CHANGE
    par = "PRC";  
    if (LEDstatus[ins][5] == 1) {
      prc[ins] = prc[ins] + 1;
    }
    if (LEDstatus[ins][6] == 1) {
      prc[ins] = prc[ins] - 1;
    }
    mod = m[3];
    state = buf;
    sprintf(buf, "%+04d", prc[ins]); 
    d_m();
    d_p();
    d_s();
    if ((ins == 1) | (ins == 2)) {
      MIDIA.sendControlChange (0, prc[ins]*20, ins+1);
      MIDIB.sendControlChange (0, prc[ins]*20, ins+1);
    }
    else {
      MIDIA.sendProgramChange (prc[ins], ins+1);
      MIDIB.sendProgramChange (prc[ins], ins+1);
    }     
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////

void handleNoteOn(byte channel, byte note, byte value) {
  byte note_m = note + notemod[channel-1];
  if(channel == 2) {
    if((note > 47) & (note < 54)) {
      MIDIA.sendNoteOn(note_m, value, 2);
    }      
  }
  if(channel == 3) {
    if((note > 35) & (note < 42)) {
      MIDIA.sendNoteOn(note-35, value, 3);
    }
    if(note > 41) {
      MIDIA.sendNoteOn(note_m, value, 3);
    }
  }
  if ((channel == 1) | (channel == 4)) {
    MIDIA.sendNoteOn(note_m, value, channel);
  }  
}

void handleNoteOff(byte channel, byte note, byte value) {
  byte note_m = note + notemod[channel-1];
  if(channel == 2) {
    if((note > 47) & (note < 54)) {
      MIDIA.sendNoteOff(note_m, value, 2);
    }      
  }
  if(channel == 3) {
    if((note > 35) & (note < 42)) {
      MIDIA.sendNoteOff(note-35, value, 3);
    }
    if(note > 41) {
      MIDIA.sendNoteOff(note_m, value, 3);
    }
  }
  if ((channel == 1) | (channel == 4)) {
    MIDIA.sendNoteOff(note_m, value, channel);
  }  
}
void handleControlChange(byte channel, byte num, byte value) {
  MIDIA.sendControlChange(num, value, channel);
}
void handleProgramChange (byte channel, byte num) {
  MIDIA.sendProgramChange(num, channel);
}

////////////////////////////////////////////////////////////////////////////////////////////////
// DISPLAY
void d_i(void) {
  u8x8.clear();
  u8x8.setFont(u8x8_font_artossans8_u);
  u8x8.drawString(9, 3, "------"); 
  u8x8.drawString(9, 7, "------");
  u8x8.drawString(9, 4, ">>");
  u8x8.setFont(u8x8_font_7x14_1x2_r);
  u8x8.drawString(0, 0, i[ins]);    
}
void d_p(void) {
  //u8x8.clearLine(6);
  u8x8.setFont(u8x8_font_courR18_2x3_r);
  u8x8.drawString(2, 3, par);   
}
void d_m(void) {
  u8x8.setFont(u8x8_font_8x13_1x2_r); 
  u8x8.drawString(2, 6, mod); 
}
void d_s(void) {
  u8x8.setFont(u8x8_font_8x13_1x2_r);
  u8x8.drawString(10, 5, state); 
}
////////////////////////////////////////////////////////////////////////////////////////////XX/
