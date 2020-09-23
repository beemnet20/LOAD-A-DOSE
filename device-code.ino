
// include all the libraries to be used
#include <AccelStepper.h> //Library to control the stepper motors must be downloaded and installed separately
#include <Wire.h> //comes with the Arduino IDE, allows communication with I2C devices 
#include <LiquidCrystal_I2C.h> // controls the LCD monitor, must be downloaded and installed separately
#include <Adafruit_Soundboard.h> // controls the soundboard, must be downloaded and installed separately 

// Define the Pins used
#define home_switch 10 // for bringing the syringe to 0 units 
#define ResetButton 4 // reset/start button for the device

//switches to be used in vialUp and vialDown functions
#define vialDown_Switch 31
#define vialUp_Switch 33

// user input button pins
#define UpButton 52
#define DownButton 50
#define CancelButton 48
#define EnterButton 46

//enable pins to tell the easydriver when to draw current
#define EnS 13 // enable pin for the syringe stepper 
#define EnV 27 // enable pin for the vial stepper 

//pins for the soundboard
#define SFX_TX 18 // Transmit pin, goes to RX on the soundboard
#define SFX_RX 19 // Receive pin, goes to TX on the soundboard
#define SFX_RST 3 // reset for the soundboard 

// initiate/define your devices
LiquidCrystal_I2C lcd(0x27, 20, 4); // adress is 0x27, number of display columns is 20 and display rows is 4

AccelStepper stepperS(1, 11, 12);   //stepper motor for the syringe
// 1 = Easy Driver interface
//  Pin 11 connected to STEP pin of Easy Driver
//  Pin 12 connected to DIR pin of Easy Driver

AccelStepper stepperV(1, 23, 25);   //stepper motor for the vial
// 1 = Easy Driver interface
//  Pin 23 connected to STEP pin of Easy Driver
//  Pin 25 connected to DIR pin of Easy Driver

//define the soundboard
Adafruit_Soundboard sfx = Adafruit_Soundboard(&Serial1, NULL, SFX_RST);

// define variables for controlling the stepper motors
int TravelS;// for the syringe stepper motor travel
int TravelV;// for the vial stepper motor travel
int procedure = -1; // Used to count which procedural step the device is at
long initialS_homing = -1; // Used to bring the syringe stepper motor to zero U
long initialV_homing = -1; // Used to bring the vial holder to its upper most position at startup
int defaultDose = 10; // This can be changed based on user need
int Dose = defaultDose ; // sets the current dose count to the defaultDose
int SpecifiedDose = 0; // This will be set by user input
int EnterCount = 0; // to count how many times enter button has been pressed
int InputComplete = 0; // will be incremented when user input is complete
int EnterInstructions = 0;// will be incremented when instructions are given (for EnterButton)
int FinalInstructions = 0;// will be incremented  when a single dose loading is complete


// procedure -1 Default procedure number when the device is started up
// procedure 0  the syringe is homed, vial holder is up
// procedure 1  dose is specified
// procedure 2  device has started drawing air
// procedure 3  air drawing is complete
// procedure 4  vial holder is down start pressurizing the vial
// procedure 5  vial is pressurized, start drawing 2 units of insulin for priming
// procedure 6  injecting the 2 units back into the vial has begun
// procedure 7  drawing the specified dose has begun 
// procedure 8  specified dose has been drawn, vial holder has been moved up 
// finally  insulin is drawn we can remove the syringe

void setup() {
  // initiate the LCD monitor, display greeting message
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("    LOAD-A-DOSE");
  lcd.setCursor(0, 2);
  lcd.print(" press START");

  // define the digital pin modes
  // INPUT_PULLUP means the button or switch is used for input and is
  //connected to an internal pullup resistor
  // The enable pins are OUTPUT since they regulate current delivered to easy drivers
  pinMode(home_switch, INPUT_PULLUP);
  pinMode(vialDown_Switch, INPUT_PULLUP);
  pinMode(vialUp_Switch, INPUT_PULLUP);
  pinMode(EnS, OUTPUT);
  pinMode(EnV, OUTPUT);
  pinMode (UpButton, INPUT_PULLUP);
  pinMode (DownButton, INPUT_PULLUP);
  pinMode (CancelButton, INPUT_PULLUP);
  pinMode (EnterButton, INPUT_PULLUP);
  pinMode (ResetButton, INPUT_PULLUP);

  //start the serial communication at 9600 Baud
  //press CTRL+SHIFT+M to open the serial monitor
  Serial.begin(9600);
  Serial1.begin(9600);//for communication with the soundboard

  //"look" if the soundboard is properly connected
  Serial.println("Adafruit Sound Board!");
  if (!sfx.reset()) {
    Serial.println("Not found");
    while (1);
  }
  Serial.println("SFX board found");
  //  audioFiles();// calling the function that lists the audioFiles on the soundboard

  //start greeting
  sfx.playTrack((uint8_t)76);// play the start jingle
  delay(1780);// wait 1780 milliseconds for jingle to end
  sfx.playTrack((uint8_t)75); // play "to start the loading procedure press start"

  digitalWrite(EnS, HIGH);
  digitalWrite(EnV, HIGH);

  //set up the initial speed and location of the syringe stepper motor
  stepperV.setMaxSpeed(500.0);      // Set Max Speed of Stepper (Slower to get better accuracy)
  stepperV.setAcceleration(500.0);  // Set Acceleration of Stepper
  stepperV.setCurrentPosition(0);

  audioFiles();
}

void loop() {
  //stepper.run must be called as often as possible in the loop so leave it here
  stepperS.run();
  stepperV.run();

  // move the vial up-> when the vial is all the way up move the syringe to 0 units
  if (procedure == -1 && digitalRead(ResetButton) == LOW ) {
    lcd.clear();
    lcd.setCursor(0, 2);
    lcd.print("  Getting ready...");
    sfx.playTrack((uint8_t)58);// play "please wait while load a dose is getting ready"
    vialUp();// calling the function to move the vial holder up

    //when the vial holder is all the way up move the syringe to 0 units
    if ( digitalRead(vialUp_Switch) == LOW) {
      digitalWrite(EnS, LOW);// only enable the stepper motor when you want it to move so that it doesn't overheat
      // enable the stepper before telling the stepper to move
      syringeHome();
      digitalWrite(EnS, HIGH); //disable the syringe stepper once its done moving
      procedure = 0; // increment the procedure to go to the next step
    }
  }

  //Start tracking user input
  if (procedure == 0 ) {
    // if the UpButton is pressed increase the Dose by one unit
    if (digitalRead(UpButton) == LOW && Dose >= 1 && Dose <= 49) {
      delay(100);//add a delay to debounce the button (to make sure they actually meant to press it)
      if (digitalRead(UpButton) == LOW) {
        ++Dose;// increase the dose by one unit
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" ENTER INSULIN UNITS");
        lcd.setCursor(0, 1);
        lcd.print("    ");
        lcd.print(Dose); // display the current dose on the LCD
        lcd.print(" Units");
        sfx.playTrack((uint8_t)Dose);// say outloud the current dose
      }
    }
    // if the DownButton is pressed decrease the Dose by one unit
    if (digitalRead(DownButton) == LOW && Dose > 1 && Dose <= 50) {
      delay(100); // debounce
      if (digitalRead(DownButton) == LOW) {
        --Dose;// decrease the dose by one unit
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" ENTER INSULIN UNITS");
        lcd.setCursor(0, 1);
        lcd.print("    ");
        lcd.print(Dose);// display the current dose
        lcd.print(" Units");
        sfx.playTrack((uint8_t)Dose);// say outloud the current dose
      }
    }

    // if the user tries to go above 50 units tell them they can't
    if (Dose == 50 && digitalRead(UpButton) == LOW ) {
      delay(100);
      if (digitalRead(UpButton) == LOW) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" ENTER INSULIN UNITS");
        lcd.setCursor(0, 2);
        lcd.print(" 50 U is Maximum");
        Dose = 50;
        sfx.playTrack((uint8_t)63);// play "50 is maximum"
      }
    }

    // if the user tries to go below 1 unit tell them they can't
    if (Dose == 1 && digitalRead(DownButton) == LOW ) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" ENTER INSULIN UNITS");
      lcd.setCursor(0, 1);
      lcd.print(" 1 Unit is minimum ");
      sfx.playTrack((uint8_t)65); // play "1 is the lowest dose"
    }

    // if the person presses the CancelButton reset the dose to 1 unit
    if (digitalRead(CancelButton) == LOW) {
      delay(400);//debounce
      if (digitalRead(CancelButton) == LOW) {
        Dose = 1;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" ENTER INSULIN UNITS");
        lcd.setCursor(0, 1);
        lcd.print(" Dose Canceled");
        lcd.setCursor(0, 2);
        lcd.print("Dose reset to ");
        lcd.setCursor(0, 3);
        lcd.print("    ");
        lcd.print(Dose);
        lcd.print(" Units");
        EnterCount = 0;
        EnterInstructions = 0;
        sfx.playTrack((uint8_t)62); // play"insulin reset to 1 unit"
      }
    }

    // if the person presses the Enter Button for the first time, make sure they really want the current dose
    if (digitalRead(EnterButton) == LOW && EnterCount == 0) {
      delay(100);
      if (digitalRead(EnterButton) == LOW) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" ENTER INSULIN UNITS");
        lcd.setCursor(0, 1);
        lcd.print(" You want ");
        lcd.print(Dose);
        lcd.print(" Units ?");
        lcd.setCursor(0, 2);
        lcd.print("hold ENTER or CANCEL");
        if (EnterInstructions == 0) {
          delay(100);
          sfx.playTrack((uint8_t)51);// play "are you sure you want"
          EnterInstructions = 1; // increment to go through the steps of instructing the user
        }
        if (EnterInstructions == 1) {
          delay(1600);//delay until previous track stops playing
          sfx.playTrack((uint8_t)Dose);// play the current dose outloud
          EnterInstructions = 2;
        }
        if (EnterInstructions == 2) {
          delay(900);//delay until previous track stops playing
          sfx.playTrack((uint8_t)79);// play "units of insulin"
          EnterInstructions = 3;
        }

        if (EnterInstructions == 3) {
          delay(1500);
          sfx.playTrack((uint8_t)59);// play "hold enter for yes"
          EnterInstructions = 4;
        }
        if (EnterInstructions == 4) {
          delay(2700);
          sfx.playTrack((uint8_t)84);// play "hold cancel for no"
        }
        EnterCount = 1; //increment EnterCount
      }
    }

    if (digitalRead(EnterButton) == LOW && EnterCount == 1) {
      delay(400);
      if (digitalRead(EnterButton) == LOW) {
        EnterCount == 2;
        SpecifiedDose = Dose;
        sfx.playTrack((uint8_t)SpecifiedDose); // say aloud the specified dose
        delay(900);
        sfx.playTrack((uint8_t)79);// play "units of insulin"
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print(" Specified dose:");
        lcd.setCursor(0, 2);
        lcd.print("    ");
        lcd.print(Dose);
        lcd.print(" Units");

        procedure = 1; // procedure 1 complete, dose is specified
        TravelS = SpecifiedDose; // This will be used to move the syringe stepper
        InputComplete = 1; // User input is complete
        EnterInstructions = 0; // reset EnterInstructions to 0 so that another input can be made during Reset
      }
    }
  }
  // if the specified dose is greater than or equal to 1 and less than or equal to 50
  //and dose is specified (procedure 1 complete)
  if (TravelS >= 1 && TravelS <= 50 && procedure == 1) {
    delay(1500);
    sfx.playTrack((uint8_t)82); // play "keep hands clear of vial holder"
    digitalWrite(EnS, LOW);// enable the syringe stepper
    //draw air the amount of the specified dose
    stepperS.moveTo(TravelS * 336); // 336 is the multiplier that translates the specified dose amount to amount of steps
    stepperS.setSpeed(300);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Drawing air...");
    lcd.setCursor(0, 1);
    lcd.print("    ");
    lcd.print(TravelS);
    lcd.print(" Units");
    procedure = 2; // device has started drawing air
  }
  // if the stepper has drawn air increment the procedure
  if (InputComplete == 1 && (procedure == 2) && (stepperS.distanceToGo() == 0)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Air drawn: ");
    lcd.setCursor(0, 1);
    lcd.print("    ");
    lcd.print(TravelS);
    lcd.print(" Units");
    procedure = 3; // air drawing is complete
  }
  // if air drawing is complete move the vial holder down
  if (procedure == 3) {
    vialDown(); // call function to move vial holder down
    // if the vial holder is at its lower most position go increment the procedure
    // and pressurize the vial pressure
    if (digitalRead(vialDown_Switch) == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" Pressurizing vial");
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.print(TravelS);
      lcd.print(" Units");
      digitalWrite(EnS, LOW);
      stepperS.moveTo(0);// inject the air into the vial by having the plunger go to 0
      procedure = 4; // vial holder is down
    }
  }
  // if the vial holder is down and all the air has been injected into vial
  // draw 2 units of insulin to prime the syringe
  if (procedure == 4 && (stepperS.currentPosition() == 0)) {
    digitalWrite(EnS, LOW);
    stepperS.moveTo(2 * 336);// draw 2 units of insulin
    stepperS.setSpeed(100);
    procedure = 5; // drawing the priming insulin has begun
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Priming syringe...");
    lcd.setCursor(0, 1);
    lcd.print("    2 Units");
  }
  // if the priming insulin has been completely drawn, inject it back into the vial
  if (procedure == 5 && (stepperS.distanceToGo() == 0)) {
    digitalWrite(EnS, LOW);
    stepperS.moveTo(0);
    stepperS.setSpeed(100);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Priming syringe...");
    lcd.setCursor(0, 1);
    lcd.print("    2 Units");
    procedure = 6;// injecting the 2 units back into the vial has begun
  }
  // if the priming insulin has been completely injected back into the vial
  // draw the specified dose
  if (procedure == 6 && (stepperS.currentPosition() == 0)) {
    digitalWrite(EnS, LOW);
    stepperS.moveTo(TravelS * 336); //drawing the specified dose
    stepperS.setSpeed(200);
    procedure = 7;// drawing the specified dose has begun 
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Drawing insulin...");
    lcd.setCursor(0, 1);
    lcd.print("    ");
    lcd.print(TravelS);
    lcd.print(" Units");
  }
  // if the specified dose has been completely drawn move the vial holder up 
  if (procedure == 7 && (stepperS.distanceToGo() == 0)) {
    vialUp();
    // if the vial holder is all the way up 
    // instruct the user to remove the syringe 
    // instruct the user how to load another dose 
    if (digitalRead(vialUp_Switch) == LOW) {
      procedure = 8;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(" Insulin drawn:");
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.print(TravelS);
      lcd.print(" Units");
      lcd.setCursor(0, 2);
      lcd.print("  Remove Syringe");

      if (FinalInstructions == 0 ) {
        sfx.playTrack((uint8_t)73);//play " please remove syringe"
        FinalInstructions = 1;
      }
      if (FinalInstructions == 1) {
        delay(1540);
        sfx.playTrack((uint8_t)81);// play "procedure complete"
        FinalInstructions = 2;
      }
      if (FinalInstructions == 2) {
        delay(2400);
        sfx.playTrack((uint8_t)64); // play "load another dose"
        FinalInstructions = 3;
      }
    }
  }
  // if the first specified dose has been drawn and the person wants to load another dose
  // move the syringe to 0 
  // restart the process
  if (procedure == 8 && digitalRead(ResetButton) == LOW) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(" Syringe is removed.");
    lcd.setCursor(0, 1);
    lcd.print(" Resetting device...");
    digitalWrite(EnS, LOW);
    //vialUp();// commented out for testing
    vialDown();
    syringeHome();
    vialUp(); // inserted for testing
    procedure = 0;// reset to 0
    EnterCount = 0;// reset to 0 
    EnterInstructions = 0;// reset to 0
    FinalInstructions = 0;// reset to 0 
  }
  // if the syringe stepper is not moving disable it
  if (stepperS.distanceToGo() == 0) {
    digitalWrite(EnS, HIGH);
  }

}

// a function to bring the syringe to 0 units
void syringeHome() {
  Dose = defaultDose;
  delay(5);  // Wait for EasyDriver wake up

  //  Set Max Speed and Acceleration of each Steppers at startup for homing
  stepperS.setMaxSpeed(500.0);      // Set Max Speed of Stepper
  stepperS.setAcceleration(500.0);  // Set Acceleration of Stepper

  Serial.print("Pushing Syringe to 0 U . . . . . . . . . . . ");
  while (digitalRead(home_switch)) {  // Make the Stepper move until the switch is activated
    stepperS.moveTo(initialS_homing);  // Set the position to move to
    initialS_homing--;  // Decrease by 1 for next move if needed
    stepperS.run();  // Start moving the stepper
    delay(5);
  }
  stepperS.setCurrentPosition(0);  // Set the current position as zero for now
  Serial.println("Syringe at 0 U");
  sfx.playTrack((uint8_t)52); // play "the syringe is now at 0"
  stepperS.setMaxSpeed(500.0);      // Set Max Speed of Stepper
  stepperS.setAcceleration(500.0);  // Set Acceleration of Stepper
  delay(2650); // this delay is for track 52 to be played completely

  //Change the messages on the LCD monitor
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" ENTER INSULIN UNITS");
  lcd.setCursor(0, 1);
  lcd.print("    ");
  lcd.print(Dose);
  lcd.print(" Units");
  sfx.playTrack((uint8_t)56); // play "enter number of units 1-50"
}

// a function for listing the audio files saved on the soundboard
// to display the audio files call the function "audioFiles();" either in set up or within the loop
void audioFiles() {
  uint8_t files = sfx.listFiles();
  for (uint8_t f = 0; f < files; f++) {
    Serial.print(f);
    Serial.print("\tname: "); Serial.print(sfx.fileName(f));
    Serial.print("\tsize: "); Serial.println(sfx.fileSize(f));
  }
}

// a function to bring the vial holder to its upper most location
void vialUp() {
  stepperV.setMaxSpeed(500.0);
  stepperV.setAcceleration(500.0);

  //while the vialUp_switch is not pressed in enable the vial stepper motor
  //and move the vial holder up until it hits the switch
  while (digitalRead(vialUp_Switch)) {
    digitalWrite(EnV, LOW);
    stepperV.moveTo(initialV_homing);
    initialV_homing--;
    stepperV.run();
    delay(5);
  }
  // if the viaUp_switch is pressed in disable the vial stepper motor so that it doesn't ovehreat
  // also stop the vial stepper motor
  if (digitalRead(vialUp_Switch) == LOW) {
    digitalWrite(EnV, HIGH);
    stepperV.stop();
  }
}

// a function to bring the vial holder to its lower most location
void vialDown() {
  stepperV.setMaxSpeed(500.0);
  stepperV.setAcceleration(500.0);

  //while the vial_down switch is not pressed in, enable the vial stepper motor
  //move the stepper motor down until it hits the switch
  while (digitalRead(vialDown_Switch)) {
    digitalWrite(EnV, LOW);
    stepperV.moveTo(initialV_homing);
    initialV_homing++;
    stepperV.run();
    delay(5);
  }
  // if the vialDown_switch is pressed in disable the vial stepper motor
  //stop the vial stepper motor.
  if (digitalRead(vialDown_Switch) == LOW) {
    digitalWrite(EnV, HIGH);
    stepperV.stop();
  }
}
