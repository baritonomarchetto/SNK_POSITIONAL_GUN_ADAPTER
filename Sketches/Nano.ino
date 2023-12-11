 /*
SNK positional gun adapter

Arduino NANO

This sketch reads outputs signals from Howard Casto's Mamehooker.
It also takes care of coils signals singularities (always on, toggling, fast toggling etc. etc.) 
to simplify MAMEHooker setup.

Here is an example of mamehooker .ini code:(messages sent to Arduino serial port COM4)

[General] 
MameStart=cmo 4 baud=9600_parity=N_data=8_stop=1 
MameStop=cmc 4 
StateChange= 
OnRotate= 
OnPause= 
[KeyStates] 
RefreshTime= 
[Output] 
Player1_Recoil_Piston=cmw 4 1.,cmw 4 %s%,cmw 4 x
Player2_Recoil_Piston=cmw 4 2.,cmw 4 %s%,cmw 4 x

by Barito, 2023 (last update dec. 2023)
*/

#define OUTPUTS 8
#define PLAYERS 2
#define FILT_COILS //Comment this for raw coils out; keep it for simplified MAMEHooker setup

const int out_pin[] = {2, 3, 5, 6, 7, 8, 9, 10};//Outputs pins. First two are coils out.

const int mode_pin = 4;
boolean mode_state = 1;//input pullup

int rID;
int value;

unsigned long high_time[PLAYERS];
unsigned long low_time[PLAYERS];
byte kick[PLAYERS]= {0, 0};//single shot phase index
boolean out_state[OUTPUTS];

byte enTime = 50;//coil energize time. The kick needs some time to fully energize.
byte relTime = 50;//coil release time. The kick needs some time to hit the rubber stop.

byte opMode = 0; //operation mode: 0 - kicker triggered by external software (e.g. MAMEHooker); 1 - kicker triggered locally (for unmapped games)

void setup() {
  for (int j = 0; j < OUTPUTS; j++){
    pinMode(out_pin[j], OUTPUT);
  }
  pinMode(mode_pin, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop(){
  ReadCMD();
  ModeSwitch();
  if (opMode == 0){
    #ifdef FILT_COILS
      KickFilter();
    #endif
  }
  else{
    KickLoop();
  }
}

void ReadCMD(){
  while (Serial.available()>0){
    rID = Serial.parseInt();    //first byte (output ID number)
    value = Serial.parseInt();  //second byte (output state)
    if (Serial.read() == 'x'){  //third byte (end of message)
      #ifdef FILT_COILS
        if(rID <=PLAYERS){//kickers states are stored for successive processing
          out_state[rID-1] = value;
        }
        else if(rID <= OUTPUTS){//lamps go out raw
          digitalWrite(out_pin[rID-1], value);
        }
      #else
        if(rID <= OUTPUTS){//everything goes out, raw
          digitalWrite(out_pin[rID-1], value);
        }
      #endif
    }
  }
}

void KickFilter(){
  for (int a = 0; a < PLAYERS; a++){
    switch(kick[a]){
      case 0://start single kick routine
        if (out_state[a] == 1){
          digitalWrite(out_pin[a], HIGH);
          high_time[a] = millis();
          kick[a] = 1;
        }
      break;
      case 1://check high segment and start low
        if (millis() - high_time[a] > enTime){
          digitalWrite(out_pin[a], LOW);
          low_time[a] = millis();
          kick[a] = 2;
        }
      break;           
      case 2://check low segment and close routine
        if (millis() - low_time[a] > relTime){
          kick[a] = 0;
        }
      break;
    }
  }
}

void KickLoop(){
  for (int a = 0; a < PLAYERS; a++){
    switch(kick[a]){
      case 0:
        digitalWrite(out_pin[a], HIGH);
        high_time[a] = millis();
        kick[a] = 1;
      break;
      case 1:
        if (millis() - high_time[a] > enTime){
          digitalWrite(out_pin[a], LOW);
          low_time[a] = millis();
          kick[a] = 2;
        }
      break;
      case 2:
        if (millis() - low_time[a] > relTime){
          kick[a] = 0;
        }
      break;
    }
  }
}

void ModeSwitch(){
  if(digitalRead(mode_pin) != mode_state){
    mode_state = !mode_state;
    opMode = !mode_state;
    delay(100); //cheap debounce
  }
}
