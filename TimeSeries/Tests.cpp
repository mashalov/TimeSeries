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
	TimeSeries::Options<double, double> options;
	options.SetTimeTolerance(0.05);

	TimeSeries::TimeSeries<double, double> series({ 1,2,3,3,4,5 }, { 1,2,3,4,5,6 });
	auto start{ series.Data_.end() };

	for (double t = -1.0; t < 6.0; t += 0.01)
		auto pr{ series.Data_.GetTimePoints(t, options, start) };

	TimeSeries::TimeSeries<double, double> onepoint({ 1 }, { 1 });
	start = onepoint.Data_.end();

	for (double t = -1.0; t < 2.0 ; t += 1.0)
	{
		auto pr{ onepoint.Data_.GetTimePoints(t, options, start) };
		ret &= pr.size() == 1 && pr.front().v() == 1.0;
	}

	TimeSeries::TimeSeries<double, double> onet({ 1, 1 }, { 2,3 });
	start = onet.Data_.end();

	for (double t = -1.0; t < 3.0; t += 1.0)
		auto pr{ onet.Data_.GetTimePoints(t, options, start) };
	
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
