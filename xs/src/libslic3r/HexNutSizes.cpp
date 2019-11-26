#include "HexNutSizes.hpp"

namespace Slic3r
{
    void HexNutSizes::getSize(std::string threadSize, double size[])
    {
        if (threadSize == "2")
        {
            size[0] = 4.32;
            size[1] = 4.0;
            size[2] = 1.6;
        }
        else if (threadSize == "2.5")
        {
            size[0] = 5.45;
            size[1] = 5.0;
            size[2] = 2.0;
        }
        else if (threadSize == "3")
        {
            size[0] = 6.01;
            size[1] = 5.5;
            size[2] = 2.4;
        }
        else if (threadSize == "4")
        {
            size[0] = 7.66;
            size[1] = 7;
            size[2] = 3.2;
        }
        else if (threadSize == "5")
        {
            size[0] = 8.79;
            size[1] = 8;
            size[2] = 4.7;
        }
        else if (threadSize == "6")
        {
            size[0] = 11.05;
            size[1] = 10;
            size[2] = 5.2;
        }
    }
}