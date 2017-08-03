#ifndef AnimatedCircularLED_H
#define AnimatedCircularLED_H

#include <Arduino.h> //It is very important to remember this! note that if you are using Arduino 1.0 IDE, change "WProgram.h" to "Arduino.h"
#include <CircularLED.h>

class AnimatedCircularLED {
public:
	AnimatedCircularLED(CircularLED *circularLed);
	~AnimatedCircularLED();
	void setAnimationStepTime(int timeStep);
	void setPercentage(float percent);
private:
	CircularLED *m_circularLED;
};

#endif