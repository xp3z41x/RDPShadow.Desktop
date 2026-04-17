using System;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using RdpShadow.Models;
using RdpShadow.Services;
using Wpf.Ui.Controls;

namespace RdpShadow;

public partial class MainWindow : FluentWindow
{
    private static readonly TimeSpan QueryTimeout  = TimeSpan.FromSeconds(15);
    private static readonly TimeSpan ShadowLaunchGrace = TimeSpan.FromSeconds(3);

    public MainWindow()
    {
        InitializeComponent();
    }

    // ── Enter in server box triggers refresh (guarded against concurrent runs) ─
    private void TxtServer_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter && BtnRefresh.IsEnabled)
            BtnRefresh_Click(sender, e);
    }

    // ── F5 anywhere refreshes ────────────────────────────────────────────
    private void RefreshShortcut_Executed(object sender, ExecutedRoutedEventArgs e)
    {
        if (BtnRefresh.IsEnabled) BtnRefresh_Click(sender, e);
    }

    // ── Refresh session list ─────────────────────────────────────────────
    private async void BtnRefresh_Click(object sender, RoutedEventArgs e)
    {
        SetBusy(true, "Querying sessions…");
        BtnRefresh.IsEnabled = false;
        BtnShadow.IsEnabled  = false;
        TxtCount.Text = "";

        try
        {
            var server = TxtServer.Text.Trim();

            using var cts = new CancellationTokenSource(QueryTimeout);
            var sessions  = await SessionQueryService.QueryAsync(server, cts.Token);

            LvSessions.ItemsSource = sessions;

            var label = string.IsNullOrWhiteSpace(server) ||
                        server.Equals("localhost", StringComparison.OrdinalIgnoreCase)
                ? "localhost"
                : server;

            TxtCount.Text = sessions.Count == 0
                ? "No sessions found"
                : $"{sessions.Count} session{(sessions.Count == 1 ? "" : "s")} on {label}";

            SetBusy(false, sessions.Count == 0
                ? "No sessions found."
                : $"Loaded {sessions.Count} session(s) from {label}.");
        }
        catch (Exception ex)
        {
            SetBusy(false, $"Error: {ex.Message}");
            TxtCount.Text = "";
            LvSessions.ItemsSource = null;

            await ShowErrorAsync("Query Failed", ex.Message);
        }
        finally
        {
            BtnRefresh.IsEnabled = true;
        }
    }

    // ── Launch shadow session ────────────────────────────────────────────
    private async void BtnShadow_Click(object sender, RoutedEventArgs e)
    {
        if (LvSessions.SelectedItem is not SessionInfo session) return;
        if (string.IsNullOrEmpty(session.Username))
        {
            await ShowErrorAsync(
                "Cannot Shadow",
                "This session has no user attached (system / listener). Select a user session.");
            return;
        }

        var server  = TxtServer.Text.Trim();
        var target  = string.IsNullOrWhiteSpace(server) ? "localhost" : server;
        var control = ChkControl.IsChecked == true ? " /control" : "";
        var args    = $"/shadow:{session.Id} /v:{target} /noConsentPrompt{control}";

        BtnShadow.IsEnabled = false;
        try
        {
            var psi = new ProcessStartInfo("mstsc", args)
            {
                UseShellExecute = true,
            };
            using var proc = Process.Start(psi)
                ?? throw new InvalidOperationException("Failed to start mstsc.");

            // Give mstsc a short window to fail fast (bad ID, access denied, etc.).
            // If it's still running after the grace period, shadow is live.
            using var cts = new CancellationTokenSource(ShadowLaunchGrace);
            try
            {
                await proc.WaitForExitAsync(cts.Token);
                if (proc.ExitCode != 0)
                {
                    SetBusy(false, $"mstsc failed (exit {proc.ExitCode}).");
                    await ShowErrorAsync(
                        "Shadow Failed",
                        $"mstsc exited with code {proc.ExitCode}. " +
                        "Usual causes: remote user rejected consent, missing firewall rules, " +
                        "or registry policy 'Shadow' not configured on the target.");
                    return;
                }
            }
            catch (OperationCanceledException)
            {
                // Still running after grace period → shadow session is active.
            }

            SetBusy(false,
                $"Shadowing session {session.Id}" +
                (string.IsNullOrEmpty(session.Username) ? "" : $" ({session.Username})") +
                $" on {target}.");
        }
        catch (Exception ex)
        {
            await ShowErrorAsync("Shadow Failed", ex.Message);
        }
        finally
        {
            // Re-enable based on current selection rules.
            UpdateShadowButtonState();
        }
    }

    // ── Double-click a row → shadow it ───────────────────────────────────
    private void LvSessions_MouseDoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (BtnShadow.IsEnabled) BtnShadow_Click(sender, e);
    }

    // ── Enable / disable Shadow button based on selection ────────────────
    private void LvSessions_SelectionChanged(
        object sender,
        SelectionChangedEventArgs e)
    {
        UpdateShadowButtonState();
    }

    private void UpdateShadowButtonState()
    {
        BtnShadow.IsEnabled =
            LvSessions.SelectedItem is SessionInfo s &&
            !string.IsNullOrEmpty(s.Username);
    }

    // ── Helpers ──────────────────────────────────────────────────────────
    private void SetBusy(bool active, string status)
    {
        PbLoading.Visibility = active ? Visibility.Visible : Visibility.Collapsed;
        TxtStatus.Text       = status;
    }

    private Task ShowErrorAsync(string title, string content)
    {
        var mb = new Wpf.Ui.Controls.MessageBox
        {
            Title             = title,
            Content           = content,
            CloseButtonText   = "OK",
            Owner             = this,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
        };
        return mb.ShowDialogAsync();
    }
}
