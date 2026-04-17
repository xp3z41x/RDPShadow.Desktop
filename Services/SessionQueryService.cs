using System.Collections.Generic;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using RdpShadow.Models;

namespace RdpShadow.Services;

public static class SessionQueryService
{
    // Groups: [1]=SessionName  [2]=Username  [3]=ID  [4]=State
    private static readonly Regex SessionRegex =
        new(@"^\s*(\S+)\s+(\S*)\s+(\d+)\s+(\S+)",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);

    // RFC-1123-ish hostname or IPv4/IPv6 literal. Rejects shell metacharacters
    // so user input can't break out of the /server: argument.
    private static readonly Regex HostnameRegex =
        new(@"^[A-Za-z0-9]([A-Za-z0-9\-._:]*[A-Za-z0-9])?$",
            RegexOptions.Compiled | RegexOptions.CultureInvariant);

    // State prefixes that mean "not a real user session" — the rdp-tcp listener
    // and equivalents. Localized across common Windows display languages.
    private static readonly string[] NonUserStatePrefixes =
    {
        "LISTEN",  // en
        "ESCUTA",  // pt-BR (Escuta / Escutar)
        "ESCUCH",  // es (Escucha)
        "ÉCOUTE",  // fr
        "LAUSCH",  // de (Lauschen)
        "LYSSN",   // sv (Lyssnar)
        "ПРОСЛ",   // ru (Прослушивание)
    };

    private static bool IsLocal(string server) =>
        string.IsNullOrWhiteSpace(server) ||
        server.Equals("localhost",  StringComparison.OrdinalIgnoreCase) ||
        server == "127.0.0.1";

    /// <summary>Validate a hostname / IP against the strict allowlist.</summary>
    public static bool IsValidHost(string host) =>
        string.IsNullOrWhiteSpace(host) || HostnameRegex.IsMatch(host);

    private static bool IsListenerState(string state)
    {
        var s = state.ToUpperInvariant();
        foreach (var p in NonUserStatePrefixes)
            if (s.StartsWith(p, StringComparison.Ordinal))
                return true;
        return false;
    }

    public static async Task<List<SessionInfo>> QueryAsync(
        string server, CancellationToken ct = default)
    {
        if (!IsValidHost(server))
            throw new ArgumentException(
                "Invalid hostname. Use letters, digits, dots, or hyphens only.",
                nameof(server));

        var args = IsLocal(server) ? "session" : $"session /server:{server}";

        var psi = new ProcessStartInfo("query", args)
        {
            RedirectStandardOutput = true,
            RedirectStandardError  = true,
            UseShellExecute        = false,
            CreateNoWindow         = true,
        };

        using var proc = Process.Start(psi)
            ?? throw new InvalidOperationException("Failed to start 'query.exe'.");

        // Read stdout and stderr concurrently to avoid deadlocks on large output
        var stdoutTask = proc.StandardOutput.ReadToEndAsync();
        var stderrTask = proc.StandardError.ReadToEndAsync();

        try
        {
            await proc.WaitForExitAsync(ct);
        }
        catch (OperationCanceledException)
        {
            try { proc.Kill(entireProcessTree: true); } catch { /* best effort */ }
            throw new TimeoutException(
                "Server did not respond in time. Check connectivity and firewall rules.");
        }

        var output = await stdoutTask;
        var stderr = await stderrTask;

        // query.exe returns exit code 1 on Windows even on partial success
        // (e.g. when it can't enumerate the rdp-tcp listener). Only treat
        // this as an error if we also got no usable stdout.
        if (proc.ExitCode != 0 && string.IsNullOrWhiteSpace(output))
            throw new InvalidOperationException(
                string.IsNullOrWhiteSpace(stderr)
                    ? $"query.exe exited with code {proc.ExitCode} and returned no output."
                    : stderr.Trim());

        var sessions = new List<SessionInfo>();
        var lines = output.Split(new[] { "\r\n", "\n" }, StringSplitOptions.RemoveEmptyEntries);

        // Need at least header + 1 session row
        if (lines.Length < 2) return sessions;

        // Skip the header line
        foreach (var line in lines[1..])
        {
            var m = SessionRegex.Match(line);
            if (!m.Success) continue;

            // Strip leading '>' marker that query.exe uses for the current session
            var sessionName = m.Groups[1].Value.TrimStart('>');
            var username    = m.Groups[2].Value;
            var state       = m.Groups[4].Value;

            // Hide rdp-tcp listener and equivalents — not shadowable, just noise.
            if (string.IsNullOrEmpty(username) && IsListenerState(state))
                continue;

            sessions.Add(new SessionInfo
            {
                SessionName = sessionName,
                Username    = username,
                Id          = m.Groups[3].Value,
                State       = state,
            });
        }

        sessions.Sort((a, b) =>
            string.Compare(a.Username, b.Username, StringComparison.CurrentCultureIgnoreCase));

        return sessions;
    }
}
