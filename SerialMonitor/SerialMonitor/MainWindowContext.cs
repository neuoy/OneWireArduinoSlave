using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace SerialMonitor
{
    public class MainWindowContext : INotifyPropertyChanged
    {
        public MainWindowContext()
        {
            Get = this;
        }

        public static MainWindowContext Get { get; private set; }

        private List<string> Lines = new List<string>();
        public string ConsoleText { get { return String.Join("\n", Lines); } }

        public void WriteLine(string line)
        {
            Lines.Add(line);
            if (Lines.Count > 9)
                Lines.RemoveAt(0);
            OnPropertyChanged("ConsoleText");
        }

        public event PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged(string name)
        {
            PropertyChangedEventHandler handler = PropertyChanged;
            if (handler != null)
            {
                handler(this, new PropertyChangedEventArgs(name));
            }
        }
    }
}
