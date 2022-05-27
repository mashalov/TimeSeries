#pragma once
#include "TimeSeries.h"
#include <filesystem>

#ifndef TIMESERIES_TEST_PATH
#define TIMESERIES_TEST_PATH ""
#endif

namespace TimeSeriesTest
{
	class TimeSeriesTests
	{
	public:
		static bool TestAll();
		static bool TestConstruct();
		static bool MonotonicTest();
		static bool GetPointsTest();
		static bool DenseOutputTest();
		static bool CompareTest();
		static bool DifferenceTest();
		static bool CompressTest();
		static bool OverallTest();
		static bool Test(bool (*fnTest)(), const std::string_view TestName);
		static bool Test(bool result, const std::string_view TestName);
		static std::filesystem::path TestPath(const std::filesystem::path& path);
	};
}


