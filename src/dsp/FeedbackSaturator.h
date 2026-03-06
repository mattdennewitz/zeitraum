#pragma once
#include <cmath>

class FeedbackSaturator
{
public:
    void prepare(double /*sampleRate*/) {}
    void reset() {}
    void updateRms(float /*inputL*/, float /*inputR*/) {}
    float process(float input) { return input; }
};
