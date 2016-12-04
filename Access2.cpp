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
 *   Dec 2016: code updated for compatibility with stm32plus 4.0.5
 *
 */

#include "Access2.h"

/**
 * ui hardware: ITDB02-3.2WD lcd: 3.2", 400x240 with HX8352A controller and XPT2046 touch controller
 *	--> http://wiki.iteadstudio.com/ITDB02-3.2WD
 */

class Access2 : public ThreePointTouchScreenCalibrator::GuiCallback {

protected:

	//global variables for display
	LcdAccessMode *_accessMode;
	LcdPanel *_lcd;
	Font *_font;
	DefaultBacklight *_backlight;	// create the backlight PWM output on timer4, channel2 (PD13)

	//global variable for touch
	Spi *_spi;
	ExtiPeripheralBase *_exti;
	ADS7843AsyncTouchScreen *touchPanel;  // the touch screen object                            // The SPI peripheral
	GpioPinRef _penIrqPin;                  // The GPIO pin that receives the pin IRQ

	PassThroughTouchScreenPostProcessor *_passThroughPostProcessor;
	PassThroughTouchScreenCalibration *_passThroughCalibration;
	AveragingTouchScreenPostProcessor *_averagingPostProcessor;

	TouchScreenCalibration *calibration;
    // the observer implementation will set this when the interrupt fires
    volatile bool clicked = false;

	// global variables for menu UI
    LcdPanel::tCOLOUR fgcolor,bgcolor;
    //Rectangle calibrate_box;
	Rectangle ResetESP8266;
	Rectangle ConnectionStatus;
	Point hitPoint;
	uint16_t _backlightPercentage;


public:

	// USART 2 with default pin assignments
	// pin mapping: (TX,RX,RTS,CTS,CK) = (PA2,PA3,PA1,PA0,PA4)
	//Usart2<> serial(57600);
	Usart2<> *serial;
	// We'll use streams to send and receive the data
//	UsartPollingOutputStream outputStream(serial);
	UsartPollingOutputStream *outputStream;
//	UsartPollingInputStream inputStream(serial);
	UsartPollingInputStream *inputStream;
	// reading buffer and counter
	uint8_t readBuffer[64];
	uint32_t received_cnt;

	void initDisplay(){

		//LCD_RESET on PE1, LCD_backlight on PD13 and LCD_RS (D/CX) on PD11
		GpioE<DefaultDigitalOutputFeature<1>> pe;
		GpioD<DefaultDigitalOutputFeature<13>,DefaultFsmcAlternateFunctionFeature<11>> pd;

		//Fsmc8080LcdTiming fsmcTiming(1,15);//FSMC timing for this panel
		Fsmc8080LcdTiming fsmcTiming(1,15);

		_accessMode = new LcdAccessMode(fsmcTiming,16,pe[1]);//FSMC on bank 1 with A16 as the RS line

		_lcd = new LcdPanel(*_accessMode);//declare and init the screen

		// set up some gamma values for this panel
		HX8352AGamma gamma(0xF0,0x07,0x00,0x43,0x16,0x16,0x43,0x77,0x00,0x1E,0x0F,0x00);
		_lcd->applyGamma(gamma);

		// create a font and select it for stream operations
		_font=new Font_KYROU_9_REGULAR8();
		*_lcd << *_font;
		_lcd->setFontFilledBackground(false);

		// turn on the backlight at 100%
		_backlight = new DefaultBacklight;
		_backlightPercentage = 100;
		_backlight->fadeTo(_backlightPercentage,5);//in 5ms steps

		// setup font/background colors
		bgcolor = ColourNames::BLACK;
		fgcolor = ColourNames::YELLOW;

		_lcd->setBackground(bgcolor);
		_lcd->setForeground(fgcolor);

	}

	void initTouchPanel(){

		// create the initial pass through calibration object that allows us to create
		// the touch screen object ready for calibrating for real
		_passThroughCalibration=new PassThroughTouchScreenCalibration;

		// create an averaging post-processor for use in accurate mode that
		// does 4x oversampling on the incoming data
		_averagingPostProcessor=new AveragingTouchScreenPostProcessor(4);

		// create the do-nothing post-processor that is used in non-accurate mode

		_passThroughPostProcessor=new PassThroughTouchScreenPostProcessor();

		// we've got the PENIRQ attached to GPIOC, port 5.
		// it's active low we want to be called back via our Observer implementation when the
		// signal falls from high to low.

		GpioC<DefaultDigitalInputFeature<5> > pc;
		_penIrqPin=pc[5];

		_exti=new Exti5(EXTI_Mode_Interrupt,EXTI_Trigger_Falling,pc[5]);

		// we've got the SPI interface to the touchscreen wired to SPI1, and since SPI1 is the fast one on the
		// STM32 we'll divide the 72Mhz clock by the maximum of 256 instead of 128 which we'd use on SPI2.

		//Spi1_Remap1<>::Parameters params;
		Spi1<>::Parameters params;
		//params.spi_baudRatePrescaler=SPI_BaudRatePrescaler_256;
		//params.spi_cpol=SPI_CPOL_Low;
		//params.spi_cpha=SPI_CPHA_1Edge;

		_spi=new Spi1<>(params);

		// now create the touch screen, initially in non-accurate mode with some dummy calibration data because the first thing
		// we're going to do in the demo is calibrate it with the 3-point routine.

		touchPanel=new ADS7843AsyncTouchScreen(
				*_passThroughCalibration,
				*_passThroughPostProcessor,
				*_spi,
				_penIrqPin,
				*_exti
		);

		const uint32_t cr[]={0x3a902de0,0xbd872b02,0x437b0000,0xbde147ae,0xba9d4952,0x43d40000};
		// input stream wrapping to be fed to the deserializer (didn't find a better wrapper)
		ByteArrayInputStream *is = new ByteArrayInputStream(cr,sizeof(cr));
		// create a new empty three points calibration result
		calibration = new ThreePointTouchScreenCalibration();
		// load the calibration results
		calibration->deserialise(*is);
		touchPanel->setCalibration(*calibration);

		// start in accurate mode
		touchPanel->setPostProcessor(*_averagingPostProcessor);

		// register as an observer for interrupts on the EXTI line
		touchPanel->TouchScreenReadyEventSender.insertSubscriber(
				TouchScreenReadyEventSourceSlot::bind(this,&Access2::onTouchScreenReady)
		);

	}

	/*
	 * Display a hit point for the user to aim at
	 */

	void displayHitPoint(const Point& pt) {

		int16_t i,j,x,y;

		x=pt.X-1;
		y=pt.Y-1;


		_lcd->setForeground(ColourNames::RED);

		for(i=0;i<2;i++)
			for(j=0;j<2;j++)
				_lcd->plotPoint(Point(x+j,y+i));
	}

	/*
	 * This will be called back when the EXTI interrupt fires.
	 */
	void onTouchScreenReady() {
		clicked=true;
	}


	/*
	 * Display the prompt "Please tap the stylus on each red point"
	 */

	void displayPrompt(const char *text) {

		Size size;

		_lcd->setBackground(ColourNames::BLACK);
		_lcd->setForeground(ColourNames::WHITE);

		_lcd->clearScreen();

		// show the prompt at the top center

		size=_lcd->measureString(*_font,text);

		*_lcd << Point((_lcd->getWidth()/2)-(size.Width/2),0) << text;
	}


	void calibrate() {

		ThreePointTouchScreenCalibrator calibrator(*touchPanel,*this);
		TouchScreenCalibration* newResults;

		// important preparation for calibration: we must set the screen to pass through mode
		// so that the calibrator sees raw co-ordinates and not calibrated!

		touchPanel->setCalibration(*_passThroughCalibration);

		// calibrate the screen and get the new results. A real application can use the serialise
		// and deserialise methods of the TouchScreenCalibration base class to read/write the
		// calibration data to a persistent stream

		if(!calibrator.calibrate(newResults))
			return;

		// store the new results

		if(calibration!=NULL)
			delete calibration;

		calibration=newResults;

		// re-initialise the touch screen with the calibration data

		touchPanel->setCalibration(*calibration);

	}

    /*
     * Get the size of the panel
     */

    virtual Size getPanelSize() {
      return Size(_lcd->getWidth(),_lcd->getHeight());
    }


    void drawBox(const char *label, int16_t X_location,int16_t Y_location , Rectangle* touchArea) {
    	// create a touch button at location specified by location
    	// and display the button on the screen with the specified label
    	// touch button are constant size for now
    	touchArea->X=X_location;
    	touchArea->Y=Y_location;
    	touchArea->Height=40;
    	touchArea->Width=80;

    	_lcd->drawRectangle(*touchArea);
    	// find the suitable location for the label and display it
    	Size stringSize = _lcd->measureString(*_font,label);
    	Point labelLocation;
    	labelLocation.X = touchArea->Width/2-stringSize.Width/2+X_location;
    	labelLocation.Y = touchArea->Height/2-stringSize.Height/2+Y_location;
    	// debug purpose: we want to make sure the location is still visible
    	// even if it's too big for the box
    	if (labelLocation.X > 0 && labelLocation.Y>0)
    		*_lcd << labelLocation << label;

    }


	void drawMenu(){
		//calibrate box
		//drawBox("Calibrate",140,100,&calibrate_box);
		//Reset command box
		drawBox("Reset",20,30,&ResetESP8266);
		//Display wifi connection status command box
		drawBox("WiFi status",20,100,&ConnectionStatus);

	}

	void processEvents() {

		//event 1: click on the touch panel

		// check if there's a click
		if(clicked){

			// a click happened!
			do {
				touchPanel->getCoordinates(hitPoint);
				if(ResetESP8266.containsPoint(hitPoint)){
					//debug purpose
					*_lcd << Point(20,160);
					*_lcd << "sending reset command";
	    			// send reset AT command
	    			const char *at_rst="AT+RST";
	    			outputStream->write(at_rst,strlen(at_rst));
	    			// get the response
	    			MillisecondTimer::delay(10);
	    			inputStream->read(readBuffer, sizeof(readBuffer),received_cnt);
					*_lcd << Point(20,200);
					//*_lcd << readBuffer;
					//
					MillisecondTimer::delay(1000);
				}
				else if(ConnectionStatus.containsPoint(hitPoint)){
					//debug purpose
					*_lcd << Point(20,160);
					*_lcd << "checking WiFi connection status...";
					//
					MillisecondTimer::delay(1000);
				}

//				if(calibrate_box.containsPoint(hitPoint)){
//					//debug purpose
//					*_lcd << Point(20,0);
//					*_lcd << "correct touch";
//					//
//					MillisecondTimer::delay(1000);
//					calibrate();
//				}


			} while(!_penIrqPin.read());
			clicked=false;

			// back to main menu
			_lcd->clearScreen();
			_lcd->setForeground(fgcolor);
			drawMenu();

		}
		// event 2: on/off toggle

		// event 3: data available on usart (future dev)
	}

	void run() {

		//setup section

		initDisplay();

		initTouchPanel();

		drawMenu();

		serial = new Usart2<>(57600);
		outputStream = new UsartPollingOutputStream(*serial);
		inputStream = new UsartPollingInputStream(*serial);

		//loop section

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
	Access2 mainpage;
	mainpage.run();

	// not reached
	return 0;
}
