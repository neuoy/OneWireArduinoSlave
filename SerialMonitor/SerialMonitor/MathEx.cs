using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialMonitor
{
    public static class MathEx
    {
        public static double Project(double value, double rangeMin, double rangeMax)
        {
            return (value - rangeMin) / (rangeMax - rangeMin);
        }

        public static double Unproject(double ratio, double rangeMin, double rangeMax)
        {
            return rangeMin + ratio * (rangeMax - rangeMin);
        }
    }
}
