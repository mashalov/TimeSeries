#pragma once
#include <vector>
#include <variant>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <locale>
#include "fmt/core.h"
#include "fmt/format.h"

namespace TimeSeriesTest
{
	class TimeSeriesTests;
};


namespace TimeSeries
{
	class Exception : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	
	template<typename T, typename V>
	class PointT
	{
	private:
		T t_ = {};
		V v_ = {};
	public:
		PointT() = default;
		PointT(const T& t, const V& v) : t_{ t }, v_{ v }{}
		inline const T& t() const { return t_; };
		inline const V& v() const { return v_;	}
		void t(const T& t) { t_ = t; }
		void v(const V& v) { v_ = v; }
	};

	template<typename T, typename V>
	struct TimeSeriesDataT : public std::vector<PointT<T, V>> {};

	template<typename T, typename V>
	class Interpolator
	{
		using DataT = typename TimeSeries::TimeSeriesDataT<T, V>;
	public:
		V Get(const DataT& Data, typename DataT::const_iterator& Place, const T& Time) const
		{
			const auto Interpolate = [](const T& Tl, const V& Vl, const T& Tr, const V& Vr, const T& Time) -> V
			{
				const auto div{ Tr - Tl };
				// if denominator is zero - choose points left
				// or right to Time given
				if (std::abs(div) > 0)
					return (Vr - Vl) / (Tr - Tl) * (Time - Tl) + Vl;
				else
					return Tl > Time ? Vl : Vr;
			};

			if (Place != Data.begin())
				Place--;

			auto NextPlace{ std::next(Place) };

			if (NextPlace != Data.end())	// got next point ? use current and next to interpolate
				return (Interpolate)(Place->t(), Place->v(), NextPlace->t(), NextPlace->v(), Time);
			else if (Place != Data.begin()) // no next point, use previous and current to interpolate
				{
					auto PrevPlace{ std::prev(Place) };
					return (Interpolate)(PrevPlace->t(), PrevPlace->v(), Place->t(), Place->v(), Time);
				}

			throw Exception(fmt::format("Interpolator::Get - Failed at t={}", Time));
		}
	};

	enum class MultiValuePointProcess
	{
		All,
		Max,
		Min,
		Avg
	};

	template<typename T, typename V>
	class Options
	{
	protected:
		T TimeTolerance_ = 1E-8;
		V ValueTolerance_ = 1E-8;
		V Atol_ = 1.0;
		V Rtol_ = 0.0;

		struct ProcessRange
		{
			std::optional<T> begin;
			std::optional<T> end;
		};

		ProcessRange Range_;

		MultiValuePointProcess MultiValuePointProcess_ = MultiValuePointProcess::All;
		
	public:
		inline T TimeTolerance() const { return TimeTolerance_; }
		void SetTimeTolerance(T TimeTolerance) { TimeTolerance_ = TimeTolerance; }
		inline V ValueTolerance() const { return ValueTolerance_; }
		void SetValueTolerance(V ValueTolerance) { ValueTolerance_ = ValueTolerance; }
		inline MultiValuePointProcess MultiValuePoint() const { return MultiValuePointProcess_; }
		void SetMultiValuePoint(MultiValuePointProcess MultiValuePoint) { MultiValuePointProcess_ = MultiValuePoint; }
		inline const ProcessRange& Range() const { return Range_; }
		void SetRange(const ProcessRange& Range) { Range_ = Range; }
		bool TimeInRange(const T& Time) const
		{
			if (Range_.begin.has_value() && Time < Range_.begin.value()) 
				return false;
			if (Range_.end.has_value() && Time >= Range_.end.value()) 
				return false;
			return true;
		}
		V Atol() const { return Atol_; }
		V Rtol() const { return Rtol_; }
		void SetAtol(const V& Atol) { Atol_ = Atol; }
		void SetRtol(const V& Rtol) { Rtol_ = Rtol; }
	};

	template <class charT, charT sep>
	class comma_facet : public std::numpunct<charT>
	{
	protected:
		charT do_decimal_point() const { return sep; }
	};



	template<typename T, typename V>
	class TimeSeriesData : protected TimeSeriesDataT<T, V>
	{
		friend class TimeSeriesTest::TimeSeriesTests;
		using fwitT = typename TimeSeriesData<T,V>::const_iterator;
		using pointT = typename TimeSeries::PointT<T, V>;
		using optionsT = typename TimeSeries::Options<T, V>;
		using NonMonotonicPairT = std::optional<std::pair<const pointT&, const pointT&>>;
		mutable bool Checked_ = false;
		void Check() const
		{
			if (Checked_)
				return;

			if (auto monotonic{ IsMonotonic() }; monotonic.has_value())
				throw Exception(fmt::format("TimeSeriesData::Check - time series is not monotonic : [{}] > [{}]",
					monotonic.value().first.t(),
					monotonic.value().second.t()));

			Checked_ = true;
		}

		std::pair<const T, const T> ToleranceRange(const T& Time, const T& HalfTolerance) const
		{
			return { Time - HalfTolerance, Time + HalfTolerance };
		}

		std::vector<T> UnionTime(const TimeSeriesData& ExtData, const optionsT& options) const
		{
			std::vector<T> uniontime;

			auto t1{ TimeSeriesData::begin() };
			auto t2{ ExtData.begin() };
			const auto& locoptions{ options };


			const auto StoreTime = [&uniontime, &locoptions](const T& Time) -> void
			{
				if(locoptions.TimeInRange(Time))
					if (uniontime.empty() || std::abs(uniontime.back() - Time) > locoptions.TimeTolerance() * 2.0)
						uniontime.emplace_back(Time);
			};

			while (1)
			{
				if (t1 != TimeSeriesData::end())
				{
					if (t2 != ExtData.end())
					{
						if (t1->t() < t2->t())
						{
							StoreTime(t1->t());
							t1++;
						}
						else
						{
							StoreTime(t2->t());
							t2++;
						}
					}
					else
					{
						StoreTime(t1->t());
						t1++;
					}
				}
				else if (t2 != ExtData.end())
				{
					StoreTime(t2->t());
					t2++;
				}
				else
					break;

			}
			return uniontime;
		}


		TimeSeriesData& Aggregate(const T& Time, const optionsT& options)
		{
			if (options.MultiValuePoint() == MultiValuePointProcess::All || TimeSeriesData::size() < 2)
				return *this;

			// if there are points and we must aggregate them - caclualte 
			// aggregate and replace points to single value

			double MultiValue{ 0 };

			for (auto TimePoint = TimeSeriesData::begin(); TimePoint != TimeSeriesData::end(); TimePoint++)
			{
				switch (options.MultiValuePoint())
				{
				case MultiValuePointProcess::Max:
					MultiValue = TimePoint == TimeSeriesData::begin() ? TimePoint->v() : (std::max)(TimePoint->v(), MultiValue);
					break;
				case MultiValuePointProcess::Min:
					MultiValue = TimePoint == TimeSeriesData::begin() ? TimePoint->v() : (std::min)(TimePoint->v(), MultiValue);
					break;
				case MultiValuePointProcess::Avg:
					MultiValue += TimePoint->v();
					break;
				}
			}

			const double PointsCount{ static_cast<const double>(TimeSeriesData::size()) };
			TimeSeriesData::clear();

			switch (options.MultiValuePoint())
			{
			case MultiValuePointProcess::Min:
			case MultiValuePointProcess::Max:
				TimeSeriesData::emplace_back(Time, MultiValue);
				break;
			case MultiValuePointProcess::Avg:
				TimeSeriesData::emplace_back(Time, MultiValue / PointsCount);
				break;
			}

			return *this;
		}
	public:

		static constexpr const char* TimeSeriesDoNotMatch = "Times and Values sizes do not match: Times {} and Values {}";
		TimeSeriesData() = default;

		TimeSeriesData(std::initializer_list<const T> Times, std::initializer_list<const V> Values)
		{
			if (Times.size() != Values.size())
				throw Exception(fmt::format(TimeSeriesDoNotMatch, Times.size(), Values.size()));

			auto v{ std::as_const(Values).begin() };
			for (const auto& t : Times)
				TimeSeriesData::emplace_back(t, *v++);
		}

		TimeSeriesData(size_t Size, const T* Times, const V* Values)
		{
			for (auto pT{ Times }; pT < Times + Size; pT++)
				TimeSeriesData::emplace_back(*pT, *Values++);
		}

		TimeSeriesData(const std::filesystem::path path)
		{
			std::ifstream csvfile(path);
			if (csvfile.is_open())
			{
				csvfile.imbue(std::locale(csvfile.getloc(), new comma_facet<char, ','>));
				double time{ 0 }, value{ 0 };
				char semicolon{ ';' };
				for (;;)
				{
					csvfile >> time >> semicolon >> value;
					if (csvfile.eof() || csvfile.fail())
						break;
					csvfile.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
					TimeSeriesData::emplace_back(time, value);
				}
			}
			else
				throw Exception(fmt::format("TimeSeries::TimeSeries - failed to open {}", path.string()));
		}

		class  CompareResult
		{
		protected:

			class MinMaxData : public pointT
			{
			protected:
				V v1_ = {};
				V v2_ = {};
			public:
				V v1() const { return v1_; }
				V v2() const { return v2_; }
				void v1(V v) { v1_ = v; }
				void v2(V v) { v2_ = v; }
			};
			MinMaxData Max_, Min_;
			V Sum_ = {};
			V SqSum_ = {};
			V Avg_ = {};
			bool Reset_ = true;
			bool Finished_ = false;
			size_t Count_ = 0;
			V KSDiffSum_ = {};	// Kolmogorov-Smirnov accumulator
			V KSDiff_ = {};		// Kolmogorov-Smirnov max difference

			inline static V WeightedDifference(const V& v1, const V& v2, const optionsT& options)
			{
				return std::abs((v1 - v2 / (options.Rtol() * std::abs(v1) + options.Atol())));
			}

		public:
			CompareResult()
			{
				Reset();
			}

			void Reset()
			{
				Count_ = {};
				Finished_ = false;
				Sum_ = {};
				KSDiffSum_ = {};
				KSDiff_ = {};
				Avg_ = {};
				SqSum_ = {};
			}

			void Update(const TimeSeriesData& series1, const TimeSeriesData& series2, const optionsT& options)
			{
				for (auto pt1{ series1.begin() }, pt2{ series2.begin() }; pt1 != series1.end() && pt2 != series2.end(); pt1++, pt2++)
				{
					const T diff{ pt1->v() - pt2->v() };

					if (Reset_)
					{
						Reset_ = false;

						Max_.t(pt1->t());
						Max_.v(CompareResult::WeightedDifference(pt1->v(), pt2->v(), options));
						Max_.v1(pt1->v());
						Max_.v2(pt2->v());

						Min_.t(pt1->t());
						Min_.v(CompareResult::WeightedDifference(pt1->v(), pt2->v(), options));
						Min_.v1(pt1->v());
						Min_.v2(pt2->v());

						KSDiffSum_ = diff;
						KSDiff_ = std::abs(diff);

					}
					else
					{
						const auto wd { WeightedDifference(pt1->v(), pt2->v(), options)};
						if (std::abs(Max_.v()) < wd)
						{
							Max_.t(pt1->t());
							Max_.v(wd);
							Max_.v1(pt1->v());
							Max_.v2(pt2->v());
						}

						if (std::abs(Min_.v()) > wd)
						{
							Min_.t(pt1->t());
							Min_.v(wd);
							Min_.v1(pt1->v());
							Min_.v2(pt2->v());
						}

						KSDiffSum_ += diff;
						const auto KSDiffSumAbs{ std::abs(KSDiffSum_) };
						if (KSDiffSumAbs > KSDiff_)
							KSDiff_ = KSDiffSumAbs;
					}

					Sum_ += diff;
					SqSum_ += diff * diff;
					Count_++;
				}
			}


			CompareResult& Finish()
			{
				if (!Finished_)
				{
					if (Count_ > 0)
						Avg_ = Sum_ / Count_;
					Finished_ = true;
				}

				return *this;
			}

			bool Idenctical(const T& Tolerance = {}) const
			{
				return Max_.v() <= Tolerance;
			}

			const V KSTest() const 
			{
				return KSDiff_;
			}

			const MinMaxData Max() const
			{
				return Max_;
			}

			const MinMaxData Min() const
			{
				return Min_;
			}

			const T Avg() const
			{
				return Avg_;
			}

			const T Sum() const
			{
				return Sum_;
			}

			const T SqSum() const
			{
				return SqSum_;
			}


		};

		NonMonotonicPairT IsMonotonic() const
		{
			// empty series is monotonic
			if (!TimeSeriesData::size())
				return {};

			auto TimePoint{ TimeSeriesData::begin() };
			auto PrevTime{ TimePoint };
			// check with no tolerance to
			// lower_ upper_bound work properly
			++TimePoint;
			for (; TimePoint != TimeSeriesData::end(); TimePoint++)
				if (PrevTime->t() > TimePoint->t())
					return { {*PrevTime, *TimePoint} };
				else
					PrevTime = TimePoint;

			return {};
		}

		void Dump() const
		{
			for (const auto& TimePoint : *this)
				std::cout << TimePoint.t() << ";" << TimePoint.v() << std::endl;
		}

		void WriteCSV(const std::filesystem::path& path)
		{
			std::ofstream csvfile(path);
			if (csvfile.is_open())
			{
				csvfile.imbue(std::locale(csvfile.getloc(), new comma_facet<char, ','>));
				for (const auto& TimePoint : *this)
					csvfile << TimePoint.t() << ";" << TimePoint.v() << std::endl;
			}
		}

		TimeSeriesData GetTimePoints(const T& Time, const optionsT& options) const
		{
			auto enddummy{ TimeSeriesData::end() };
			return GetTimePoints(Time, options, enddummy);
		}

		TimeSeriesData GetTimePoints(const T& Time, const optionsT& options, fwitT& Start) const
		{
			Check();

			TimeSeriesData retdata;

			if (TimeSeriesData::empty())
				return retdata;	// no output for empty series
			else if (TimeSeriesData::size() == 1)
			{
				// single point series outputs its point
				retdata.emplace_back(TimeSeriesData::front().t(), TimeSeriesData::front().v());
				return retdata;
			}

			auto start { Start == TimeSeriesData::end() ? TimeSeriesData::begin() : Start};
			const auto pred = [](const pointT& lhs, const pointT& rhs) -> bool
			{
				return lhs.t() < rhs.t();
			};

			// select range for the bound search to left and right from the point requested
			auto tolrange{ ToleranceRange(Time, options.TimeTolerance())};
			const auto leftpoint{ pointT(tolrange.first, {}) };
			const auto rightpoint{ pointT(tolrange.second, {}) };
			// get bounds
			auto left{ std::lower_bound(start, TimeSeriesData::end(), leftpoint, pred) };
			auto right{ std::upper_bound(start, TimeSeriesData::end(), rightpoint, pred) };

			// return iterator found for the bound to speedup next GetTimePoints call
			Start = left;

			// if there are points between bounds - send them to the output
			for (auto it = left; it != right; it++)
				if (it->t() >= tolrange.first && it->t() < tolrange.second)
					retdata.emplace_back(*it);

			// if no points around time requested - interpolate series
			// to time requested
			if (retdata.empty())
			{
				Interpolator<T, V> linear;
				retdata.emplace_back(Time, linear.Get(*this, left, Time));
			}
			else
				retdata.Aggregate(Time, options);

			/*
			// debug dump
			if (!retdata.empty())
			{
				std::cout << Time << " : ";
				for (const auto& TimePoint : retdata)
					std::cout << " { " << TimePoint.t() << " ; " << TimePoint.v() << " } ";
				std::cout << std::endl;
			}
			*/

			return { retdata };
		}

		TimeSeriesData Difference(const TimeSeriesData& ExtData, const optionsT& options) const
		{
			const auto uniontime{ UnionTime(ExtData, options) };
			TimeSeriesData ret;
			ret.reserve(uniontime.size());

			auto it1{ TimeSeriesData::end() };
			auto it2{ ExtData.end() };
			for (const auto& time : uniontime)
			{
				const auto series1{ GetTimePoints(time, options, it1) };
				const auto series2{ ExtData.GetTimePoints(time, options, it2) };

				for (auto pt1{ series1.begin() }, pt2{ series2.begin() }; pt1 != series1.end() && pt2 != series2.end(); pt1++, pt2++)
					ret.emplace_back(time, pt1->v() - pt2->v());

				/*if (op1.size() > 1 || op2.size() > 1)
					throw Exception(fmt::format("TimeSeriesData::Difference can be computed for single points only: got {} and {}", op1.size(), op2.size()));
					*/
				
			}
			return ret;
		}

		CompareResult Compare(const TimeSeriesData<T,V>& ExtData, const optionsT& options) const
		{
			CompareResult comps;
			auto it1{ TimeSeriesData::end() };
			auto it2{ ExtData.end() };
			for (const auto& time : UnionTime(ExtData, options))
				comps.Update(GetTimePoints(time, options, it1), ExtData.GetTimePoints(time, options, it2), options);
			return comps.Finish();
		}

		size_t Compress(const optionsT& options)
		{
			size_t originalsize{ TimeSeriesData::size() };
			TimeSeriesData compressed;
			compressed.reserve(originalsize);
			auto it{ TimeSeriesData::begin() };
			if (it != TimeSeriesData::end())
			{
				compressed.emplace_back(*it);
				it++;
				const auto tolt{ 2.0 * options.TimeTolerance() };
				for (; it != TimeSeriesData::end(); it++)
				{
					const auto prev{ compressed.back() };
					if (std::abs(prev.t() - it->t()) < tolt && std::abs(prev.v() - it->v()) < options.ValueTolerance())
						continue;

					const auto next{ std::next(it) };
					if (next != TimeSeriesData::end())
					{
						auto denom{ next->t() - prev.t() };
						if (std::abs(denom) > 0.0)
						{
							denom = (next->v() - prev.v()) / denom * (it->t() - prev.t()) + prev.v();
							if (std::abs(it->v() - denom) < options.ValueTolerance())
								continue;
						}
						else
							continue;
					}
					compressed.emplace_back(*it);
				}
			}
			TimeSeriesData::swap(compressed);
			return originalsize - TimeSeriesData::size();
		}

		TimeSeriesData DenseOutput(const T& Start, const T& End, const T& Step, const optionsT& options) const
		{
			TimeSeriesData dense;
			auto start{ TimeSeriesData::end() };
			ptrdiff_t ti{ 0 };
			for (; ; ti++)
			{
				const T t{ Start + static_cast<T>(ti) * Step };
				if (t > End)
					break;

				for (const auto& TimePoint : GetTimePoints(t, options, start))
					dense.emplace_back(t, TimePoint.v());
			}
			return dense;
		}
	};

	template<typename T, typename V>
	class TS : public TimeSeriesData<T, V>
	{
		friend class TimeSeriesTest::TimeSeriesTests;
	public:
		using TimeSeriesData<T, V>::TimeSeriesData;
	};
}