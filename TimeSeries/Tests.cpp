#include "Tests.h"

using namespace timeseries_test;

using TSD = typename timeseries::TimeSeries<double, double>;
using TSO = typename TSD::Options;

std::filesystem::path TimeSeriesTests::TestPath(const std::filesystem::path& testpath)
{
	return std::filesystem::path(TIMESERIES_TEST_PATH) /= testpath;
}

bool TimeSeriesTests::TestConstruct()
{
	bool ret{ true };
	TSD EmptySeries;
	TSD InitListSeries{ { 1,2,3,4,5 }, { 1,2,3,4,5 } };
	const double pt[5] = { 1,2,3,4,5 }, pv[5] = { 1,2,3,4,5 };
	TSD PtrSeries(5, pt, pv);
	TSD CSVSeries(TimeSeriesTests::TestPath("tests/test1.csv"));
	return ret;
}

bool TimeSeriesTests::MonotonicTest()
{
	bool ret{ true };
	TSD monotonic(TimeSeriesTests::TestPath("tests/monotonic.csv"));
	ret &= !monotonic.IsMonotonic().has_value();
	TSD nonmonotonic(TimeSeriesTests::TestPath("tests/nonmonotonic.csv"));
	ret &= nonmonotonic.IsMonotonic().has_value();
	if (!ret)
		return ret;
	try
	{
		ret = false;
		nonmonotonic.Check();
	}
	catch (const timeseries::Exception&)
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

	TSD series(TimeSeriesTests::TestPath("tests/monotonic.csv"));
	TSD::fwitT start{ series.end() };

for (double t = -1.0; t < 6.0; t += 0.01)
		auto pr{ series.GetTimePoints(t, options, start) };

	TSD onepoint({ 1 }, { 1 });
	start = onepoint.end();

	for (double t = -1.0; t < 2.0 ; t += 1.0)
	{
		auto pr{ onepoint.GetTimePoints(t, options, start) };
		ret &= pr.size() == 1 && pr.front().v() == 1.0;
	}

	TSD onet({ 1, 1 }, { 2,3 });
	start = onet.end();

	for (double t = -1.0; t < 3.0; t += 1.0)
		auto pr{ onet.GetTimePoints(t, options, start) };
	
	return ret;
}

bool TimeSeriesTests::DenseOutputTest()
{
	bool ret{ true };
	TSD series(TimeSeriesTests::TestPath("tests/monotonic.csv"));
	TSO options;
	//options.SetTimeTolerance(0.005);
	options.SetMultiValuePoint(timeseries::MultiValuePointProcess::Avg);
	auto dense{ series.DenseOutput(-1.0, 6.0, 0.01, options) };
	dense.WriteCSV(TimeSeriesTests::TestPath("tests/denseoutput.csv"));
	dense.Compare(series, options);
	return ret;
}


bool TimeSeriesTests::CompareTest()
{
	bool ret{ true };
	// Transient data
	TSD series1{ TimeSeriesTests::TestPath("tests/compare1.csv") };
	TSD series2{ TimeSeriesTests::TestPath("tests/compare2.csv") };
	TSO options;
	options.SetMultiValuePoint(timeseries::MultiValuePointProcess::Avg);
	auto cr1{ series1.Compare(series2, options) };
	auto cr2{ series2.Compare(series1, options) };
	Test(std::abs(cr1.Max().v() - cr2.Max().v()) < 1e-14 && 
		 std::abs(cr1.Max().t() - cr2.Max().t()) < 1e-14 && 
		 std::abs(cr1.KSTest()  - cr2.KSTest()) < 1e-14,
		 "Forward-reverse compare");

	// Kolmogorov-Smirnov test from example
	// https://www.researchgate.net/post/How_can_I_statistically_compare_two_curves_same_X_values_Different_Y_values_without_using_MATLAB_or_R
	TSD series3{ TimeSeriesTests::TestPath("tests/kstest1.csv") };
	TSD series4{ TimeSeriesTests::TestPath("tests/kstest2.csv") };
	Test(std::abs(series3.Compare(series4, options).Finish().KSTest() - 0.529978470995037) < 1e-14,
		"Kolmogorov-Smirnov test");


	return ret;
}

bool TimeSeriesTests::DifferenceTest()
{
	bool ret{ true };
	TSD series1{ TimeSeriesTests::TestPath("tests/compare1.csv") };
	TSD series2{ TimeSeriesTests::TestPath("tests/compare2.csv") };
	TSO options;
	//options.SetTimeTolerance(1e-6);
	//options.SetValueTolerance(1e-6);
	options.SetMultiValuePoint(timeseries::MultiValuePointProcess::Avg);
	auto diff{ series1.Difference(series2, options) };
	diff.Compress(options);
	diff.WriteCSV(TimeSeriesTests::TestPath("tests/diff.csv"));
	return ret;
}

bool TimeSeriesTests::CompressTest()
{
	bool ret{ true };
	TSD series(TimeSeriesTests::TestPath("tests/monotonic.csv"));
	TSO options;
	options.SetMultiValuePoint(timeseries::MultiValuePointProcess::Avg);
	series.Compress(options);
	series.WriteCSV(TimeSeriesTests::TestPath("tests/compressed.csv"));
	return ret;
}

bool TimeSeriesTests::OverallTest()
{
	bool ret{ true };
	TSD series1(TimeSeriesTests::TestPath("tests/monotonic.csv"));
	TSD series1ref({1,3,3,5}, {1,3,4,6});
	TSO options;
	series1.Compress(options);
	ret &= Test(series1.Compare(series1ref, options).Idenctical(), "Monotonic compress");
	auto dense{ series1.DenseOutput(-1.0, 7.0, 0.01, options) };
	dense.Compress(options);
	TSD series2ref({ -1,3,3,7 }, { -1,3,4,8 });
	ret &= Test(dense.Compare(series2ref, options).Idenctical(), "Dense output compress");

	options.SetRange({1, 5.5});

	auto diff{ series2ref.Difference(series1, options) };
	diff.Compress(options);

	options.SetRange({});
	TSD series3ref({ -1, 7 }, { 0, 0 });
	ret &= Test(diff.Compare(series3ref, options).Idenctical(), "Diff compress");
	return ret;
}
 
bool TimeSeriesTests::TestAll()
{
	bool ret{ true };
	ret &= Test(TestConstruct, "Construct");
	ret &= Test(MonotonicTest, "Monotonic");
	ret &= Test(GetPointsTest, "GetPoints");
	ret &= Test(DenseOutputTest, "DenseOutput");
	ret &= Test(CompareTest, "Compare");
	ret &= Test(DifferenceTest, "Difference");
	ret &= Test(CompressTest, "Compress");
	ret &= Test(OverallTest, "Overall");
	return ret;
}

bool TimeSeriesTests::Test(bool result, const std::string_view TestName)
{
	std::cout << (result ? "Passed" : "!!! Failed !!!") << " : " << TestName << std::endl;
	return result;
}

bool TimeSeriesTests::Test(bool(*fnTest)(), const std::string_view TestName)
{
	return Test((fnTest)(), TestName);
}
