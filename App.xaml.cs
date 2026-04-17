using System;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Threading;
using Wpf.Ui.Appearance;

namespace RdpShadow;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        // Catch anything an async void handler or converter throws, so the user
        // sees a friendly dialog instead of the default WPF crash window.
        DispatcherUnhandledException += OnDispatcherUnhandledException;
        TaskScheduler.UnobservedTaskException += (_, ev) => ev.SetObserved();
        AppDomain.CurrentDomain.UnhandledException += (_, ev) =>
        {
            if (ev.ExceptionObject is Exception ex)
                ShowCrashDialog(ex);
        };

        // Follow Windows system theme (dark/light) and keep it in sync.
        // Runs before StartupUri creates MainWindow, so no theme flash.
        ApplicationThemeManager.ApplySystemTheme();
        SystemThemeWatcher.Watch(
            window: null,
            backdrop: Wpf.Ui.Controls.WindowBackdropType.Acrylic,
            updateAccents: true);
    }

    private void OnDispatcherUnhandledException(
        object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        e.Handled = true;
        ShowCrashDialog(e.Exception);
    }

    private static void ShowCrashDialog(Exception ex)
    {
        try
        {
            var mb = new Wpf.Ui.Controls.MessageBox
            {
                Title           = "Unexpected Error",
                Content         = ex.Message + "\n\n" + ex.GetType().FullName,
                CloseButtonText = "OK",
            };
            _ = mb.ShowDialogAsync();
        }
        catch
        {
            // Last resort — don't let the crash handler itself crash.
            MessageBox.Show(ex.ToString(), "Unexpected Error",
                MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }
}
