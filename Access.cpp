/*
 * Copyright (c) Jaafar Ben-Abdallah June-July-2015
 *
 * This is basically a main menu page with buttons to tap to start a specific app
 * A terminal (text display) area is also available in the bottom of the screen
 * for now only portrait orientation is supported (future extension will use autorotate)
 * all subsequent apps will be declared here, and a specific button will be added
 *
 * Tested on devices:
 *   STM32F4 discovery board
 *
 */

#include "Access.h"

/**
 * ui hardware: ITDB02-3.2WD lcd: 3.2", 400x240 with HX8352A controller and XPT2046 touch controller
 *	--> http://wiki.iteadstudio.com/ITDB02-3.2WD
 */

class Access {
protected:



	//global variables for display
	LcdAccessMode *_accessMode;
	LcdPanel *lcd;
	Font *_font;
	DefaultBacklight *backlight;	// create the backlight PWM output on timer4, channel2 (PD13)

	//global variable for touch
	TouchPanel *touch = new TouchPanel(lcd,_font);



	//global variables for menu UI
	LcdPanel::tCOLOUR fgcolor,bgcolor;
	Rectangle calibrate_box,ATcommands;

	Point hitPoint;
	//Size boxSize;



public:

	void initDisplay(){

		// we've got RESET on PE1, backlight on PD13 and RS (D/CX) on PD11
		GpioE<DefaultDigitalOutputFeature<1>> pe;
		GpioD<DefaultDigitalOutputFeature<13>,DefaultFsmcAlternateFunctionFeature<11>> pd;

		// set up the FSMC timing for this panel
		Fsmc8080LcdTiming fsmcTiming(2,5);

		// set up the FSMC on bank 1 with A16 as the RS line (PD11)
		_accessMode=new LcdAccessMode(fsmcTiming,16,pe[1]);

		// create the LCD interface in landscape mode
		// this will power it up and do the reset sequence
		lcd=new LcdPanel(*_accessMode);

		HX8352AGamma gamma(0xA0,0x03,0x00,0x45,0x03,0x47,0x23,0x77,0x01,0x1F,0x0F,0x03);
		// jb settings: check which one gives better colors
		//HX8352AGamma gamma(0xF0,0x07,0x00,0x43,0x16,0x16,0x43,0x77,0x00,0x1E,0x0F,0x00);
		lcd->applyGamma(gamma);

		// create a font and select it for stream operations
		_font=new Font_NINTENDO_DS_BIOS16();
		*lcd << *_font;
		lcd->setFontFilledBackground(true);

		// turn on the backlight at 100%
		backlight=new DefaultBacklight;

		//uint16_t backlightPercentage=100;
		backlight->fadeTo(100,4);

		// setup font/background colors
		bgcolor = ColourNames::BLACK;
		fgcolor = ColourNames::YELLOW;

		lcd->setBackground(bgcolor);
		lcd->setForeground(fgcolor);

	}

	/**
	       * Display a touch button
	       * @param[in] label : button label
	       * @param[in] location : button top  left location
	       * @param[in] Rectangle : touchArea and object to be associated with this button
	       */


	void drawBox(const char *label, int16_t X_location,int16_t Y_location , Rectangle* touchArea) {
		// create a touch button at location specified by location
		// and display the button on the screen with the specified label
		// touch button are constant size for now
		touchArea->X=X_location;
		touchArea->Y=Y_location;
		touchArea->Height=40;
		touchArea->Width=80;

		lcd->drawRectangle(*touchArea);
		// find the suitable location for the label and display it
		Size stringSize = lcd->measureString(*_font,label);
		Point labelLocation;
		labelLocation.X = touchArea->Width/2-stringSize.Width/2+X_location;
		labelLocation.Y = touchArea->Height/2-stringSize.Height/2+Y_location;
		// debug purpose: we want to make sure the location is still visible
		// even if it's too big for the box
		if (labelLocation.X > 0 && labelLocation.Y>0)
			*lcd << labelLocation << label;

	}


	void drawMenu(){

		//calibrate box
		drawBox("Calibrate",20,30,&calibrate_box);

		//AT commands box
		drawBox("ESP8266",140,30,&ATcommands);

	}


	void processEvents() {

		//event 1: click on the touch panel

		// check if there's a click
		if(touch->_clicked){

			// a click happened!
			do {
				touch->panel->getCoordinates(hitPoint);

				// event: calibrate button tap
				if(calibrate_box.containsPoint(hitPoint)){
					MillisecondTimer::delay(500);
					touch->calibrate();
				}
				// event: send AT commands button tap
				else if (ATcommands.containsPoint(hitPoint)){
					// NEXT STEP: start usart, send AT+RST COMMAND AND GET THE ANSWER
					//debug purpose
					*lcd << Point(20,160);
					*lcd << "sending reset commands";
					MillisecondTimer::delay(1000);
					//

				}

			} while(!touch->_penIrqPin.read());

			//reset clicked flag
			touch->_clicked=false;

			// back to main menu
			lcd->clearScreen();
			lcd->setForeground(fgcolor);
			drawMenu();

		}
	}

	void run() {

		initDisplay();

		touch->initialize();

		drawMenu();

		for(;;){
			processEvents();
		}
	}
};


/*
 * Main entry point
 */

int main() {

	// set up SysTick at 1ms resolution
	MillisecondTimer::initialise();
	Access mainpage;
	mainpage.run();

	// not reached
	return 0;
}
