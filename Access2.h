#pragma once

#include "config/stm32plus.h"
#include "config/display/tft.h"
#include "config/display/touch.h"
#include "config/usart.h"


using namespace stm32plus;

using namespace stm32plus::display;


//display objects
typedef Fsmc16BitAccessMode<FsmcBank1NorSram1> LcdAccessMode;
typedef ITDB02_Portrait_64K<LcdAccessMode> LcdPanel;
