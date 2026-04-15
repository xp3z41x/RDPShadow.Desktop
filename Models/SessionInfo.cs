namespace RdpShadow.Models;

public sealed class SessionInfo
{
    public string Id          { get; init; } = "";
    public string SessionName { get; init; } = "";
    public string Username    { get; init; } = "";
    public string State       { get; init; } = "";

    public bool IsActive
    {
        get
        {
            var s = State.ToUpperInvariant();
            return s.StartsWith("ACT", StringComparison.Ordinal)  // Active / Activo / Actif
                || s.StartsWith("ATIV", StringComparison.Ordinal) // Ativo (pt-BR)
                || s.StartsWith("AKT", StringComparison.Ordinal); // Aktiv (de/sv/no/da)
        }
    }

    // Display helper — blank username shows em dash
    public string DisplayUsername =>
        string.IsNullOrEmpty(Username) ? "—" : Username;
}
