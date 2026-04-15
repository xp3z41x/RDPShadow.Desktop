using System;
using System.Globalization;
using System.Windows.Data;
using System.Windows.Media;

namespace RdpShadow.Converters;

[ValueConversion(typeof(string), typeof(Brush))]
public sealed class StateToColorConverter : IValueConverter
{
    // Prefixes covering the localized values `query session` returns across
    // common Windows display languages. The column is 7–8 chars wide so
    // values are usually truncated — we match the shortest unambiguous prefix.
    private static readonly string[] ActivePrefixes =
    {
        "ACT",   // Active (en) / Activo (es) / Actif (fr)
        "ATIV",  // Ativo (pt-BR)
        "AKT",   // Aktiv (de, sv, no, da)
        "АКТ",   // Активн (ru)
    };

    private static readonly string[] DisconnectedPrefixes =
    {
        "DISC",  // Disconn / Disconectado (en, es)
        "DESC",  // Desconectado (pt-BR)
        "DECO",  // Déconnecté (fr)
        "GETR",  // Getrennt (de)
        "FRÅN",  // Frånkopplad (sv)
        "ОТКЛ",  // Отключено (ru)
    };

    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        var s = value?.ToString()?.ToUpperInvariant() ?? "";

        foreach (var p in ActivePrefixes)
            if (s.StartsWith(p, StringComparison.Ordinal))
                return new SolidColorBrush(Color.FromRgb(0x00, 0xCC, 0x6A)); // green

        foreach (var p in DisconnectedPrefixes)
            if (s.StartsWith(p, StringComparison.Ordinal))
                return new SolidColorBrush(Color.FromRgb(0xFF, 0x8C, 0x00)); // amber

        return new SolidColorBrush(Color.FromRgb(0x66, 0x66, 0x66));         // grey
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
