// TimeSeries.cpp : This file contains the 'main' function. Program execution begins and ends there.

#include <iostream>
#include "Tests.h"

int main()
{
    try
    {
        TimeSeriesTests::TestAll();
    }
    catch (const TimeSeries::Exception& ex)
    {
        std::cout << ex.what() << std::endl;
    }
}

