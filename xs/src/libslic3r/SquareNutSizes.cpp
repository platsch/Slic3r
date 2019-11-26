#include "SquareNutSizes.hpp"

namespace Slic3r
{
    void SquareNutSizes::getSize(std::string threadSize, double size[])
    {
        if (threadSize == "3")
        {
            size[0] = 5.5;
            size[1] = 5.5;
            size[2] = 1.8;
        }
        else if (threadSize == "4")
        {
            size[0] = 7.0;
            size[1] = 7.0;
            size[2] = 2.2;
        }
        else if (threadSize == "5")
        {
            size[0] = 8;
            size[1] = 8;
            size[2] = 2.7;
        }
        else if (threadSize == "6")
        {
            size[0] = 10.0;
            size[1] = 10.0;
            size[2] = 3.2;
        }
        else if (threadSize == "8")
        {
            size[0] = 13.0;
            size[1] = 13.0;
            size[2] = 4.0;
        }
    }
}