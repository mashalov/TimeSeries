#pragma once
#include <vector>
#include <variant>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <algorithm>

class TimeSeriesTests;

namespace TimeSeries
{
	class Exception : public std::runtime_error
	{
	public:
		using std::runtime_error::runtime_error;
	};

	template<typename T>
	class TimeT
	{
	private:
		T t_;
	public:
		TimeT(const T& t) : t_{ t }{}
		operator const T& () const { return t_; }
		bool
			operator < (const T& rhs) const { return t_ < rhs.t_; }
	};

	template<typename V>
	class ValueT
	{
	private:
		V v_;
	public:
		ValueT(const V& v) : v_{ v }{}
		operator const V& () const { return v_; }
	};

	template<typename T, typename V>
	class PointT
	{
	private:
		TimeT<T> t_;
		ValueT<V> v_;
	public:
		PointT(const T& t, const V& v) : t_{ t }, v_{ v }{}
		const T& t() const { return t_; };
		const V& v() const { return v_;	}
	};


	template<typename T, typename V>
	class TimeSeriesData : public std::vector<PointT<T, V>>
	{
		friend class TimeSeriesTests;
		using fwitT = typename TimeSeriesData<T,V>::const_iterator;
		using pointT = typename PointT<T, V>;
		using TimePointReturnT = std::variant<std::monostate, ValueT<V>, TimeSeriesData>;
		using NonMonotonicPairT = std::optional<std::pair<const pointT&, const pointT&>>;
		bool Checked_ = false;
		void Check()
		{
			if (Checked_)
				return;
			if (auto monotonic{ IsMonotonic() }; monotonic.has_value())
				throw Exception("TimeSeriesData::Check - time series is not monotonic");
			Checked_ = true;
		}
	public:
		NonMonotonicPairT IsMonotonic() const
		{
			if (!TimeSeriesData::size())
				return {};

			auto TimePoint{ TimeSeriesData::begin() };
			auto PrevTime{ TimePoint };
			TimePoint = std::next(TimePoint);
			for (; TimePoint != TimeSeriesData::end(); TimePoint++)
				if (PrevTime->t() > TimePoint->t())
					return { {*PrevTime, *TimePoint} };
				else
					PrevTime = TimePoint;

			return {};
		}
		
		TimePointReturnT GetTimePoints(TimeT<T> Time, const fwitT Start = {}) const
		{
			auto start { Start == fwitT{} ? TimeSeriesData::begin() : Start };
			const auto pred = [](const pointT& lhs, const pointT& rhs) -> bool
			{
				return lhs.t() < rhs.t();
			};
			const pointT checkpoint{ Time, {} };
			auto left{ std::lower_bound(start, TimeSeriesData::end(), checkpoint, pred) };
			auto right{ std::upper_bound(start, TimeSeriesData::end(), checkpoint, pred) };
			return { start->v() };
		}
	};


	template<typename T, typename V>
	class TimeSeries
	{
	private:
		friend class TimeSeriesTests;
		TimeSeriesData<T, V> Data_;
		static constexpr const char* TimeSeriesDoNotMatch = "Times and Values sizes do not match";
	public:
		TimeSeries() = default;
		TimeSeries(std::initializer_list<const T> Times, std::initializer_list<const V> Values)
		{
			if (Times.size() != Values.size())
				throw Exception(TimeSeriesDoNotMatch);

			auto v{ std::as_const(Values).begin()};
			for (const auto& t : Times)
				Data_.emplace_back( t, *v++ );
		}

		TimeSeries(size_t Size, const T* Times, const V* Values)
		{
			for (auto pT{Times}; pT < Times + Size ; pT++)
				Data_.emplace_back( *pT, *Values++ );
		}

		void Dump() const
		{
			for (const auto& TimePoint : Data_)
				std::cout << TimePoint.t() << ";" << TimePoint.v() << std::endl;
		}
	};
}