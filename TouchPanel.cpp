/**
 * Touch Panel manager
 */

#include "TouchPanel.h"


//constructor needs a reference to lcd and font objects
TouchPanel::TouchPanel(LcdPanel *tft, Font *font ){
	lcd = tft;
	_font = font;
}

void TouchPanel::initialize(){

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

	panel=new ADS7843AsyncTouchScreen(
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
	panel->setCalibration(*calibration);

	// start in accurate mode
	panel->setPostProcessor(*_averagingPostProcessor);

	// register as an observer for interrupts on the EXTI line

	panel->TouchScreenReadyEventSender.insertSubscriber(
			TouchScreenReadyEventSourceSlot::bind(this,&TouchPanel::onTouchScreenReady)
	);

}

/*
 * Display a hit point for the user to aim at
 */

void TouchPanel::displayHitPoint(const Point& pt) {

	int16_t i,j,x,y;

	x=pt.X-1;
	y=pt.Y-1;


	lcd->setForeground(ColourNames::RED);

	for(i=0;i<2;i++)
		for(j=0;j<2;j++)
			lcd->plotPoint(Point(x+j,y+i));
}

/*
 * This will be called back when the EXTI interrupt fires.
 */

void TouchPanel::onTouchScreenReady() {
	_clicked=true;
}


/*
 * Display the prompt "Please tap the stylus on each red point"
 */

void TouchPanel::displayPrompt(const char *text) {

	Size size;

	lcd->setBackground(ColourNames::BLACK);
	lcd->setForeground(ColourNames::WHITE);

	lcd->clearScreen();

	// show the prompt at the top center

	size=lcd->measureString(*_font,text);

	*lcd << Point((lcd->getWidth()/2)-(size.Width/2),0) << text;
}


void TouchPanel::calibrate() {

	ThreePointTouchScreenCalibrator calibrator(*panel,*this);
	TouchScreenCalibration* newResults;

	// important preparation for calibration: we must set the screen to pass through mode
	// so that the calibrator sees raw co-ordinates and not calibrated!

	panel->setCalibration(*_passThroughCalibration);

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

	panel->setCalibration(*calibration);

}
