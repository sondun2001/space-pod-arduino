#include "AnimatedCircularLED.h" //include the declaration for this class
#include <CircularLED.h>

CircularLED circularLED(8, 7);

unsigned int m_ledStatusArray[24];
unsigned int m_currentLastLED;
unsigned long m_lastLEDDisplayTime;

//<<constructor>> setup the LED, make pin 13 an OUTPUT
AnimatedCircularLED::AnimatedCircularLED() {
	circularLED.CircularLEDWrite(m_ledStatusArray);
}

//<<destructor>>
AnimatedCircularLED::~AnimatedCircularLED(){/*nothing to destruct*/ }

//turn the LED on
void AnimatedCircularLED::setPercentage(float percent) {
	unsigned int lastLED = (int)(24 * percent);
	unsigned long currentTime = millis();
	if (m_currentLastLED != lastLED && currentTime > m_lastLEDDisplayTime + 30) {
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

		circularLED.CircularLEDWrite(m_ledStatusArray);
		m_lastLEDDisplayTime = currentTime;
	}
}