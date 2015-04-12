using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialMonitor
{
    public static class SerialPortExtensions
    {
        public static int ReadShort(this SerialPort port)
        {
            int b1 = port.ReadByte();
            int b2 = port.ReadByte();
            return b2 * 256 + b1;
        }

        public static ulong ReadULong(this SerialPort port)
        {
            ulong b1 = (ulong)port.ReadByte();
            ulong b2 = (ulong)port.ReadByte();
            ulong b3 = (ulong)port.ReadByte();
            ulong b4 = (ulong)port.ReadByte();
            return b4 * 256 * 256 * 256 + b3 * 256 * 256 + b2 * 256 + b1;
        }

        public static byte[] ReadBytes(this SerialPort port, int numBytes)
        {
            var bytes = new byte[numBytes];
            int remaining = numBytes;
            int pos = 0;
            while (remaining > 0)
            {
                int read = port.Read(bytes, pos, remaining);
                if (read < 0)
                    throw new Exception("Connection closed");
                remaining -= read;
                pos += read;
            }
            return bytes;
        }
    }
}
