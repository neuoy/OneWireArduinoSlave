using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
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
            if (Lines.Count > 100)
                Lines.RemoveAt(0);
            OnPropertyChanged("ConsoleText");
        }

        public int MaxSequenceSize { get { return 2048; } }

        private class Sequence
        {
            public ulong StartTime { get; set; } // time in microseconds of the first sample
            public byte[] Data { get; set; }
            public double Frequency { get; set; } // sampling rate (in samples per second)

            public class Comparer : IComparer<Sequence>
            {
                public int Compare(Sequence x, Sequence y)
                {
                    return x.StartTime.CompareTo(y.StartTime);
                }
            }
        }
        double ValueToHeight(double time, byte value)
        {
            //value = (byte)(Math.Sin(time * 100.0) * 127.0 + 128.0);
            return 256.0 - (double)value + 10;
        }
        private SortedSet<Sequence> Sequences = new SortedSet<Sequence>(new Sequence.Comparer());
        public IEnumerable<Line> Oscilloscope
        {
            get
            {
                if(!Sequences.Any())
                    yield break;
                
                foreach (var sequence in Sequences)
                {
                    double seqStartTime = (double)sequence.StartTime/1000000.0;
                    double sampleDelay = 1.0 / sequence.Frequency;
                    if (seqStartTime + (double)sequence.Data.Length * sampleDelay > ViewportStartTime && seqStartTime < ViewportStartTime + ViewportTimeWidth)
                    {
                        foreach (var line in ConvertToLines(sequence))
                            yield return line;
                    }
                }
            }
        }

        private IEnumerable<Line> ConvertToLines(Sequence sequence)
        {
            double viewportWidth = OscilloscopeCanvas.ActualWidth;
            double scale = viewportWidth / ViewportTimeWidth; // in pixels per second

            ulong displayStartTime = (ulong)(viewportStartTime_ * 1000000.0);
            double sampleDelay = 1.0 / sequence.Frequency;

            double pos = ((double)sequence.StartTime - (double)displayStartTime) / 1000000.0 * scale;
            if (pos > 1000)
                yield break;
            double prevPos = pos;
            byte minValue = sequence.Data[0];
            byte maxValue = minValue;
            int prevIdx = 0;
            double prevHeight = ValueToHeight(sequence.StartTime / 1000000.0, minValue);
            for (int idx = 1; idx < sequence.Data.Length; ++idx)
            {
                byte value = sequence.Data[idx];
                pos += sampleDelay * scale;
                if (value > maxValue) maxValue = value;
                if (value < minValue) minValue = value;

                if (pos > 0 && pos < viewportWidth && pos - prevPos >= 5 || idx == sequence.Data.Length - 1)
                {
                    var line = new Line();
                    line.Stroke = System.Windows.Media.Brushes.LightSteelBlue;
                    line.HorizontalAlignment = HorizontalAlignment.Left;
                    line.VerticalAlignment = VerticalAlignment.Center;
                    line.StrokeThickness = 1;

                    line.X1 = prevPos;
                    prevPos = pos;
                    line.X2 = pos;

                    double time = (double)sequence.StartTime / 1000000.0 + (double)idx * sampleDelay;
                    double lastHeight = ValueToHeight(time, value);

                    if (idx == prevIdx + 1)
                    {
                        line.Y1 = prevHeight;
                        line.Y2 = lastHeight;
                    }
                    else
                    {
                        if (value - minValue > maxValue - value)
                        {
                            line.Y1 = ValueToHeight(time, minValue);
                            line.Y2 = ValueToHeight(time, maxValue);
                        }
                        else
                        {
                            line.Y1 = ValueToHeight(time, maxValue);
                            line.Y2 = ValueToHeight(time, minValue);
                        }
                    }

                    prevHeight = lastHeight;
                    minValue = value;
                    maxValue = value;
                    prevIdx = idx;

                    yield return line;
                }
            }
        }

        public void AddSequence(ulong startTime, byte frequency, byte[] data)
        {
            // TODO: merge sequences if total size is lower than MaxSequenceSize
            double cpuClock = 16000000.0;
            int adsp0 = (frequency & (1 << 0)) != 0 ? 1 : 0;
            int adsp1 = (frequency & (1 << 1)) != 0 ? 1 : 0;
            int adsp2 = (frequency & (1 << 2)) != 0 ? 1 : 0;
            int skipBit0 = (frequency & (1 << 3)) != 0 ? 1 : 0;
            int skipBit1 = (frequency & (1 << 4)) != 0 ? 1 : 0;
            int skipBit2 = (frequency & (1 << 5)) != 0 ? 1 : 0;
            int adcDivider =  1 << Math.Max(1, adsp0 + adsp1*2 + adsp2*4);
            int skip = skipBit0 + skipBit1 * 2 + skipBit2 * 4;
            double freq = cpuClock / (double)adcDivider / 13.0 / (double)(1 + skip);
            var sequence = new Sequence { StartTime = startTime, Frequency = freq, Data = data };
            Sequences.Add(sequence);
            OnPropertyChanged("Oscilloscope");
            OnPropertyChanged("MinTime");
            OnPropertyChanged("MaxTime");
            if (Sequences.Count == 1)
            {
                ViewportStartTime = MinTime;
            }

            var canvas = OscilloscopeCanvas;
            foreach (var line in ConvertToLines(sequence))
                canvas.Children.Add(line);
        }

        void RefreshOscilloscope()
        {
            var canvas = OscilloscopeCanvas;
            canvas.Children.Clear();
            foreach (var line in Oscilloscope)
                canvas.Children.Add(line);
        }

        private Canvas oscilloscopeCanvas_;
        public Canvas OscilloscopeCanvas { get { if (oscilloscopeCanvas_ == null) oscilloscopeCanvas_ = (Canvas)Window.FindName("Oscilloscope"); return oscilloscopeCanvas_; } }

        public double MinTime { get { return Sequences.Any() ? (double)Sequences.First().StartTime / 1000000.0 : 0.0; } }
        public double MaxTime { get { return Sequences.Any() ? Math.Max(MinTime + 0.1, (double)Sequences.Last().StartTime / 1000000.0) : 0.1; } }
        private double viewportTimeWidth_ = 0.1;
        public double ViewportTimeWidth
        {
            get { return viewportTimeWidth_; }
            set { viewportTimeWidth_ = value; OnPropertyChanged("ViewportTimeWidth"); RefreshOscilloscope(); }
        }

        private double viewportStartTime_ = 0;
        public double ViewportStartTime
        {
            get { return viewportStartTime_; }
            set { viewportStartTime_ = value; OnPropertyChanged("ViewportStartTime"); RefreshOscilloscope(); }
        }
        public double ScrollValue
        {
            get { return MathEx.Unproject(MathEx.Project(ViewportStartTime, MinTime, MaxTime - ViewportTimeWidth), MinTime, MaxTime); }
            set { ViewportStartTime = MathEx.Unproject(MathEx.Project(value, MinTime, MaxTime), MinTime, MaxTime - ViewportTimeWidth); }
        }

        public void SetViewport(double startTime, double timeWidth)
        {
            viewportStartTime_ = startTime;
            ViewportTimeWidth = timeWidth;
        }

        public event PropertyChangedEventHandler PropertyChanged;
        protected void OnPropertyChanged(string name)
        {
            if (PropertyChanged != null)
            {
                PropertyChanged(this, new PropertyChangedEventArgs(name));
            }
        }
    }
}
