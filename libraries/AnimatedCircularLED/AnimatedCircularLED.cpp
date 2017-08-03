#include "AnimatedCircularLED.h" //include the declaration for this class

unsigned int m_ledStatusArray[24];
unsigned int m_currentLastLED;
unsigned long m_lastLEDDisplayTime;
unsigned int m_timeStep;

//<<constructor>> setup the LED, make pin 13 an OUTPUT
AnimatedCircularLED::AnimatedCircularLED(CircularLED *circularLed) {
	m_circularLED = circularLed;
	m_circularLED->CircularLEDWrite(m_ledStatusArray);
}

//<<destructor>>
AnimatedCircularLED::~AnimatedCircularLED(){/*nothing to destruct*/ }

void AnimatedCircularLED::setAnimationStepTime(int timeStep) {
	m_timeStep = timeStep;
}

//turn the LED on
void AnimatedCircularLED::setPercentage(float percent) {
	unsigned int lastLED = (int)(24 * percent);
	unsigned long currentTime = millis();
	if (m_currentLastLED != lastLED && currentTime > m_lastLEDDisplayTime + m_timeStep) {
		if (lastLED > m_currentLastLED) {
			for (int i = 0; i < 24; i++)
			{
				unsigned int ledOnOff = (i < lastLED) ? 0x88 : 0;
				if (m_ledStatusArray[i] != ledOnOff) {
					m_ledStatusArray[i] = ledOnOff;
					m_currentLastLED = i;
					break;
				}
			}
		}
		else
		{
			for (int i = 23; i >= 0; i--)
			{
				unsigned int ledOnOff = (i < lastLED) ? 0x88 : 0;
				if (m_ledStatusArray[i] != ledOnOff) {
					m_ledStatusArray[i] = ledOnOff;
					m_currentLastLED = i;
					break;
				}
			}
		}

		m_circularLED->CircularLEDWrite(m_ledStatusArray);
		m_lastLEDDisplayTime = currentTime;
	}
}