using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialMonitor
{
    public class SerialMessage
    {
        public string ChannelName { get; set; }
        
        /// <summary>
        /// Time at which the message was sent, in microseconds since the start of the remote executable
        /// </summary>
        public ulong SendTime { get; set; }

        public byte[] Data { get; set; }

        public string StringData
        {
            get { return Encoding.UTF8.GetString(Data); }
            set { Data = Encoding.UTF8.GetBytes(value); }
        }
    }
}
