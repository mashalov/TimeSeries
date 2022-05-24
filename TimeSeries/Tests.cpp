#include "Tests.h"

using TSD = typename TimeSeries::TimeSeries<double, double>;
using TSO = typename TimeSeries::Options<double, double>;

bool TimeSeriesTests::TestConstruct()
{
	bool ret{ true };
	TSD EmptySeries;
	TSD InitListSeries{ {1,2,3,4,5}, {1,2,3,4,5} };
	const double pt[5] = { 1,2,3,4,5 }, pv[5] = { 1,2,3,4,5 };
	TSD PtrSeries(5, pt, pv);
	TSD CSVSeries("tests/test1.csv");
	CSVSeries.Data_.Dump();
	return ret;
}

bool TimeSeriesTests::MonotonicTest()
{
	bool ret{ true };
	TSD monotonic("tests/monotonic.csv");
	ret &= !monotonic.Data_.IsMonotonic().has_value();
	TSD nonmonotonic("tests/nonmonotonic.csv");
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
	TSO options;
	options.SetTimeTolerance(0.05);

	TSD series("tests/monotonic.csv");
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

bool TimeSeriesTests::DenseOutputTest()
{
	bool ret{ true };
	TSD series("tests/monotonic.csv");
	TSO options;
	options.SetTimeTolerance(0.005);
	options.SetMultiValuePoint(TimeSeries::MultiValuePointProcess::All);
	auto dense{ series.DenseOutput(-1.0, 6.0, 0.01, options) };
	dense.Data_.WriteCSV("tests/denseoutput.csv");
	return ret;
}

 
bool TimeSeriesTests::TestAll()
{
	bool ret{ true };
	ret &= Test(TestConstruct, "Construct");
	ret &= Test(MonotonicTest, "Monotonic");
	ret &= Test(GetPointsTest, "GetPoints");
	ret &= Test(DenseOutputTest, "DenseOutput");
	return ret;
}


bool TimeSeriesTests::Test(bool(*fnTest)(), std::string_view TestName)
{
	bool ret{ (fnTest)() };
	std::cout << TestName << " : " << (ret ? "Passed" : "Failed") << std::endl;
	return ret;
}
