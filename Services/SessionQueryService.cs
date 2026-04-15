using System.Collections.Generic;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using RdpShadow.Models;

namespace RdpShadow.Services;

public static class SessionQueryService
{
    // Groups: [1]=SessionName  [2]=Username  [3]=ID  [4]=State
    private static readonly Regex SessionRegex =
        new(@"^\s*(\S+)\s+(\S*)\s+(\d+)\s+(\S+)", RegexOptions.Compiled);

    private static bool IsLocal(string server) =>
        string.IsNullOrWhiteSpace(server) ||
        server.Equals("localhost",  StringComparison.OrdinalIgnoreCase) ||
        server == "127.0.0.1";

    public static async Task<List<SessionInfo>> QueryAsync(string server)
    {
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
        await proc.WaitForExitAsync();

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

            sessions.Add(new SessionInfo
            {
                SessionName = sessionName,
                Username    = m.Groups[2].Value,
                Id          = m.Groups[3].Value,
                State       = m.Groups[4].Value,
            });
        }

        sessions.Sort((a, b) =>
            string.Compare(a.Username, b.Username, StringComparison.CurrentCultureIgnoreCase));

        return sessions;
    }
}
