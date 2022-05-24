#include "Tests.h"

bool TimeSeriesTests::TestConstruct()
{
	bool ret{ true };
	TimeSeries::TimeSeries<double, double> EmptySeries;
	TimeSeries::TimeSeries<double, double> InitListSeries{ {1,2,3,4,5}, {1,2,3,4,5} };
	const double pt[5] = { 1,2,3,4,5 }, pv[5] = { 1,2,3,4,5 };
	TimeSeries::TimeSeries<double, double> PtrSeries(5, pt, pv);
	return ret;
}

bool TimeSeriesTests::MonotonicTest()
{
	bool ret{ true };
	TimeSeries::TimeSeries<double, double> monotonic({1,2,3,4,5}, {1,2,3,4,5});
	ret &= !monotonic.Data_.IsMonotonic().has_value();
	TimeSeries::TimeSeries<double, double> nonmonotonic({ 2,1,3,4,5 }, { 1,2,3,4,5 });
	ret &= nonmonotonic.Data_.IsMonotonic().has_value();
	if (!ret)
		return ret;
	try
	{
		ret = false;
		nonmonotonic.Data_.Check();
	}
	catch (const TimeSeries::Exception&)
	{
		ret = true;
	}
	return ret;
}

bool TimeSeriesTests::GetPointsTest()
{
	bool ret{ true };
	TimeSeries::TimeSeries<double, double> series({ 1,2,3,4,5 }, { 1,2,3,4,5 });
	auto pr{ series.Data_.GetTimePoints(3.5) };
	return ret;
}

bool TimeSeriesTests::TestAll()
{
	bool ret{ true };
	ret &= Test(TestConstruct, "Construct");
	ret &= Test(MonotonicTest, "Monotonic");
	ret &= Test(GetPointsTest, "GetPoints");
	return ret;
}

bool TimeSeriesTests::Test(bool(*fnTest)(), std::string_view TestName)
{
	bool ret{ (fnTest)() };
	std::cout << TestName << " : " << (ret ? "Passed" : "Failed") << std::endl;
	return ret;
}
