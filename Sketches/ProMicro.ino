/*
SNK positional gun adapter

Arduino Pro Micro
Two players

This sketch converts inputs from two positional guns to a Direct Input Joystick.
It also reads external inputs (from arduino NANO) to drive kickers.
An authomatic calibration routine has been coded.
Keeping pressed COIN 2 button and pressing:
- SERVICE reduces the gun span range on the X axis
- TEST offsets P1 and P2 central positions 

by Barito, 2023 (last update dec. 2023)
*/

#include <Joystick.h>

#define SWITCHES 10

#define PLAYERS 2

int P1_X_MIN = 1023;
int P1_X_MAX = 0;
int P1_Y_MIN = 1023;
int P1_Y_MAX = 0;
int P2_X_MIN = 1023;
int P2_X_MAX = 0;
int P2_Y_MIN = 1023;
int P2_Y_MAX = 0;

int P1_Xcorr; //Aspect ratio correction on X axis
int P2_Xcorr; //Aspect ratio correction on X axis
int Xcorr_factor; //range reduction factor on X axis
byte Xcorr_count = 0;

int P1_Xrange;
int P2_Xrange;
int Xpos_offset; //positional offset between players
int Xpos_factor = 15; //actual offset range percentage 
bool act_players = 0; //how many players, actually? 0 - 1; 1 - 2.

const int delayTime = 20;
const int noLoss = 8;

Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, 
  JOYSTICK_TYPE_GAMEPAD, SWITCHES, 0, //joy type, button count, hatswitch count
  true, true, false, // X, Y, Z axis
  true, true, false, // X, Y, Z rotation
  false, false, //rudder, throttle
  false, false, false); //accelerator, brake, steering

struct digitalInput {const byte pin; boolean state; unsigned long dbTime; const byte btn;} 
digitalInput[SWITCHES] = {
  {2, HIGH, 0, 0},  //TEST
  {3, HIGH, 0, 1},  //P1 COIN
  {4, HIGH, 0, 2},  //P1 TRIGGER
  {5, HIGH, 0, 3},  //P2 COIN
  {6, HIGH, 0, 4},  //SERVICE
  {7, HIGH, 0, 5},  //P2 START
  {8, HIGH, 0, 6},  //P2 BOMB/GRENADE
  {9, HIGH, 0, 7},  //P2 TRIGGER
  {10, HIGH, 0, 8}, //P1 START
  {16, HIGH, 0, 9,} //P1 BOMB/GRENADE
};

const byte P1_X = A2;
const byte P1_Y = A3;
const byte P2_X = A0;
const byte P2_Y = A1;

const byte trigger[PLAYERS] = {2, 7};// trigger buttons POSITIONS in struct digitalInput
const byte coil[PLAYERS] = {14, 15}; //pro micro coil outs pins
const byte nano[PLAYERS] = {0, 1}; //pro micro coil in pins (from NANO). Please notice that TX0 pin is actually pin 1, Rx1 is 0

int P1_X_Value;
int P1_Y_Value;
int P2_X_Value;
int P2_Y_Value;

boolean nano_state[PLAYERS];
boolean prev_nano[PLAYERS];

void setup(){
  for (int j = 0; j < SWITCHES; j++){
    pinMode(digitalInput[j].pin, INPUT_PULLUP);
    digitalInput[j].state = digitalRead(digitalInput[j].pin);
    digitalInput[j].dbTime = millis();
  }
  pinMode(nano[0], INPUT);
  pinMode(nano[1], INPUT);
  pinMode(P1_X, INPUT);
  pinMode(P1_Y, INPUT);
  pinMode(P2_X, INPUT);
  pinMode(P2_Y, INPUT);

  pinMode(coil[0], OUTPUT);
  pinMode(coil[1], OUTPUT);
  
  Joystick.begin();
  //Joystick.setXAxisRange(0, 1023);
  //Joystick.setYAxisRange(0, 1023);
  //Joystick.setRxAxisRange(0, 1023);
  //Joystick.setRyAxisRange(0, 1023);
}

void loop(){
  DigitalInputs();
  ShiftFunctions();
  Recoils_out();
  X_Corrections();
  AnalogInputs();
}

void DigitalInputs(){
  //general input handling
  for (int j = 0; j < SWITCHES; j++){
    if (millis()-digitalInput[j].dbTime > delayTime && digitalRead(digitalInput[j].pin) !=  digitalInput[j].state){
      digitalInput[j].state = !digitalInput[j].state;
      digitalInput[j].dbTime = millis();
      Joystick.setButton(digitalInput[j].btn, !digitalInput[j].state);
      delay(noLoss);
    }
  }
}

void ShiftFunctions(){
  if(digitalInput[3].state == LOW){ //COIN 2 PRESSED
    //range span correction 
    if(digitalInput[4].state == LOW){ //SERVICE PRESSED
      Xcorr_count++;
      if(Xcorr_count == 4){
        Xcorr_count = 0;
      }
      switch(Xcorr_count){
        case 0:
          Xcorr_factor = 0;
        break;
        case 1:
          Xcorr_factor = 10; //this should be good for 4:3 games
        break;
        case 2:
          Xcorr_factor = 15; //this should be a good tradeoff for 16:9 games
        break;
        case 3:
          Xcorr_factor = 17; //this could be too much...
        break;
      }
      delay(500);//this is to avoid multiple presses
    }
    else if (digitalInput[0].state == LOW){ //TEST PRESSED
      act_players = !act_players;
      delay(500);//this is to avoid multiple presses
    }
  }
}

void X_Corrections(){
  P1_Xrange = P1_X_MAX - P1_X_MIN;
  P2_Xrange = P2_X_MAX - P2_X_MIN;
  P1_Xcorr = P1_Xrange * Xcorr_factor /100; //RANGE correction for PLAYER 1
  P2_Xcorr = P2_Xrange * Xcorr_factor /100; //RANGE correction for PLAYER 2
  Xpos_offset = act_players * P1_Xrange * Xpos_factor /100; // Players relative OFFSET on the X axis
}

void AnalogInputs(){
  P1_X_Value = analogRead(P1_X);
  if (P1_X_Value > P1_X_MAX){//axis calibration
    P1_X_MAX = P1_X_Value;
  }
  else if (P1_X_Value < P1_X_MIN){
    P1_X_MIN = P1_X_Value;
  }
  P1_X_Value = map(P1_X_Value, P1_X_MIN, P1_X_MAX, 1023 - P1_Xcorr - Xpos_offset, 0 + P1_Xcorr - Xpos_offset);
  if(P1_X_Value < 0) {P1_X_Value = 0;}
  else if(P1_X_Value > 1023) {P1_X_Value = 1023;}
  Joystick.setXAxis(P1_X_Value);
  
  P1_Y_Value = analogRead(P1_Y);
  if (P1_Y_Value > P1_Y_MAX){//axis calibration
    P1_Y_MAX = P1_Y_Value;
  }
  else if (P1_Y_Value < P1_Y_MIN){
    P1_Y_MIN = P1_Y_Value;
  }
  P1_Y_Value = map(P1_Y_Value, P1_Y_MIN, P1_Y_MAX, 1023, 0);
  Joystick.setYAxis(P1_Y_Value);
  
  P2_X_Value = analogRead(P2_X);
  if (P2_X_Value > P2_X_MAX){//axis calibration
    P2_X_MAX = P2_X_Value;
  }
  else if (P2_X_Value < P2_X_MIN){
    P2_X_MIN = P2_X_Value;
  }
  P2_X_Value = map(P2_X_Value, P2_X_MIN, P2_X_MAX, 1023 - P2_Xcorr + Xpos_offset, 0 + P2_Xcorr + Xpos_offset);
  if(P2_X_Value < 0) {P2_X_Value = 0;}
  else if(P2_X_Value > 1023) {P2_X_Value = 1023;}
  Joystick.setRxAxis(P2_X_Value);

  P2_Y_Value = analogRead(P2_Y);
  if (P2_Y_Value > P2_Y_MAX){//axis calibration
    P2_Y_MAX = P2_Y_Value;
  }
  else if (P2_Y_Value < P2_Y_MIN){
    P2_Y_MIN = P2_Y_Value;
  }
  P2_Y_Value = map(P2_Y_Value, P2_Y_MIN, P2_Y_MAX, 1023, 0);
  Joystick.setRyAxis(P2_Y_Value);
}

void Recoils_out(){
  for (int a = 0; a < PLAYERS; a++){
    if(digitalInput[trigger[a]].state == LOW){//P1/P2 TRIGGER PRESSED
      //digitalWrite(coil[a], digitalRead(nano[a]));
      nano_state[a] = digitalRead(nano[a]);
      if(nano_state[a] != prev_nano[a]){//outputs on state change only
        digitalWrite(coil[a], nano_state[a]);
        prev_nano[a] = nano_state[a];
      }
    }
    else{//TRIGGER NOT PRESSED
      digitalWrite(coil[a], LOW);
    }
  }
}
