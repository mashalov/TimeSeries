// TimeSeries.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include "Tests.h"

int main()
{
    try
    {
        TimeSeriesTest::TimeSeriesTests::TestAll();
    }
    catch (const std::runtime_error& ex)
    {
        std::cout << ex.what() << std::endl;
    }
}

