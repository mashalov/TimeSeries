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
	//options.SetTimeTolerance(0.005);
	options.SetMultiValuePoint(TimeSeries::MultiValuePointProcess::Avg);
	auto dense{ series.DenseOutput(-1.0, 6.0, 0.01, options) };
	dense.Data_.WriteCSV("tests/denseoutput.csv");
	dense.Data_.Compare(series.Data_, options);
	return ret;
}


bool TimeSeriesTests::CompareTest()
{
	bool ret{ true };
	TSD series1{ "tests/compare1.csv" };
	TSD series2{ "tests/compare2.csv" };
	TSO options;
	options.SetMultiValuePoint(TimeSeries::MultiValuePointProcess::Avg);
	auto cr1{ series1.Data_.Compare(series2.Data_, options) };
	auto cr2{ series2.Data_.Compare(series1.Data_, options) };
	return ret;
}

bool TimeSeriesTests::DifferenceTest()
{
	bool ret{ true };
	TSD series1{ "tests/compare1.csv" };
	TSD series2{ "tests/compare2.csv" };
	TSO options;
	options.SetMultiValuePoint(TimeSeries::MultiValuePointProcess::Avg);
	auto diff{ series1.Data_.Difference(series2.Data_, options) };
	diff.Compress(options);
	diff.WriteCSV("tests/diff.csv");
	return ret;
}

bool TimeSeriesTests::CompressTest()
{
	bool ret{ true };
	TSD series("tests/monotonic.csv");
	TSO options;
	options.SetMultiValuePoint(TimeSeries::MultiValuePointProcess::Avg);
	series.Data_.Compress(options);
	series.Data_.WriteCSV("tests/compressed.csv");
	return ret;
}

bool TimeSeriesTests::OverallTest()
{
	bool ret{ true };
	TSD series1("tests/monotonic.csv");
	TSD series1ref({1,3,3,5}, {1,3,4,6});
	TSO options;
	series1.Data_.Compress(options);
	ret &= Test(series1.Data_.Compare(series1ref.Data_, options).Idenctical(), "Monotonic compress");
	auto dense{ series1.DenseOutput(-1.0, 7.0, 0.01, options) };
	dense.Data_.Compress(options);
	TSD series2ref({ -1,3,3,7 }, { -1,3,4,8 });
	ret &= Test(dense.Data_.Compare(series2ref.Data_, options).Idenctical(), "Dense output compress");

	options.SetRange({1, 5.5});

	auto diff{ series2ref.Data_.Difference(series1.Data_, options) };
	diff.Compress(options);

	options.SetRange({});
	TSD series3ref({ -1, 7 }, { 0, 0 });
	ret &= Test(diff.Compare(series3ref.Data_, options).Idenctical(), "Diff compress");
	return ret;
}
 
bool TimeSeriesTests::TestAll()
{
	bool ret{ true };
	/*ret &= Test(TestConstruct, "Construct");
	ret &= Test(MonotonicTest, "Monotonic");
	ret &= Test(GetPointsTest, "GetPoints");
	ret &= Test(DenseOutputTest, "DenseOutput");
	ret &= Test(CompareTest, "Compare");
	ret &= Test(DifferenceTest, "Difference");
	ret &= Test(CompressTest, "Compress");*/
	ret &= Test(OverallTest, "Overall");
	return ret;
}

bool TimeSeriesTests::Test(bool result, std::string_view TestName)
{
	std::cout << TestName << " : " << (result ? "Passed" : "Failed") << std::endl;
	return result;
}

bool TimeSeriesTests::Test(bool(*fnTest)(), std::string_view TestName)
{
	return Test((fnTest)(), TestName);
}
