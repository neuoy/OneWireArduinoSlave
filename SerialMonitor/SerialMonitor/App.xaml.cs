using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Diagnostics;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Shapes;
using System.Windows.Threading;

namespace SerialMonitor
{
    /// <summary>
    /// Interaction logic for App.xaml
    /// </summary>
    public partial class App : Application
    {
        private SerialPort Serial;
        private IDictionary<int, string> Channels = new Dictionary<int, string>();
        private bool Connected = false;

        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);

            Serial = new SerialPort();
            Serial.PortName = "COM4";
            Serial.BaudRate = 9600;

            Serial.DataReceived += new System.IO.Ports.SerialDataReceivedEventHandler(OnDataReceived);

            Serial.Open();
            Serial.Write("C");
        }

        private void OnDataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            if (!Connected)
            {
                Serial.ReadTo("CONNECTION");
                Connected = true;
            }

            // channel declaration message: byte 0 ; string "ChannelInit" ; byte <channel id> ; short <length> ; byte[length] <channel name>
            // regular message: byte <channel id> ; short <length> ; byte[length] <data>

            SerialMessage message = null;

            Serial.ReadTo("START");

            int id = Serial.ReadByte();
            if (id == 0)
            {
                string check = Encoding.UTF8.GetString(Serial.ReadBytes(11));
                if (check != "ChannelInit")
                    throw new Exception("Incorrect data check for channel initialization");
                id = Serial.ReadByte();
                int length = Serial.ReadShort();
                string name = Encoding.UTF8.GetString(Serial.ReadBytes(length));
                Channels[id] = name;
                message = new SerialMessage { ChannelName = "debug", StringData = "Channel " + name + " opened", SendTime = 0 };
            }
            else
            {
                ulong sendTime = Serial.ReadULong();
                int length = Serial.ReadShort();
                byte[] data = Serial.ReadBytes(length);
                message = new SerialMessage { ChannelName = Channels[id], Data = data, SendTime = sendTime };
            }

            if(message != null)
                Dispatcher.BeginInvoke(DispatcherPriority.Normal, new Action(() => OnMessageReceived(message)));
        }

        private void OnMessageReceived(SerialMessage message)
        {
            switch (message.ChannelName)
            {
                case "debug":
                    string text = ((float)message.SendTime / 1000000).ToString("0.000000") + "s " + message.StringData;
                    //Debug.WriteLine(text);
                    MainWindowContext.Get.WriteLine(text);
                    break;
                case "oscilloscope":
                    byte frequency = message.Data[0];
                    MainWindowContext.Get.AddSequence(message.SendTime, frequency, message.Data.Skip(1).ToArray());
                    break;
            }
        }
    }
}
