
#include <Adafruit_NeoPixel.h>
#include "Arduino.h"
#include "SoftwareSerial.h"
#include "DFRobotDFPlayerMini.h"
#include <Stepper.h>
#include <MsTimer2.h>
#include "BPM.h"

SoftwareSerial mySoftwareSerial(10, 11); // RX, TX
DFRobotDFPlayerMini myDFPlayer;

#define PinA 2
#define PinB 3
#define PinC 4
#define PinD 5
#define PIRPin 8
#define BusyPin 7
#define NeoPin 9

#define LED_MAX_BRIGHT 63
#define SHOW_SWITCH 10000

//******** Vars für Neopixel *******
#define STRIP_LEN 4
Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIP_LEN, NeoPin, NEO_GRB + NEO_KHZ800);
long ShowSwitchTime = 0;
int ShowState = 0;


//********** PIR ************
int PrePir = 0;
int Pir = 0;
int NewPir = 0;
uint32_t PirTime = 0;

//********** BPM-Array***********
#define LEN_BPM 35
long BPMOnTime = 0;
long BPMOffTime = 0;
bool BPMNew = false, BPMOn = true, BPMOff = false;

//************* DFPlayer ***********
#define MIN_RUNTIME 10000
#define MAX_ON_CARD 100
bool FilesPlayed[100];
uint32_t VolTime = 0, NextTime = 0;
int NoOfFiles = 0;
int CurrentFile = 2;
int LastFile = 2;
int Volume = 0, LastVolume = 0;

int GetNextFile (void) {
  int NextFile = 0, Tested = 0, i = 0, PlayCount = 0;
  PlayCount = 0;
  for (i = 0; i < MAX_ON_CARD; i++) {
    if (FilesPlayed[i])
      PlayCount++;
  }
  if (PlayCount >= NoOfFiles) {
    Serial.println("Reset PlayCount und Array");
    PlayCount = 0;
    for (i = 0; i < MAX_ON_CARD; i++) {
      FilesPlayed[i] = false;
    }
  }
  Serial.print("Bisher gespielt = ");
  Serial.println(PlayCount);
  NextFile = int(random(1, NoOfFiles + 1));
  while (FilesPlayed[NextFile]) {
    NextFile = int(random(1, NoOfFiles + 1));
    Tested++;
  }
  FilesPlayed[NextFile] = true;
  BPMNew = true;
  if (CurrentFile < LEN_BPM)
    BPMOnTime = millis() + pgm_read_word(&bpm[CurrentFile]);
  else
    BPMOnTime = millis() + pgm_read_word(&bpm[0]);
  //BPMOn = true;
  Serial.print("BPM = ");
  if (CurrentFile < LEN_BPM)
    Serial.println(pgm_read_word(&bpm[CurrentFile]));
  else
    Serial.println(pgm_read_word(&bpm[0]));
  Serial.print("NextFile = ");
  Serial.print(NextFile);
  Serial.print("...Tested = ");
  Serial.println(Tested);
  return NextFile;
}

//******* non-blocking Stepper **********
enum StepState {halt = 0, vor, rueck};
StepState Dir = halt;
int StepsPerRevolution = 4096;

const bool seq_r[4][4] = {
  {1, 0, 0, 0},
  {0, 1, 0, 0},
  {0, 0, 1, 0},
  {0, 0, 0, 1}
};

const bool seq_v[4][4] = {
  {0, 0, 0, 1},
  {0, 0, 1, 0},
  {0, 1, 0, 0},
  {1, 0, 0, 0}
};

int ThisStep = 0;

void Step(void)
{
  static int LastStep = 0, Div3ms = 0;
  //Serial.println(Dir);
  if (!Div3ms) {
    switch (Dir) {
      case 1:
        digitalWrite(PinA, seq_v[0][ThisStep]);
        digitalWrite(PinB, seq_v[1][ThisStep]);
        digitalWrite(PinC, seq_v[2][ThisStep]);
        digitalWrite(PinD, seq_v[3][ThisStep]);
        break;
      case 2:
        digitalWrite(PinA, seq_r[0][ThisStep]);
        digitalWrite(PinB, seq_r[1][ThisStep]);
        digitalWrite(PinC, seq_r[2][ThisStep]);
        digitalWrite(PinD, seq_r[3][ThisStep]);
        break;
      case 0:
      default:
        digitalWrite(PinA, 0);
        digitalWrite(PinB, 0);
        digitalWrite(PinC, 0);
        digitalWrite(PinD, 0);
        break;
    }
    ThisStep++;
    if (ThisStep > 3)
      ThisStep = 0;
    Div3ms = 3;
  }
  Div3ms--;
  if ((millis() - PirTime) > 100) {
    PirTime = millis();
    Pir = digitalRead(PIRPin);
    if (Pir != PrePir) {
      Serial.print("PIR ist ");
      Serial.println(Pir);
      PrePir = Pir;
      if (Pir)
        NewPir = 1;
    }
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println("Hallo Zigarrenkiste!");
  pinMode(PinA, OUTPUT);
  pinMode(PinB, OUTPUT);
  pinMode(PinC, OUTPUT);
  pinMode(PinD, OUTPUT);
  pinMode(PIRPin, INPUT_PULLUP);
  pinMode(BusyPin, INPUT_PULLUP);
  for (int i = 0; i < (MAX_ON_CARD); i++) {
    FilesPlayed[i] = false;
  }
  MsTimer2::set(3, Step); // 3ms period
  MsTimer2::start();
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  mySoftwareSerial.begin(9600);
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!myDFPlayer.begin(mySoftwareSerial)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true) {
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));
  Serial.print("Anzahl der MP3-Dateien: ");
  NoOfFiles = myDFPlayer.readFileCounts();
  Serial.println(NoOfFiles);
  Dir = halt;
  randomSeed(millis());
  MsTimer2::set(1, Step); // 500ms period
  MsTimer2::start();
  Pir = digitalRead(PIRPin);
  PirTime = millis();
  myDFPlayer.volume(map(analogRead(0), 0, 1023, 0, 30));
  myDFPlayer.play(CurrentFile);
  Dir = vor;
  Serial.print("Start! Now playing...");
  Serial.println(CurrentFile); //read current play file number
  FilesPlayed[CurrentFile] = true;
  //while(digitalRead(PIRPin));
  strip.setPixelColor(0, 0x000800);
  strip.show();
  delay(10000);
  NewPir = 0;
  Serial.println("Los geht's");
  strip.setPixelColor(0, 0x000000);
  strip.show();
}

void loop() {
  if ((millis() - VolTime) > 50) {
    Volume = analogRead(0);
    if(Volume != LastVolume) {
      myDFPlayer.volume(map(Volume, 0, 1023, 0, 30));
      LastVolume = Volume;
    }
    VolTime = millis();;
  }

  if (((millis() - NextTime) > MIN_RUNTIME) && NewPir) {
    NextTime = millis();
    NewPir = 0;
    CurrentFile = GetNextFile();
    LastFile = CurrentFile;
    myDFPlayer.play(CurrentFile);  //Play next mp3 every 3 second.
    delay(1000);
    //Serial.print("Gewischt! Now playing...");
    //Serial.println(CurrentFile); //read current play file number
    if (Dir == vor)
      Dir = rueck;
    else
      Dir = vor;
  }
  else if (digitalRead(BusyPin)) {
    CurrentFile = GetNextFile();
    LastFile = CurrentFile;
    myDFPlayer.play(CurrentFile);  //Play next mp3 every 3 second.
    //Serial.print("Normal weiter. Now playing...");
    //Serial.println(CurrentFile); //read current play file number
    delay(1000);
    if (Dir == vor)
      Dir = rueck;
    else
      Dir = vor;
  }

  if ((millis() > BPMOnTime) && BPMOn) {
    if (CurrentFile < LEN_BPM)
      BPMOnTime = millis() + pgm_read_word(&bpm[CurrentFile]);
    else
      BPMOnTime = millis() + pgm_read_word(&bpm[0]);
    BPMOffTime = millis() + 50;  //50ms blinken
    BPMOff = true;
    BPMOn = false;
    if (millis() > ShowSwitchTime) {
      ShowState = random(0, 3);
      ShowSwitchTime = millis() + SHOW_SWITCH;
    }
    switch (ShowState) {
      case 0:
        strip.setPixelColor(0, strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        strip.setPixelColor(1, strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        strip.setPixelColor(2, strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        strip.setPixelColor(3, strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        break;
      case 1:
        strip.setPixelColor(0, 0);
        strip.setPixelColor(1, 0);
        strip.setPixelColor(2, 0);
        strip.setPixelColor(3, 0);
        strip.setPixelColor(random(0, STRIP_LEN), strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        break;
      case 2:
      default:
        strip.setPixelColor(0, 0);
        strip.setPixelColor(1, 0);
        strip.setPixelColor(2, 0);
        strip.setPixelColor(3, 0);
        strip.setPixelColor(random(0, STRIP_LEN), strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        strip.setPixelColor(random(0, STRIP_LEN), strip.Color(random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT), random(0, LED_MAX_BRIGHT)));
        break;
    }
    strip.show();
    //Serial.println("On");
  }

  if ((millis() > BPMOffTime) && BPMOff) {
    BPMOff = false;
    BPMOn = true;
    strip.setPixelColor(0, 0x000000);
    strip.setPixelColor(1, 0x000000);
    strip.setPixelColor(2, 0x000000);
    strip.setPixelColor(3, 0x000000);
    strip.show();
    //Serial.println("Off");
  }
}
