using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Shapes;

namespace SerialMonitor
{
    public class MainWindowContext : INotifyPropertyChanged
    {
        private MainWindow Window;

        public MainWindowContext(MainWindow window)
        {
            Window = window;
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

        private class Sequence
        {
            public ulong StartTime { get; set; }
            public byte[] Data { get; set; }

            public class Comparer : IComparer<Sequence>
            {
                public int Compare(Sequence x, Sequence y)
                {
                    return x.StartTime.CompareTo(y.StartTime);
                }
            }
        }
        double ValueToHeight(byte value) { return (double)value + 10; }
        private SortedSet<Sequence> Sequences = new SortedSet<Sequence>(new Sequence.Comparer());
        public IEnumerable<Line> Oscilloscope
        {
            get
            {
                if(!Sequences.Any())
                    yield break;

                ulong startTime = Sequences.ElementAt(0).StartTime;
                
                foreach (var sequence in Sequences)
                {
                    foreach (var line in ConvertToLines(sequence, startTime))
                        yield return line;
                }
            }
        }

        private IEnumerable<Line> ConvertToLines(Sequence sequence, ulong displayStartTime)
        {
            double sampleDelay = 0.000936; // in seconds
            double scale = 10; // in pixels per second

            double pos = (sequence.StartTime - displayStartTime) / 1000000.0 * scale;
            if (pos > 1000)
                yield break;
            double prevHeight = ValueToHeight(sequence.Data[0]);
            for (int idx = 1; idx < sequence.Data.Length; ++idx)
            {
                byte value = sequence.Data[idx];
                var line = new Line();
                line.Stroke = System.Windows.Media.Brushes.LightSteelBlue;
                line.X1 = pos;
                pos += sampleDelay * scale;
                line.X2 = pos;
                line.Y1 = prevHeight;
                prevHeight = ValueToHeight(value);
                line.Y2 = prevHeight;
                line.HorizontalAlignment = HorizontalAlignment.Left;
                line.VerticalAlignment = VerticalAlignment.Center;
                line.StrokeThickness = 1;
                yield return line;
            }
        }

        public void AddSequence(ulong startTime, byte[] data)
        {
            var sequence = new Sequence { StartTime = startTime, Data = data };
            Sequences.Add(sequence);
            OnPropertyChanged("Oscilloscope");
            var canvas = (Canvas)Window.FindName("Oscilloscope");
            /*canvas.Children.Clear();
            foreach (var line in Oscilloscope)
                canvas.Children.Add(line);*/
            foreach (var line in ConvertToLines(sequence, Sequences.ElementAt(0).StartTime))
                canvas.Children.Add(line);
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
