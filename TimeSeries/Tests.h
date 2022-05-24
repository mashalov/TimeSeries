#pragma once
#include <string>
#include "TimeSeries.h"


class TimeSeriesTests
{
public:
	static bool TestAll();
	static bool TestConstruct();
	static bool MonotonicTest();
	static bool GetPointsTest();
	static bool DenseOutputTest();
	static bool Test(bool (*fnTest)(), std::string_view TestName);
};


