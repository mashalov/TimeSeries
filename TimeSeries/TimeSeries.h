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
		T t_;
		V v_;
	public:
		PointT(const T& t, const V& v) : t_{ t }, v_{ v }{}
		inline const T& t() const { return t_; };
		inline const V& v() const { return v_;	}
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

	template<typename T, typename V>
	class Options
	{
	protected:
		T TimeTolerance_ = 1E-8;
	public:
		inline T TimeTolerance() const { return TimeTolerance_; }
		void SetTimeTolerance(T TimeTolerance) { TimeTolerance_ = TimeTolerance; }
	};


	template<typename T, typename V>
	class TimeSeriesData : public TimeSeriesDataT<T, V>
	{
		friend class TimeSeriesTests;
		using fwitT = typename TimeSeriesData<T,V>::const_iterator;
		using pointT = typename PointT<T, V>;
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
	public:
		NonMonotonicPairT IsMonotonic() const
		{
			// empty series is monotonic
			if (!TimeSeriesData::size())
				return {};

			auto TimePoint{ TimeSeriesData::begin() };
			auto PrevTime{ TimePoint };
			// check with no tolerance to
			// lower_ upper_bound work properly
			TimePoint = std::next(TimePoint);
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
				for (const auto& TimePoint : *this)
					csvfile << TimePoint.t() << ";" << TimePoint.v() << std::endl;
			}
		}

		TimeSeriesData GetTimePoints(const T& Time, const Options<T,V>& options) const
		{
			auto enddummy{ TimeSeriesData::end() };
			return GetTimePoints(Time, options, enddummy);
		}

		TimeSeriesData GetTimePoints(const T& Time, const Options<T,V>& options, fwitT& Start) const
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

			// select range for bound search to left and right from point requested
			auto tolrange{ ToleranceRange(Time, options.TimeTolerance())};
			const auto leftpoint{ pointT(tolrange.first, {}) };
			const auto rightpoint{ pointT(tolrange.second, {}) };
			// get bounds
			auto left{ std::lower_bound(start, TimeSeriesData::end(), leftpoint, pred) };
			auto right{ std::upper_bound(start, TimeSeriesData::end(), rightpoint, pred) };

			// return iterator found for bound to speedup next GetTimePoints call
			Start = left;

			// if there are points between bounds - send them to output
			for (auto it = left; it != right; it++)
				if (it->t() <= Time)
					retdata.emplace_back(*it);

			// if no points around time requested - interpolate series
			// to time requested
			if (retdata.empty())
			{
				Interpolator<T, V> linear;
				retdata.emplace_back(Time, linear.Get(*this, left, Time));
			}

			// debug dump
			if (!retdata.empty())
			{
				std::cout << Time << " : ";
				for (const auto& TimePoint : retdata)
					std::cout << " { " << TimePoint.t() << " ; " << TimePoint.v() << " } ";
				std::cout << std::endl;
			}

			return { retdata };
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
				for (const auto& TimePoint : Data_.GetTimePoints(t, options, start))
					dense.Data_.emplace_back(TimePoint);
			}
			return dense;
		}
	};
}