/*
 Name:		modelRocketController.ino
 Created:	9/2/2018 4:59:36 PM
 Author:	mvirdone

*/

/************************/
// Program Flow
// 1) Startup - Display Ready on the LEDs
// 2) 

/************************/

// Design uses a 4x8x8 LED array using the MAX7219 I/O expander (e.g. https://www.amazon.com/gp/product/B01EJ1AFW8/)
// Downloaded on 8/29/18 from: https://github.com/MajicDesigns/MD_MAX72XX
#include <MD_MAX72xx.h>
#include <SPI.h>

// New update for scrolling text - old version was static
#include <MD_Parola.h>
// Example: https://github.com/MajicDesigns/MD_Parola/blob/master/examples/Parola_Scrolling_ESP8266/Parola_Scrolling_ESP8266.ino
// Master: https://github.com/MajicDesigns/MD_Parola



// Code from an example at: https://github.com/MajicDesigns/MD_MAX72XX/blob/master/examples/MD_MAX72xx_PrintText/MD_MAX72xx_PrintText.ino

// Global message buffers shared by Wifi and Scrolling functions
#define BUF_SIZE  512
char curMessage[BUF_SIZE];
char newMessage[BUF_SIZE];
bool newMessageAvailable = false;

// Uses arduino analog write which is 0-255 for intesity
// All dimming done in software, no inline resistor for this
const int RGBYellow[] = { 60, 60, 0 };
const int RGBOff[] = { 0, 0, 0 };
const int RGBGreen[] = { 0, 70, 0 };
const int RGBRed[] = { 80, 0, 0 };

// Define I/O pins based on hardware configuration
#define RED_PIN 3
#define GREEN_PIN 6
#define BLUE_PIN 5

#define INVERT_DUTY_CYCLE

// Define the number of devices we have in the chain and the hardware interface
// NOTE: These pin numbers will probably not work with your hardware and may
// need to be adapted
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW //PAROLA_HW
// Changed to FC16_HW for my amazondisplay

// #define MAX_DEVICES 8
#define MAX_DEVICES 4 // Changed from example

// Pinouts from here: https://learn.sparkfun.com/tutorials/esp8266-thing-hookup-guide/using-the-arduino-addon


//#define CLK_PIN   12 // or SCK
//#define DATA_PIN  11 // or MOSI
#define CS_PIN    10 // or SS


#define BRIGHTNESS 10


// WARNING - I connected these to the SCK/MOSI/SS pins, but really I needed HSCK/HMOSI/HSS pins!

// SPI hardware interface
// Removing instance and using MD_Parola
// MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
// Arbitrary pins
//MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

uint8_t frameDelay = 50;  // default frame delay value

// Hardware Setup
// Arduino Nano w/ ATmega328P

#define HIGH_LOW 5000
#define RELAY_PIN 2
#define ARM_PIN 18
#define FIRE_PIN 19


#define DEBOUNCE_TIME_MS 100


unsigned long lastFlashTime = 0;
unsigned long startupTime = 0;

boolean displayOn = true;
boolean flashing = false;
boolean flashingInAnimate = false;

boolean doDisarmed = true;

int flash_timeout_ms = 500;

int oldSecondTime = 1000;

enum controllerState
{
	Ready,
	ReadyToArmed,
	Armed,
	Disarming,
	Countdown,
	Fire
};

controllerState currentState = Ready;



// the setup function runs once when you press reset or power the board
void setup() 
{
	
	pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    
	pinMode(ARM_PIN, INPUT);           // set pin to input
	// digitalWrite(ARM_PIN, HIGH);       // turn on pullup resistors
    // ARM is high polarity

	pinMode(FIRE_PIN, INPUT);           // set pin to input
	digitalWrite(FIRE_PIN, HIGH);       // turn on pullup resistors

	// get the display up and running
	P.begin();
	P.displayClear();
	P.displaySuspend(false);
	P.setIntensity(BRIGHTNESS);
	P.displayScroll(curMessage, PA_CENTER, PA_CLOSING_CURSOR, frameDelay);


	// Do this the other way where I use the pre-existing global text buffer
	// Create our final formatted string
	String statusString = String("READY");
	statusString.toCharArray(curMessage, 512);    

	// Print our string to the serial terminal
	Serial.begin(57600);
	Serial.print("Local Time: ");
	Serial.println(statusString);

	//P.displayText(curMessage, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

	startupTime = millis();
	lastFlashTime = startupTime;
    
    // Let the startup animation finish before we jump into the loop - otherwise it interrupts the state machine
    boolean animationComplete = manageDisplay(startupTime);
    while (animationComplete)
    {
        animationComplete = manageDisplay(startupTime);
    }
    delay(1000);
    

	currentState = Ready;



}

// the loop function runs over and over again until power down or reset
void loop() 
{

	// http://gammon.com.au/millis   
	unsigned long currentTimer = millis();
	

	boolean toArmed = false;
	boolean toFire = false;

    // Handle the updating of the display - This will return true once a scheduled animation is complete
    boolean animationComplete = manageDisplay(currentTimer);

    // Declare variables for use inside the switch statement
    String statusString = String("");
    int secondTime = 0;


	// Handle the transition of the ARM pin
	// debounce for 10 ms
	if (digitalRead(ARM_PIN) == HIGH && currentState == Ready)
	{
		delay(DEBOUNCE_TIME_MS);

		if (digitalRead(ARM_PIN) == HIGH)
		{
			toArmed = true;
		}

	}

	// Handle the transition of the ARM pin
	// debounce for 100 ms
	if (digitalRead(FIRE_PIN) == LOW && currentState == Armed)
	{
		delay(DEBOUNCE_TIME_MS);

		if (digitalRead(FIRE_PIN) == LOW)
		{
			toFire = true;
		}

	}


    // Special case for disarming - this ignores the flow of the state machine as any disarming will break out of any step and reset the state machine

    // See if we're anywhere outside of ready - we're somewhere in the arming/firing process
    if (currentState != Ready && currentState != Disarming && animationComplete == false)
    {
        // If the pin floats low - that means we've been disarmed
        if (digitalRead(ARM_PIN) == LOW)
        {
            delay(DEBOUNCE_TIME_MS);

            if (digitalRead(ARM_PIN) == LOW)
            {
                LEDWrite(RGBYellow); // Y
                // After the debounce, scroll some text and then go into the disarming state
                statusString = String("");
                statusString.toCharArray(curMessage, 512);
                P.displayText(curMessage, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
                P.displayAnimate();
                statusString = String("DISARMED!");
                P.displayScroll(curMessage, PA_CENTER, PA_SCROLL_LEFT, frameDelay);
                statusString.toCharArray(curMessage, 512);
                flashingInAnimate = true;
                flashing = true;
                flash_timeout_ms = 500;
                currentState = Disarming;
                Serial.println("Going to Disarming");
                animationComplete = false;
                
            }
        }
    }

    
    




	// Timing state machine

	// TODO
	// 1 - Create state machine where we go from:
	// READY --> ARMED --> COUNTDOWN --> FIRE (Countdown)
	// Any change from ARMED 
	switch (currentState)
	{
	case Ready:
        LEDWrite(RGBGreen); // G

		if (toArmed)
		{
			currentState = ReadyToArmed;             
            statusString = String("");
			statusString.toCharArray(curMessage, 512);
            P.displayText(curMessage, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
            P.displayAnimate();
            statusString = String("ARMED"); 
            statusString.toCharArray(curMessage, 512);
			P.displayScroll(curMessage, PA_CENTER, PA_SCROLL_LEFT, frameDelay);
			flashingInAnimate = false;
			flashing = false;
            
            Serial.println("Transition to Ready to Armed");
            LEDWrite(RGBYellow); // Y
		}
		break;

	case ReadyToArmed:
		// if we're done animating, we can move to the next state
		if (animationComplete)
		{
			currentState = Armed;
            Serial.println("Going to Armed");
		}
		break;

	case Armed:
        

		if (toFire)
		{
			currentState = Countdown;
			startupTime = millis();
            Serial.println("Heading Into Fire");
            flashingInAnimate = true;
            flashing = true;
		}
        break;

	case Countdown:
        LEDWrite(RGBRed); // R
        
        // get the elapsed time in seconds
        secondTime = ((currentTimer - startupTime) / 1000);

        if (secondTime != oldSecondTime)
        {
            statusString = countdownTimerTenSecond(secondTime);
        }

        if (secondTime == 11)
        {
            currentState = Fire;
        }


        statusString.toCharArray(curMessage, 512);       
        P.displayText(curMessage, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

		break;

	case Fire:
        digitalWrite(RELAY_PIN, HIGH);
		break;

    case Disarming:
        // if we're done animating, we can move to the next state
        if (animationComplete)
        {
            currentState = Ready;
            Serial.println("Going to Ready");
            String statusString = String("READY");
            statusString.toCharArray(curMessage, 512);
            P.displayScroll(curMessage, PA_CENTER, PA_CLOSING_CURSOR, frameDelay);
            flashingInAnimate = false;
            flashing = false;
            digitalWrite(RELAY_PIN, LOW);
            
        }

        break;

	default:
		currentState = Ready;
        digitalWrite(RELAY_PIN, LOW);
		break;
	}

  
}


String countdownTimerTenSecond(int currentTime)
{
    String timerString = String("");
        
    // Perform the countdown timer - Ten minus the number of seconds elapsed since the start of the countdown
    switch (10 - currentTime)
    {
    case 10:
        timerString = String("TEN");
        flashing = true;
        flash_timeout_ms = 250;
        break;
    case 9:
        timerString = String("NINE");
        break;
    case 8:
        timerString = String("EIGHT");

        break;
    case 7:
        timerString = String("SEVEN");
        break;
    case 6:
        timerString = String("SIX");
        break;
    case 5:
        timerString = String("FIVE");        
        flash_timeout_ms = 125;
        break;
    case 4:
        timerString = String("FOUR");
        break;
    case 3:
        timerString = String("THREE");
        
        break;
    case 2:
        timerString = String("TWO");
        flash_timeout_ms = 100;
        break;
    case 1:
        timerString = String("ONE");
        break;
    case 0:
    default:
        timerString = String("FIRE!!");

    }

    return timerString;
}

boolean manageDisplay(unsigned long currentTimer)
{
	// This checks to see if we're done with our animation
	boolean animationDone = P.displayAnimate();

	if (animationDone)
	{
		// If we've completed animation go to a static text
		P.displayText(curMessage, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);


		// P.displayScroll(curMessage, PA_CENTER, PA_PRINT, frameDelay);
		// P.displayReset();  // Reset and display it again
	}

	// See if we're supposed to be flashing, and the animation is done, and we hit the timeout
	if ((currentTimer - lastFlashTime >= flash_timeout_ms) && flashing && (flashingInAnimate || animationDone))
	{
		lastFlashTime = currentTimer;

		if (displayOn)
		{
			displayOn = false;
			P.displayShutdown(true);
		}
		else
		{
			displayOn = true;
			P.displayShutdown(false);
		}

	}

	return animationDone;

}

void LEDWrite(const int colors[3])
{
#ifdef INVERT_DUTY_CYCLE
    analogWrite(RED_PIN, 255 - colors[0]);
    analogWrite(GREEN_PIN, 255 - colors[1]);
    analogWrite(BLUE_PIN, 255 - colors[2]);
#endif // INVERT_DUTY_CYCLE

    // Don't invert
#ifndef INVERT_DUTY_CYCLE
    analogWrite(RED_PIN, colors[0]);
    analogWrite(GREEN_PIN, colors[1]);
    analogWrite(BLUE_PIN, colors[2]);
#endif // !INVERT_DUTY_CYCLE

}
