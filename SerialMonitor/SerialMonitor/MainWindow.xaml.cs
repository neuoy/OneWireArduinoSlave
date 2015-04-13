using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace SerialMonitor
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            var context = new MainWindowContext(this);
            context.WriteLine("Connecting...");
            this.DataContext = context;

            this.MouseWheel += OnMouseWheel;
        }

        private void OnMouseWheel(object sender, MouseWheelEventArgs e)
        {
            MainWindowContext context = (MainWindowContext)this.DataContext;
            Point cursorPos = e.GetPosition(context.OscilloscopeCanvas);
            if (cursorPos.X < 0 || cursorPos.X > context.OscilloscopeCanvas.ActualWidth || cursorPos.Y < 0 || cursorPos.Y > context.OscilloscopeCanvas.ActualHeight)
                return;

            double cursorPosRatio = cursorPos.X / context.OscilloscopeCanvas.ActualWidth;
            double cursorTime = context.ViewportStartTime + cursorPosRatio * context.ViewportTimeWidth;

            double newTimeWidth = context.ViewportTimeWidth;
            if (e.Delta > 0)
                newTimeWidth /= e.Delta * 0.01;
            else if (e.Delta < 0)
                newTimeWidth *= -e.Delta * 0.01;

            double totalTimeWidth = Math.Max(0.1, context.MaxTime - context.MinTime);
            if (newTimeWidth > totalTimeWidth)
                newTimeWidth = totalTimeWidth;

            double newStartTime = cursorTime - cursorPosRatio * newTimeWidth;
            if (newStartTime < context.MinTime)
                newStartTime = context.MinTime;
            if (newStartTime + newTimeWidth > context.MaxTime)
                newStartTime = context.MaxTime - newTimeWidth;

            context.SetViewport(newStartTime, newTimeWidth);
        }
    }
}
