#pragma once
#include <cmath>

class FeedbackFilter
{
public:
    void prepare(double /*sampleRate*/) {}
    void reset() {}
    void setLPFrequency(float /*freqHz*/) {}
    void setHPFrequency(float /*freqHz*/) {}
    void setLPBypassed(bool /*bypassed*/) {}
    void setHPBypassed(bool /*bypassed*/) {}
    float process(int /*channel*/, float input) { return input; }
};
