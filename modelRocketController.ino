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

uint8_t frameDelay = 65;  // default frame delay value
textEffect_t	scrollEffect = PA_SCROLL_LEFT;

// Hardware Setup
// Arduino Nano w/ ATmega328P

#define HIGH_LOW 5000
#define RELAY_PIN 2

#define FLASH_TIMEOUT_MS 500

#define ARMED_TIMEOUT_MS 10000

unsigned long lastFlashTime = 0;
unsigned long startupTime = 0;

boolean displayOn = true;
boolean flashing = false;
boolean animateDone = false;

void manageDisplay(unsigned long currentTimer)
{
    // This checks to see if we're done with our animation
    if (P.displayAnimate()) 
    {
        // If we've completed animation go to a static text
        P.displayText(curMessage, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
        animateDone = true;
        lastFlashTime = currentTimer;
        // P.displayScroll(curMessage, PA_CENTER, PA_PRINT, frameDelay);
        // P.displayReset();  // Reset and display it again
    }

    // See if we're supposed to be flashing, and the animation is done, and we hit the timeout
    if ((currentTimer - lastFlashTime >= FLASH_TIMEOUT_MS) && flashing && animateDone)
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
    
}



// the setup function runs once when you press reset or power the board
void setup() 
{
    
    pinMode(RELAY_PIN, OUTPUT);

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

}

// the loop function runs over and over again until power down or reset
void loop() 
{


    // http://gammon.com.au/millis   
    unsigned long currentTimer = millis();

    manageDisplay(currentTimer);

    // Timing state machine

    // TODO
    // 1 - Create state machine where we go from:
    // READY --> ARMED --> COUNTDOWN --> FIRE (Countdown)
    // Any change from ARMED 

    // THIS DOESN'T WORK AT ALL!
    if (currentTimer - startupTime >= ARMED_TIMEOUT_MS)
    {

        String statusString = String("DISARM");
        statusString.toCharArray(curMessage, 512);
        P.displayScroll(curMessage, PA_CENTER, PA_CLOSING_CURSOR, frameDelay);
        flashing = true;
        animateDone = false;

    }


    //digitalWrite(LED_BUILTIN, HIGH);
    //digitalWrite(RELAY_PIN, HIGH);
    //delay(HIGH_LOW);
    //digitalWrite(LED_BUILTIN, LOW);
    //digitalWrite(RELAY_PIN, LOW);
    //delay(HIGH_LOW);

  
}
