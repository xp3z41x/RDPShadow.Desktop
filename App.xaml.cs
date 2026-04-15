using System.Windows;
using Wpf.Ui.Appearance;

namespace RdpShadow;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);

        // Follow Windows system theme (dark/light) and keep it in sync.
        ApplicationThemeManager.ApplySystemTheme();
        SystemThemeWatcher.Watch(
            window: null,
            backdrop: Wpf.Ui.Controls.WindowBackdropType.Acrylic,
            updateAccents: true);
    }
}
