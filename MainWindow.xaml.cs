using System;
using System.Diagnostics;
using System.Windows;
using System.Windows.Input;
using RdpShadow.Models;
using RdpShadow.Services;
using Wpf.Ui.Controls;

namespace RdpShadow;

public partial class MainWindow : FluentWindow
{
    public MainWindow()
    {
        InitializeComponent();
    }

    // ── Enter in server box triggers refresh ─────────────────────────────
    private void TxtServer_KeyDown(object sender, KeyEventArgs e)
    {
        if (e.Key == Key.Enter)
            BtnRefresh_Click(sender, e);
    }

    // ── Refresh session list ─────────────────────────────────────────────
    private async void BtnRefresh_Click(object sender, RoutedEventArgs e)
    {
        SetBusy(true, "Querying sessions…");
        BtnRefresh.IsEnabled = false;
        BtnShadow.IsEnabled  = false;
        LvSessions.ItemsSource = null;
        TxtCount.Text = "";

        try
        {
            var server   = TxtServer.Text.Trim();
            var sessions = await SessionQueryService.QueryAsync(server);

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

            var mb = new Wpf.Ui.Controls.MessageBox
            {
                Title   = "Query Failed",
                Content = ex.Message,
            };
            await mb.ShowDialogAsync();
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

        var server  = TxtServer.Text.Trim();
        var target  = string.IsNullOrWhiteSpace(server) ? "localhost" : server;
        var control = ChkControl.IsChecked == true ? " /control" : "";
        var args    = $"/shadow:{session.Id} /v:{target} /noConsentPrompt{control}";

        try
        {
            Process.Start("mstsc", args);
            SetBusy(false,
                $"Shadowing session {session.Id}" +
                (string.IsNullOrEmpty(session.Username) ? "" : $" ({session.Username})") +
                $" on {target}.");
        }
        catch (Exception ex)
        {
            var mb = new Wpf.Ui.Controls.MessageBox
            {
                Title   = "Shadow Failed",
                Content = ex.Message,
            };
            await mb.ShowDialogAsync();
        }
    }

    // ── Enable / disable Shadow button based on selection ────────────────
    private void LvSessions_SelectionChanged(
        object sender,
        System.Windows.Controls.SelectionChangedEventArgs e)
    {
        BtnShadow.IsEnabled = LvSessions.SelectedItem is SessionInfo;
    }

    // ── Helpers ──────────────────────────────────────────────────────────
    private void SetBusy(bool active, string status)
    {
        PbLoading.Visibility = active ? Visibility.Visible : Visibility.Collapsed;
        TxtStatus.Text       = status;
    }
}
