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

class TimeSeriesTests;

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
		using DataT = typename TimeSeriesDataT<T, V>;
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
		MultiValuePointProcess MultiValuePointProcess_ = MultiValuePointProcess::All;
		
	public:
		inline T TimeTolerance() const { return TimeTolerance_; }
		void SetTimeTolerance(T TimeTolerance) { TimeTolerance_ = TimeTolerance; }
		inline V ValueTolerance() const { return ValueTolerance_; }
		void SetValueTolerance(V ValueTolerance) { ValueTolerance_ = ValueTolerance; }
		inline MultiValuePointProcess MultiValuePoint() const { return MultiValuePointProcess_; }
		void SetMultiValuePoint(MultiValuePointProcess MultiValuePoint) { MultiValuePointProcess_ = MultiValuePoint; }
	};

	template <class charT, charT sep>
	class comma_facet : public std::numpunct<charT>
	{
	protected:
		charT do_decimal_point() const { return sep; }
	};



	template<typename T, typename V>
	class TimeSeriesData : public TimeSeriesDataT<T, V>
	{
		friend class TimeSeriesTests;
		using fwitT = typename TimeSeriesData<T,V>::const_iterator;
		using pointT = typename PointT<T, V>;
		using optionsT = typename Options<T, V>;
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
			const auto tol{ options.TimeTolerance() * 2.0 };

			const auto StoreTime = [&uniontime, &tol](const T& Time) -> void
			{
				if (uniontime.empty() || std::abs(uniontime.back() - Time) > tol * 2.0)
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
					MultiValue = TimePoint == TimeSeriesData::begin() ? TimePoint->v() : std::max(TimePoint->v(), MultiValue);
					break;
				case MultiValuePointProcess::Min:
					MultiValue = TimePoint == TimeSeriesData::begin() ? TimePoint->v() : std::min(TimePoint->v(), MultiValue);
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

		// Kolmogorov-Smirnov
		// Chi - squared
		class  CompareResult
		{
		protected:
			pointT Max, Min;
			V Sum = {};
			V SqSum = {};
			V Avg = {};
			bool Reset_ = true;
			bool Finished_ = false;
			size_t Count_ = 0;

		public:
			CompareResult()
			{
				Reset();
			}

			void Reset()
			{
				Count_ = 0;
				Finished_ = false;
			}

			void Update(const TimeSeriesData<T, V>& p1, const TimeSeriesData<T, V>& p2)
			{
				if (p1.size() == 1 && p2.size() == 1)
				{
					const auto& pt1{ p1.front() };
					const auto& pt2{ p2.front() };
					const V diff{ std::abs(pt1.v() - pt2.v()) };

					if (Reset_)
					{
						Reset_ = false;
						Max = Min = {};
					}
					else
					{
						const auto absdiff{ std::abs(diff) };
						if (std::abs(Max.v()) < absdiff)
						{
							Max.t(pt1.t());
							Max.v(absdiff);
						}

						if (std::abs(Min.v()) > absdiff)
						{
							Min.t(pt1.t());
							Min.v(absdiff);
						}
					}

					Sum += diff;
					SqSum += diff * diff;

				}
				else
					throw Exception(fmt::format("CompareResult::Update - can compare single points, not sets {} {}", p1.size(), p2.size()));

				Count_++;
			}

			CompareResult& Finish()
			{
				if (!Finished_)
				{
					if (Count_ > 0)
						Avg = Sum / Count_;
					Finished_ = true;
				}

				return *this;
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

		TimeSeriesData<T, V> Difference(const TimeSeriesData<T, V>& ExtData, const optionsT& options) const
		{
			const auto uniontime{ UnionTime(ExtData, options) };
			TimeSeriesData<T, V> ret;
			ret.reserve(uniontime.size());
			for (const auto& time : uniontime)
			{
				const auto op1{ GetTimePoints(time, options) };
				const auto op2{ ExtData.GetTimePoints(time, options) };
				if(op1.size() > 1 || op2.size() > 1)
					throw Exception(fmt::format("TimeSeriesData::Difference can be computed for single points {} {}", op1.size(), op2.size()));
				ret.emplace_back(time, op1.front().v() - op2.front().v());
			}
			return ret;
		}

		CompareResult Compare(const TimeSeriesData<T,V>& ExtData, const optionsT& options) const
		{
			CompareResult comps;
			for (const auto& time : UnionTime(ExtData, options))
				comps.Update(GetTimePoints(time, options), ExtData.GetTimePoints(time, options));
			return comps.Finish();
		}

		size_t Compress(const optionsT& options)
		{
			size_t originalsize{ TimeSeriesData::size() };
			TimeSeriesData<T, V> compressed;
			compressed.reserve(originalsize);
			auto it{ TimeSeriesData::begin() };
			if (it != TimeSeriesData::end())
			{
				compressed.emplace_back(*it);
				it++;
				const auto tolt{ 2.0 * options.TimeTolerance() };
				for (; it != TimeSeriesData::end(); it++)
				{
					const auto prev{ std::prev(it) };
					if (std::abs(prev->t() - it->t()) < tolt && std::abs(prev->v() - it->v()) < options.ValueTolerance())
						continue;

					const auto next{ std::next(it) };
					if (next != TimeSeriesData::end())
					{
						auto denom{ next->t() - prev->t() };
						if (std::abs(denom) > 0.0)
						{
							denom = (next->v() - prev->v()) / denom * (it->t() - prev->t()) + prev->v();
							if (std::abs(it->v() - denom) < options.ValueTolerance())
								continue;
						}
					}
					compressed.emplace_back(*it);
				}
			}
			TimeSeriesData::swap(compressed);
			return originalsize - TimeSeriesData::size();
		}
	};


	template<typename T, typename V>
	class TimeSeries
	{
	private:
		friend class TimeSeriesTests;
		TimeSeriesData<T, V> Data_;
		static constexpr const char* TimeSeriesDoNotMatch = "Times and Values sizes do not match: Times {} and Values {}";
	public:
		TimeSeries() = default;
		TimeSeries(std::initializer_list<const T> Times, std::initializer_list<const V> Values)
		{
			if (Times.size() != Values.size())
				throw Exception(fmt::format(TimeSeriesDoNotMatch, Times.size(), Values.size()));

			auto v{ std::as_const(Values).begin()};
			for (const auto& t : Times)
				Data_.emplace_back( t, *v++ );
		}

		TimeSeries(size_t Size, const T* Times, const V* Values)
		{
			for (auto pT{Times}; pT < Times + Size ; pT++)
				Data_.emplace_back( *pT, *Values++ );
		}

		TimeSeries(const std::filesystem::path path)
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
					csvfile.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
					Data_.emplace_back(time, value);
				}
			}
			else
				throw Exception(fmt::format("TimeSeries::TimeSeries - failed to open {}", path.string()));
		}
		
		TimeSeries DenseOutput(const T& Start, const T& End, const T& Step, const Options<T,V>&  options) const
		{
			TimeSeries dense;
			auto start{ Data_.end() };
			ptrdiff_t ti{ 0 };
			for (; ; ti++)
			{
				const T t{ Start + static_cast<T>(ti) * Step };
				if (t > End)
					break;

				for(const auto& TimePoint : Data_.GetTimePoints(t, options, start))
					dense.Data_.emplace_back(t, TimePoint.v());
			}
			return dense;
		}
	};
}