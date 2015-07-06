/*
* Touch Panel manager
*/

#pragma once

#include "config/stm32plus.h"
#include "config/display/tft.h"
#include "config/display/touch.h"
#include "config/stream.h"

using namespace stm32plus;
using namespace stm32plus::display;


//display objects recall
typedef Fsmc16BitAccessMode<FsmcBank1NorSram1> LcdAccessMode;
typedef ITDB02_Portrait_64K<LcdAccessMode> LcdPanel;

class TouchPanel : public ThreePointTouchScreenCalibrator::GuiCallback  {

protected:

	//variables to get from lcd object declared in Access class
	LcdPanel *lcd;
	Font *_font;

	// touch panel specific variables & objects
	Spi *_spi;
	ExtiPeripheralBase *_exti;

	PassThroughTouchScreenPostProcessor *_passThroughPostProcessor;
	PassThroughTouchScreenCalibration *_passThroughCalibration;
	AveragingTouchScreenPostProcessor *_averagingPostProcessor;

	TouchScreenCalibration *calibration;


public:

	volatile bool _clicked;
	GpioPinRef _penIrqPin;
	TouchScreen *panel;

	//constructor needs a reference to lcd and font objects
	TouchPanel(LcdPanel *tft, Font *font );

	void initialize();
	/*
	 * Display a hit point for the user to aim at
	 */

	void displayHitPoint(const Point& pt);

	/*
	 * This will be called back when the EXTI interrupt fires.
	 */

	void onTouchScreenReady();


	/*
	 * Display the prompt "Please tap the stylus on each red point"
	 */

	void displayPrompt(const char *text);

	/*
	 * Get the size of the panel
	 */

	Size getPanelSize() {
		return Size(lcd->getWidth(),lcd->getHeight());
	}

    void calibrate();

};
