// RdpShadow - RDP session shadowing tool.
// Pure Win32 + GDI+ (flat C API), custom-drawn Fluent-dark UI, zero dependencies.
// Behavior mirrors the original WPF app: query.exe session list -> mstsc /shadow.
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

// ---------------------------------------------------------------- GDI+ flat API
typedef struct { UINT32 ver; void *dbg; BOOL noThread; BOOL noCodecs; } GdipStartupInput;
typedef void *GpGraphics, *GpBrush, *GpPen, *GpPath;
#define GDIP(fn, ...) __declspec(dllimport) int WINAPI fn(__VA_ARGS__)
GDIP(GdiplusStartup, ULONG_PTR*, const GdipStartupInput*, void*);
GDIP(GdipCreateFromHDC, HDC, GpGraphics**);
GDIP(GdipDeleteGraphics, GpGraphics*);
GDIP(GdipSetSmoothingMode, GpGraphics*, int);
GDIP(GdipCreateSolidFill, UINT32, GpBrush**);
GDIP(GdipDeleteBrush, GpBrush*);
GDIP(GdipCreatePen1, UINT32, float, int, GpPen**);
GDIP(GdipDeletePen, GpPen*);
GDIP(GdipCreatePath, int, GpPath**);
GDIP(GdipDeletePath, GpPath*);
GDIP(GdipAddPathArc, GpPath*, float, float, float, float, float, float);
GDIP(GdipClosePathFigure, GpPath*);
GDIP(GdipFillPath, GpGraphics*, GpBrush*, GpPath*);
GDIP(GdipDrawPath, GpGraphics*, GpPen*, GpPath*);
GDIP(GdipFillEllipse, GpGraphics*, GpBrush*, float, float, float, float);
GDIP(GdipDrawArc, GpGraphics*, GpPen*, float, float, float, float, float, float);

// ---------------------------------------------------------------- theme (dark)
#define A(hex)      (0xFF000000u | (hex))            // opaque ARGB for GDI+
#define REF(hex)    RGB((hex) >> 16, ((hex) >> 8) & 0xFF, (hex) & 0xFF)
#define CLR_BG      0x202020   // window
#define CLR_CARD    0x272727   // list card
#define CLR_FIELD   0x2B2B2B   // text field
#define CLR_CTRL    0x2D2D2D   // buttons
#define CLR_HOV     0x333333
#define CLR_PRESS   0x2A2A2A
#define CLR_STROKE  0x3B3B3B
#define CLR_EDGE    0x383838
#define CLR_TXT     0xFFFFFF
#define CLR_TXT2    0xC5C5C5
#define CLR_TXT3    0x9D9D9D
#define CLR_DIS     0x6E6E6E
#define CLR_ACC     0x4CC2FF   // accent
#define CLR_ACC_H   0x47B1E8
#define CLR_ACC_P   0x3FA0D0
#define CLR_GREEN   0x00CC6A
#define CLR_AMBER   0xFF8C00
#define CLR_GRAY    0x808080
#define CLR_ERRBG   0x442726
#define CLR_ERRFG   0xFF99A4
#define CLR_CLOSE   0xC42B1C

// ---------------------------------------------------------------- strings
// UI language: pt-BR when Windows runs in Portuguese, English otherwise.
// The state COLUMN is also rendered from these tables (not from query.exe's
// localized raw text) so the UI never mixes languages.
enum { S_CUE, S_REFRESH, S_CONTROL, S_SHADOW, S_HSESSION, S_HUSER, S_HID, S_HSTATE,
       S_GACTIVE, S_GOTHER, S_EMPTY1, S_EMPTY2, S_EMPTY3, S_READY, S_QUERYING,
       S_QFAILED, S_LAUNCHED, S_STARTING, S_SFAILED, S_NOTHING, S_FOOTER, S_ERROR,
       S_EINVALID, S_ETIMEOUT, S_EQSTART, S_EQEXIT, S_EMSTART, S_EMEXIT,
       S_STACTIVE, S_STDISC, N_STR };
static const WCHAR *STR_EN[N_STR] = {
    L"Hostname or IP (empty = localhost)", L"Refresh", L"Control", L"Shadow Session",
    L"SESSION", L"USERNAME", L"ID", L"STATE",
    L"ACTIVE SESSIONS", L"OTHER SESSIONS",
    L"No sessions found.", L"Enter a hostname and click Refresh.",
    L"No shadowable sessions on this host.",
    L"Ready.", L"Querying sessions\u2026", L"Query failed.",
    L"Shadow session launched.", L"Starting shadow session for %ls (ID %d)\u2026",
    L"Shadow failed.", L"Nothing queried.", L"%d active, %d other on %ls", L"Error",
    L"Invalid hostname. Only letters, digits, hyphens, dots, and colons are allowed.",
    L"Query timed out after 15 seconds.",
    L"Failed to start query.exe (error %lu).", L"query.exe exited with code %lu.",
    L"Failed to start mstsc.exe (error %lu).",
    L"mstsc.exe exited with code %lu. Check that the session ID is valid and shadowing is allowed by policy.",
    L"Active", L"Disconnected",
};
static const WCHAR *STR_PT[N_STR] = {
    L"Nome do host ou IP (vazio = localhost)", L"Atualizar", L"Controle", L"Espelhar Sess\u00E3o",
    L"SESS\u00C3O", L"USU\u00C1RIO", L"ID", L"ESTADO",
    L"SESS\u00D5ES ATIVAS", L"OUTRAS SESS\u00D5ES",
    L"Nenhuma sess\u00E3o encontrada.", L"Digite um nome de host e clique em Atualizar.",
    L"Nenhuma sess\u00E3o dispon\u00EDvel para espelhar neste host.",
    L"Pronto.", L"Consultando sess\u00F5es\u2026", L"Falha na consulta.",
    L"Sess\u00E3o de espelhamento iniciada.", L"Iniciando espelhamento de %ls (ID %d)\u2026",
    L"Falha no espelhamento.", L"Nada consultado.", L"%d ativas, %d outras em %ls", L"Erro",
    L"Nome de host inv\u00E1lido. Apenas letras, d\u00EDgitos, hifens, pontos e dois-pontos s\u00E3o permitidos.",
    L"A consulta expirou ap\u00F3s 15 segundos.",
    L"Falha ao iniciar query.exe (erro %lu).", L"query.exe encerrou com c\u00F3digo %lu.",
    L"Falha ao iniciar mstsc.exe (erro %lu).",
    L"mstsc.exe encerrou com c\u00F3digo %lu. Verifique se o ID da sess\u00E3o \u00E9 v\u00E1lido e se o espelhamento \u00E9 permitido pela pol\u00EDtica.",
    L"Ativa", L"Desconectada",
};
static const WCHAR **T = STR_EN;

// ---------------------------------------------------------------- model
typedef enum { ST_ACTIVE, ST_DISC, ST_OTHER } State;
typedef struct { WCHAR name[64], user[64], raw[48]; int id; State st; } Session;
typedef struct { BOOL group; int si; const WCHAR *label; } Row;  // list line

#define MAXS 64
static Session g_ses[MAXS];  static int g_nses;
static Row     g_rows[MAXS + 2]; static int g_nrows;
static BOOL    g_queried;                      // at least one successful query

// worker -> UI handoff (guarded by g_cs)
static CRITICAL_SECTION g_cs;
static Session g_pses[MAXS]; static int g_pn;
static WCHAR   g_perr[512];
static WCHAR   g_host[256];                    // hostname snapshot for worker

// ---------------------------------------------------------------- ui state
static HWND  g_wnd, g_edit;
static WNDPROC g_editProc;
static int   g_dpi = 96;
static BOOL  g_busy, g_err, g_ctrlMode, g_editFocus, g_track;
static WCHAR g_status[256], g_footer[128], g_errmsg[512];
static int   g_sel = -1;                       // selected row index (session rows only)
static int   g_hot = -1;                       // hovered row
static int   g_hotCtl;                         // hovered control id (HT_*)
static int   g_downCtl;                        // pressed control id
static int   g_scroll, g_scrollMax;
static BOOL  g_dragSb; static int g_dragOff;
static float g_spin;
static HFONT g_fBody, g_fSemi, g_fSmall, g_fSmallSemi, g_fMono, g_fTitle, g_fIco, g_fIcoSm, g_fIcoBig;
static int   g_monoH;                          // mono line height, for centering the edit
static HBRUSH g_brField;

enum { HT_NONE, HT_MIN, HT_CLOSE, HT_REFRESH, HT_SHADOW, HT_CHECK, HT_SCROLL };

typedef struct {                               // computed layout (per size/dpi)
    RECT title, bMin, bClose;
    RECT field, bRefresh, chk, bShadow;
    RECT errbar, list, inner, status;
    int  headH, rowH, grpH;
} Layout;
static Layout L;

#define S(x) MulDiv(x, g_dpi, 96)
static const WCHAR *W_APP = L"RDP Shadow";

// ---------------------------------------------------------------- small helpers
static void TxtR(HDC dc, HFONT f, UINT32 hex, RECT r, UINT flags, const WCHAR *s) {
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, REF(hex));
    HFONT o = SelectObject(dc, f);
    DrawTextW(dc, s, -1, &r, flags | DT_NOPREFIX | DT_SINGLELINE);
    SelectObject(dc, o);
}
static int TxtW(HDC dc, HFONT f, const WCHAR *s) {
    SIZE sz; HFONT o = SelectObject(dc, f);
    GetTextExtentPoint32W(dc, s, (int)wcslen(s), &sz);
    SelectObject(dc, o); return sz.cx;
}
static void FillR(HDC dc, RECT r, UINT32 hex) {
    HBRUSH b = CreateSolidBrush(REF(hex)); FillRect(dc, &r, b); DeleteObject(b);
}
static GpPath *RoundPath(float x, float y, float w, float h, float rad) {
    GpPath *p; float d = rad * 2; GdipCreatePath(0, &p);
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p); return p;
}
static void RoundFill(GpGraphics *g, RECT r, float rad, UINT32 argb) {
    GpPath *p = RoundPath((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), rad);
    GpBrush *b; GdipCreateSolidFill(argb, &b);
    GdipFillPath(g, b, p); GdipDeleteBrush(b); GdipDeletePath(p);
}
static void RoundStroke(GpGraphics *g, RECT r, float rad, UINT32 argb, float w) {
    GpPath *p = RoundPath(r.left + .5f, r.top + .5f, (float)(r.right - r.left - 1), (float)(r.bottom - r.top - 1), rad);
    GpPen *pen; GdipCreatePen1(argb, w, 2, &pen);
    GdipDrawPath(g, pen, p); GdipDeletePen(pen); GdipDeletePath(p);
}
static void Dot(GpGraphics *g, float cx, float cy, float rad, UINT32 argb) {
    GpBrush *b; GdipCreateSolidFill(argb, &b);
    GdipFillEllipse(g, b, cx - rad, cy - rad, rad * 2, rad * 2);
    GdipDeleteBrush(b);
}

// ---------------------------------------------------------------- fonts / dpi
static HFONT MkFont(const WCHAR *name, int px, int weight) {
    return CreateFontW(-S(px), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, name);
}
static void MakeFonts(void) {
    HFONT *all[] = { &g_fBody, &g_fSemi, &g_fSmall, &g_fSmallSemi, &g_fMono, &g_fTitle, &g_fIco, &g_fIcoSm, &g_fIcoBig };
    for (int i = 0; i < 9; i++) if (*all[i]) DeleteObject(*all[i]);
    // Cascadia Mono when present (Win11 / dev boxes), Consolas otherwise
    HFONT mono = MkFont(L"Cascadia Mono", 12, FW_NORMAL);
    HDC dc = GetDC(NULL); WCHAR face[64] = L"";
    HFONT o = SelectObject(dc, mono);
    GetTextFaceW(dc, 64, face); SelectObject(dc, o); ReleaseDC(NULL, dc);
    if (_wcsicmp(face, L"Cascadia Mono")) { DeleteObject(mono); mono = MkFont(L"Consolas", 12, FW_NORMAL); }
    g_fMono      = mono;
    g_fBody      = MkFont(L"Segoe UI", 13, FW_NORMAL);
    g_fSemi      = MkFont(L"Segoe UI Semibold", 13, FW_SEMIBOLD);
    g_fSmall     = MkFont(L"Segoe UI", 12, FW_NORMAL);
    g_fSmallSemi = MkFont(L"Segoe UI Semibold", 11, FW_SEMIBOLD);
    g_fTitle     = MkFont(L"Segoe UI Semibold", 12, FW_SEMIBOLD);
    g_fIco       = MkFont(L"Segoe MDL2 Assets", 14, FW_NORMAL);
    g_fIcoSm     = MkFont(L"Segoe MDL2 Assets", 10, FW_NORMAL);
    g_fIcoBig    = MkFont(L"Segoe MDL2 Assets", 48, FW_NORMAL);

    HDC dcm = GetDC(NULL); HFONT om = SelectObject(dcm, g_fMono);
    TEXTMETRICW tm; GetTextMetricsW(dcm, &tm); g_monoH = tm.tmHeight;
    SelectObject(dcm, om); ReleaseDC(NULL, dcm);
}

// ---------------------------------------------------------------- validation
static BOOL HostOk(const WCHAR *h) {           // allowlist: blocks arg injection
    size_t n = wcslen(h);
    if (!n) return TRUE;
    for (size_t i = 0; i < n; i++) {
        WCHAR c = h[i];
        BOOL alnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if ((i == 0 || i == n - 1) && !alnum) return FALSE;
        if (!alnum && c != '-' && c != '.' && c != '_' && c != ':') return FALSE;
    }
    return TRUE;
}

// ---------------------------------------------------------------- query parse
// Prefix tables cover EN, PT-BR, ES, DE, FR, SV, NO, DA, RU. query.exe truncates
// the State column, so prefix matching on the localized value is required.
static const WCHAR *PFX_LISTEN[] = { L"LISTEN", L"ESCUTA", L"ESCUCH", L"\u00C9COUTE", L"LAUSCH", L"LYSSN", L"\u041F\u0420\u041E\u0421\u041B", 0 };
static const WCHAR *PFX_ACTIVE[] = { L"ACT", L"ATIV", L"AKT", L"\u0410\u041A\u0422", 0 };
static const WCHAR *PFX_DISC[]   = { L"DISC", L"DESC", L"DECO", L"GETR", L"FR\u00C5N", L"\u041E\u0422\u041A\u041B", 0 };

static BOOL HasPfx(const WCHAR *up, const WCHAR **tbl) {
    for (; *tbl; tbl++) if (!wcsncmp(up, *tbl, wcslen(*tbl))) return TRUE;
    return FALSE;
}
static void Slice(const WCHAR *line, int len, int a, int b, WCHAR *out, int cap) {
    if (a > len) a = len;
    if (b > len || b < 0) b = len;
    int n = b - a; if (n >= cap) n = cap - 1; if (n < 0) n = 0;
    wmemcpy(out, line + a, n); out[n] = 0;
    WCHAR *s = out, *e = out + n;                       // trim + drop '>' marker
    while (*s == ' ' || *s == '>') s++;
    while (e > s && (e[-1] == ' ' || e[-1] == '\r')) e--;
    *e = 0; if (s > out) wmemmove(out, s, e - s + 1);
}
static int IFind(const WCHAR *hay, const WCHAR *needle) {
    WCHAR up[512]; int i = 0;
    for (; hay[i] && i < 511; i++) up[i] = towupper(hay[i]);
    up[i] = 0;
    const WCHAR *p = wcsstr(up, needle);
    return p ? (int)(p - up) : -1;
}
static WCHAR *NextLine(WCHAR **cur) {                  // in-place \n splitter
    WCHAR *s = *cur;
    while (*s == '\n') s++;
    if (!*s) return NULL;
    WCHAR *e = wcschr(s, '\n');
    if (e) { *e = 0; *cur = e + 1; } else *cur = s + wcslen(s);
    return s;
}
static int ParseSessions(WCHAR *out, Session *dst) {   // -> count
    int n = 0;
    WCHAR *cur = out, *line = NextLine(&cur);
    if (!line) return 0;
    int iU = IFind(line, L"USERNAME"), iI = IFind(line, L"ID"), iS = IFind(line, L"STATE");
    if (iU < 0 || iI < 0 || iS < 0) return 0;
    while ((line = NextLine(&cur)) && n < MAXS) {
        int len = (int)wcslen(line);
        if (len <= iS) continue;
        Session *s = &dst[n];
        WCHAR idbuf[16], st[48];
        Slice(line, len, 0,  iU, s->name, 64);
        Slice(line, len, iU, iI, s->user, 64);
        Slice(line, len, iI, iS, idbuf,   16);
        Slice(line, len, iS, -1, st,      48);
        WCHAR *sp = wcschr(st, ' '); if (sp) *sp = 0;   // first token only
        WCHAR up[48]; int k = 0;
        for (; st[k] && k < 47; k++) up[k] = towupper(st[k]);
        up[k] = 0;
        if (HasPfx(up, PFX_LISTEN)) continue;           // listener rows not shadowable
        WCHAR *end; long id = wcstol(idbuf, &end, 10);
        if (end == idbuf || *end) continue;
        wcscpy(s->raw, st);
        s->id = (int)id;
        s->st = HasPfx(up, PFX_ACTIVE) ? ST_ACTIVE : HasPfx(up, PFX_DISC) ? ST_DISC : ST_OTHER;
        n++;
    }
    return n;
}

// ---------------------------------------------------------------- rows / sort
static int CmpSes(const void *a, const void *b) {
    const Session *x = a, *y = b;
    int rx = x->st != ST_ACTIVE, ry = y->st != ST_ACTIVE;
    return rx != ry ? rx - ry : _wcsicmp(x->user, y->user);
}
static void BuildRows(void) {
    qsort(g_ses, g_nses, sizeof(Session), CmpSes);
    g_nrows = 0; int lastGrp = -1;
    for (int i = 0; i < g_nses; i++) {
        int grp = g_ses[i].st != ST_ACTIVE;
        if (grp != lastGrp) {
            g_rows[g_nrows].group = TRUE;
            g_rows[g_nrows].label = grp ? T[S_GOTHER] : T[S_GACTIVE];
            g_rows[g_nrows++].si  = -1;
            lastGrp = grp;
        }
        g_rows[g_nrows].group = FALSE;
        g_rows[g_nrows++].si  = i;
    }
    g_sel = g_hot = -1; g_scroll = 0;
}

// ---------------------------------------------------------------- workers
#define WM_QUERY_DONE  (WM_APP + 1)   // wParam: 0 ok, 1 error, 2 timeout
#define WM_SHADOW_DONE (WM_APP + 2)   // wParam: 0 ok, 1 error

typedef struct { HANDLE out, err; char *bo, *be; DWORD no, ne; } Pipes;
#define CAPBUF (256 * 1024)

static DWORD WINAPI PipeReader(LPVOID p) {     // sequential: stderr is tiny
    Pipes *pp = p; DWORD rd;
    while (ReadFile(pp->out, pp->bo + pp->no, CAPBUF - 1 - pp->no, &rd, NULL) && rd) pp->no += rd;
    while (ReadFile(pp->err, pp->be + pp->ne, 4095 - pp->ne, &rd, NULL) && rd) pp->ne += rd;
    return 0;
}
static BOOL Spawn(WCHAR *cmd, BOOL withPipes, PROCESS_INFORMATION *pi, Pipes *pp) {
    STARTUPINFOW si = { .cb = sizeof si };
    HANDLE wo = NULL, we = NULL;
    if (withPipes) {
        SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
        CreatePipe(&pp->out, &wo, &sa, 0); SetHandleInformation(pp->out, HANDLE_FLAG_INHERIT, 0);
        CreatePipe(&pp->err, &we, &sa, 0); SetHandleInformation(pp->err, HANDLE_FLAG_INHERIT, 0);
        si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = wo; si.hStdError = we;
    }
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, withPipes, CREATE_NO_WINDOW, NULL, NULL, &si, pi);
    if (wo) CloseHandle(wo);                   // close our write ends -> reader gets EOF
    if (we) CloseHandle(we);
    return ok;
}
static DWORD WINAPI QueryWorker(LPVOID unused) {
    (void)unused;
    WCHAR host[256], cmd[320];
    EnterCriticalSection(&g_cs); wcscpy(host, g_host); LeaveCriticalSection(&g_cs);
    swprintf(cmd, 320, host[0] ? L"query.exe session /server:%ls" : L"query.exe session", host);

    Pipes pp = {0};
    pp.bo = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, CAPBUF);
    pp.be = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096);
    PROCESS_INFORMATION pi;
    if (!Spawn(cmd, TRUE, &pi, &pp)) {
        EnterCriticalSection(&g_cs);
        swprintf(g_perr, 512, T[S_EQSTART], GetLastError());
        LeaveCriticalSection(&g_cs);
        CloseHandle(pp.out); CloseHandle(pp.err);
        HeapFree(GetProcessHeap(), 0, pp.bo); HeapFree(GetProcessHeap(), 0, pp.be);
        PostMessageW(g_wnd, WM_QUERY_DONE, 1, 0);
        return 0;
    }
    HANDLE rd = CreateThread(NULL, 0, PipeReader, &pp, 0, NULL);
    BOOL timeout = WaitForSingleObject(pi.hProcess, 15000) == WAIT_TIMEOUT;
    if (timeout) TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(rd, 5000); CloseHandle(rd);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    CloseHandle(pp.out); CloseHandle(pp.err);

    // console output arrives in the OEM codepage -> decode to UTF-16
    WCHAR *wout = HeapAlloc(GetProcessHeap(), 0, CAPBUF * sizeof(WCHAR));
    int wn = MultiByteToWideChar(CP_OEMCP, 0, pp.bo, pp.no, wout, CAPBUF - 1);
    wout[wn] = 0;
    WCHAR werr[512];
    int en = MultiByteToWideChar(CP_OEMCP, 0, pp.be, pp.ne, werr, 511);
    werr[en] = 0;
    while (en && (werr[en - 1] == '\n' || werr[en - 1] == '\r' || werr[en - 1] == ' ')) werr[--en] = 0;
    // stderr is multi-line; the bar is one line -> collapse runs of whitespace
    int wr = 0;
    for (int i = 0; i < en; i++) {
        WCHAR c = werr[i] == '\r' || werr[i] == '\n' || werr[i] == '\t' ? ' ' : werr[i];
        if (c == ' ' && (wr == 0 || werr[wr - 1] == ' ')) continue;
        werr[wr++] = c;
    }
    werr[wr] = 0; en = wr;

    BOOL blank = TRUE;
    for (WCHAR *c = wout; *c; c++) if (*c > ' ') { blank = FALSE; break; }

    EnterCriticalSection(&g_cs);
    WPARAM res;
    if (timeout) { wcscpy(g_perr, T[S_ETIMEOUT]); res = 2; }
    else if (code != 0 && blank) {             // query.exe exits 1 even on partial success
        if (en) wcscpy(g_perr, werr);
        else swprintf(g_perr, 512, T[S_EQEXIT], code);
        res = 1;
    } else { g_pn = ParseSessions(wout, g_pses); res = 0; }
    LeaveCriticalSection(&g_cs);

    HeapFree(GetProcessHeap(), 0, pp.bo); HeapFree(GetProcessHeap(), 0, pp.be);
    HeapFree(GetProcessHeap(), 0, wout);
    PostMessageW(g_wnd, WM_QUERY_DONE, res, 0);
    return 0;
}
static int g_shadowId; static BOOL g_shadowCtrl;
static DWORD WINAPI ShadowWorker(LPVOID unused) {
    (void)unused;
    WCHAR host[256], cmd[384];
    EnterCriticalSection(&g_cs); wcscpy(host, g_host); LeaveCriticalSection(&g_cs);
    swprintf(cmd, 384, L"mstsc.exe /shadow:%d /v:%ls /noConsentPrompt%ls",
             g_shadowId, host[0] ? host : L"localhost", g_shadowCtrl ? L" /control" : L"");
    PROCESS_INFORMATION pi; Pipes dummy;
    if (!Spawn(cmd, FALSE, &pi, &dummy)) {
        EnterCriticalSection(&g_cs);
        swprintf(g_perr, 512, T[S_EMSTART], GetLastError());
        LeaveCriticalSection(&g_cs);
        PostMessageW(g_wnd, WM_SHADOW_DONE, 1, 0);
        return 0;
    }
    // early exit inside 3s with a non-zero code == shadow setup failure;
    // still running after 3s == the shadow session is up
    WPARAM res = 0; DWORD code = 0;
    if (WaitForSingleObject(pi.hProcess, 3000) == WAIT_OBJECT_0 &&
        GetExitCodeProcess(pi.hProcess, &code) && code != 0) {
        EnterCriticalSection(&g_cs);
        swprintf(g_perr, 512, T[S_EMEXIT], code);
        LeaveCriticalSection(&g_cs);
        res = 1;
    }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    PostMessageW(g_wnd, WM_SHADOW_DONE, res, 0);
    return 0;
}

// ---------------------------------------------------------------- actions
static void CalcLayout(void);                  // defined below (needs Layout)

static void SetBusy(BOOL on) {
    g_busy = on;
    if (on) { g_spin = 0; SetTimer(g_wnd, 1, 16, NULL); }
    else KillTimer(g_wnd, 1);
    InvalidateRect(g_wnd, NULL, FALSE);
}
static void DoRefresh(void) {
    if (g_busy) return;
    WCHAR host[256];
    GetWindowTextW(g_edit, host, 256);
    if (!HostOk(host)) {
        g_err = TRUE;
        wcscpy(g_errmsg, T[S_EINVALID]);
        wcscpy(g_status, T[S_NOTHING]);
        CalcLayout();                          // error bar changes the row heights
        InvalidateRect(g_wnd, NULL, FALSE);
        return;
    }
    if (g_err) { g_err = FALSE; CalcLayout(); }
    EnterCriticalSection(&g_cs); wcscpy(g_host, host); LeaveCriticalSection(&g_cs);
    wcscpy(g_status, T[S_QUERYING]);
    SetBusy(TRUE);
    CloseHandle(CreateThread(NULL, 0, QueryWorker, NULL, 0, NULL));
}
static Session *SelSession(void) {
    if (g_sel < 0 || g_sel >= g_nrows || g_rows[g_sel].group) return NULL;
    return &g_ses[g_rows[g_sel].si];
}
static void DoShadow(void) {
    Session *s = SelSession();
    if (g_busy || !s || !s->user[0]) return;
    g_err = FALSE;
    g_shadowId = s->id; g_shadowCtrl = g_ctrlMode;
    swprintf(g_status, 256, T[S_STARTING], s->user, s->id);
    SetBusy(TRUE);
    CloseHandle(CreateThread(NULL, 0, ShadowWorker, NULL, 0, NULL));
}
static void UpdateFooter(void) {
    int act = 0;
    for (int i = 0; i < g_nses; i++) act += g_ses[i].st == ST_ACTIVE;
    WCHAR host[256];
    EnterCriticalSection(&g_cs); wcscpy(host, g_host); LeaveCriticalSection(&g_cs);
    swprintf(g_footer, 128, T[S_FOOTER], act, g_nses - act, host[0] ? host : L"localhost");
}

// ---------------------------------------------------------------- layout
static int RowsHeight(void) {
    int h = 0;
    for (int i = 0; i < g_nrows; i++) h += g_rows[i].group ? L.grpH : L.rowH;
    return h;
}
static void CalcLayout(void) {
    RECT rc; GetClientRect(g_wnd, &rc);
    int w = rc.right, h = rc.bottom, pad = S(14), bh = S(34), cap = S(40);

    L.title  = (RECT){ 0, 0, w, cap };
    L.bClose = (RECT){ w - S(46), 0, w, cap };
    L.bMin   = (RECT){ w - S(92), 0, w - S(46), cap };

    int y = cap + S(10);
    int wShadow = S(158), wRefresh = S(104), wChk = S(78), gap = S(10);
    L.bShadow  = (RECT){ w - pad - wShadow, y, w - pad, y + bh };
    L.chk      = (RECT){ L.bShadow.left - gap - wChk, y, L.bShadow.left - gap, y + bh };
    L.bRefresh = (RECT){ L.chk.left - gap - wRefresh, y, L.chk.left - gap, y + bh };
    L.field    = (RECT){ pad, y, L.bRefresh.left - gap, y + bh };
    y += bh + S(10);

    if (g_err) { L.errbar = (RECT){ pad, y, w - pad, y + S(36) }; y = L.errbar.bottom + S(10); }
    else L.errbar = (RECT){0};

    L.status = (RECT){ 0, h - S(37), w, h };
    L.list   = (RECT){ pad, y, w - pad, L.status.top - S(12) };
    L.headH  = S(32); L.rowH = S(36); L.grpH = S(30);
    L.inner  = (RECT){ L.list.left + 1, L.list.top + L.headH + 1, L.list.right - 1, L.list.bottom - 1 };

    int vis = L.inner.bottom - L.inner.top;
    g_scrollMax = RowsHeight() - vis; if (g_scrollMax < 0) g_scrollMax = 0;
    if (g_scroll > g_scrollMax) g_scroll = g_scrollMax;

    if (g_edit) {
        // Size the edit to the font's real line height and center it in the field.
        // A single-line EDIT top-aligns its text, so height == text height is what
        // makes the text land centered (a fixed magic height does not).
        int eh = g_monoH;
        MoveWindow(g_edit, L.field.left + S(12), L.field.top + (bh - eh) / 2,
                   L.field.right - L.field.left - S(24), eh, TRUE);
    }
}
// row geometry: returns top y of row i (client coords, scrolled)
static int RowY(int i) {
    int y = L.inner.top - g_scroll;
    for (int k = 0; k < i; k++) y += g_rows[k].group ? L.grpH : L.rowH;
    return y;
}
static int RowAt(int px, int py) {
    if (px < L.inner.left || px >= L.inner.right || py < L.inner.top || py >= L.inner.bottom) return -1;
    int y = L.inner.top - g_scroll;
    for (int i = 0; i < g_nrows; i++) {
        int rh = g_rows[i].group ? L.grpH : L.rowH;
        if (py >= y && py < y + rh) return g_rows[i].group ? -1 : i;
        y += rh;
    }
    return -1;
}
static int CtlAt(int x, int y) {
    POINT p = { x, y };
    if (PtInRect(&L.bMin, p))   return HT_MIN;
    if (PtInRect(&L.bClose, p)) return HT_CLOSE;
    if (PtInRect(&L.bRefresh, p)) return HT_REFRESH;
    if (PtInRect(&L.bShadow, p))  return HT_SHADOW;
    if (PtInRect(&L.chk, p))      return HT_CHECK;
    if (g_scrollMax > 0 && x >= L.list.right - S(14) && x < L.list.right &&
        y >= L.inner.top && y < L.inner.bottom) return HT_SCROLL;
    return HT_NONE;
}
static void ScrollTo(int v) {
    if (v < 0) v = 0;
    if (v > g_scrollMax) v = g_scrollMax;
    if (v != g_scroll) { g_scroll = v; InvalidateRect(g_wnd, NULL, FALSE); }
}
// scrollbar thumb: track height, thumb height, thumb top (all in client coords)
static void ThumbGeom(int *vis, int *th, int *ty) {
    *vis = L.inner.bottom - L.inner.top;
    *th  = g_scrollMax > 0 ? *vis * *vis / (g_scrollMax + *vis) : *vis;
    if (*th < S(28)) *th = S(28);
    if (*th > *vis)  *th = *vis;               // keeps (vis - th) a safe divisor
    *ty = L.inner.top + (g_scrollMax > 0 && *vis > *th
        ? (int)((*vis - *th) * (double)g_scroll / g_scrollMax) : 0);
}
static void ScrollToThumb(int y) {             // drag/jump: thumb top -> offset
    int vis, th, ty; ThumbGeom(&vis, &th, &ty);
    if (vis > th) ScrollTo((y - L.inner.top) * g_scrollMax / (vis - th));
}
static void EnsureVisible(int row) {
    if (row < 0) return;
    int top = RowY(row), bot = top + L.rowH;
    if (top < L.inner.top) ScrollTo(g_scroll - (L.inner.top - top));
    else if (bot > L.inner.bottom) ScrollTo(g_scroll + (bot - L.inner.bottom));
}

// ---------------------------------------------------------------- painting
static void PaintButton(HDC dc, GpGraphics *g, RECT r, int id, const WCHAR *glyph,
                        const WCHAR *label, BOOL primary, BOOL disabled) {
    UINT32 bg, fg;
    BOOL hot = g_hotCtl == id, down = g_downCtl == id && hot;
    if (disabled)     { bg = 0x2A2A2A;  fg = CLR_DIS; }
    else if (primary) { bg = down ? CLR_ACC_P : hot ? CLR_ACC_H : CLR_ACC; fg = 0x101010; }
    else              { bg = down ? CLR_PRESS : hot ? CLR_HOV : CLR_CTRL;  fg = 0xEBEBEB; }
    RoundFill(g, r, (float)S(6), A(bg));
    if (!primary && !disabled) RoundStroke(g, r, (float)S(6), A(CLR_STROKE), 1);
    HFONT ft = primary ? g_fSemi : g_fBody;
    int wg = TxtW(dc, g_fIco, glyph), wt = TxtW(dc, ft, label);
    int x = (r.left + r.right - wg - S(8) - wt) / 2;
    RECT rg = { x, r.top, x + wg, r.bottom }, rt = { x + wg + S(8), r.top, r.right, r.bottom };
    TxtR(dc, g_fIco, fg, rg, DT_VCENTER, glyph);
    TxtR(dc, ft, fg, rt, DT_VCENTER, label);
}
static void Paint(HDC dc) {
    RECT rc; GetClientRect(g_wnd, &rc);
    FillR(dc, rc, CLR_BG);
    GpGraphics *g; GdipCreateFromHDC(dc, &g);
    GdipSetSmoothingMode(g, 4);

    // -- title bar
    RECT rIco = { S(16), 0, S(36), L.title.bottom };
    TxtR(dc, g_fIco, CLR_ACC, rIco, DT_VCENTER, L"\uE7F4");
    RECT rTit = { S(42), 0, L.bMin.left, L.title.bottom };
    TxtR(dc, g_fTitle, CLR_TXT, rTit, DT_VCENTER, W_APP);
    struct { RECT r; int id; const WCHAR *gl; } cap[] = {
        { L.bMin, HT_MIN, L"\uE921" },
        { L.bClose, HT_CLOSE, L"\uE8BB" },
    };
    for (int i = 0; i < 2; i++) {
        BOOL hot = g_hotCtl == cap[i].id, down = g_downCtl == cap[i].id && hot;
        if (hot) FillR(dc, cap[i].r, cap[i].id == HT_CLOSE ? CLR_CLOSE : down ? CLR_PRESS : 0x2D2D2D);
        TxtR(dc, g_fIcoSm, hot && cap[i].id == HT_CLOSE ? 0xFFFFFF : hot ? CLR_TXT : CLR_TXT2,
             cap[i].r, DT_CENTER | DT_VCENTER, cap[i].gl);
    }

    // -- toolbar: hostname field + buttons
    RoundFill(g, L.field, (float)S(6), A(CLR_FIELD));
    RoundStroke(g, L.field, (float)S(6), A(g_editFocus ? CLR_ACC : CLR_STROKE), g_editFocus ? 1.6f : 1.0f);
    PaintButton(dc, g, L.bRefresh, HT_REFRESH, L"\uE72C", T[S_REFRESH], FALSE, g_busy);
    Session *sel = SelSession();
    PaintButton(dc, g, L.bShadow, HT_SHADOW, L"\uE8A7", T[S_SHADOW], TRUE,
                g_busy || !sel || !sel->user[0]);
    // checkbox
    int cbs = S(18), cy = (L.chk.top + L.chk.bottom) / 2;
    RECT box = { L.chk.left, cy - cbs / 2, L.chk.left + cbs, cy + cbs / 2 };
    if (g_ctrlMode) {
        RoundFill(g, box, (float)S(4), A(CLR_ACC));
        TxtR(dc, g_fIcoSm, 0x101010, box, DT_CENTER | DT_VCENTER, L"\uE73E");
    } else {
        RoundFill(g, box, (float)S(4), A(g_hotCtl == HT_CHECK ? 0x3A3A3A : 0x313131));
        RoundStroke(g, box, (float)S(4), A(0x8A8A8A), 1);
    }
    RECT rcl = { box.right + S(8), L.chk.top, L.chk.right, L.chk.bottom };
    TxtR(dc, g_fBody, 0xEBEBEB, rcl, DT_VCENTER, T[S_CONTROL]);

    // -- error bar
    if (g_err) {
        RoundFill(g, L.errbar, (float)S(6), A(CLR_ERRBG));
        RoundStroke(g, L.errbar, (float)S(6), A(0x5A3434), 1);
        RECT ri = { L.errbar.left + S(12), L.errbar.top, L.errbar.left + S(30), L.errbar.bottom };
        TxtR(dc, g_fIco, CLR_ERRFG, ri, DT_VCENTER, L"\uE783");
        int we = TxtW(dc, g_fSemi, T[S_ERROR]);
        RECT rt2 = { ri.right + S(4), L.errbar.top, ri.right + S(4) + we, L.errbar.bottom };
        TxtR(dc, g_fSemi, CLR_TXT, rt2, DT_VCENTER, T[S_ERROR]);
        RECT rm = { rt2.right + S(10), L.errbar.top, L.errbar.right - S(12), L.errbar.bottom };
        TxtR(dc, g_fSmall, 0xF0DEDE, rm, DT_VCENTER | DT_END_ELLIPSIS, g_errmsg);
    }

    // -- list card
    RoundFill(g, L.list, (float)S(8), A(CLR_CARD));
    RoundStroke(g, L.list, (float)S(8), A(CLR_EDGE), 1);
    int x0 = L.list.left + S(16);
    int xstate = L.list.right - S(126);
    int xid_r = xstate - S(28), xid_l = xid_r - S(52);
    int x1 = x0 + S(148), x1r = xid_l - S(12);
    RECT rh = { 0, L.list.top, 0, L.list.top + L.headH };
    struct { int l, r; UINT fl; const WCHAR *t; } hdr[] = {
        { x0, x1 - S(8), DT_LEFT, T[S_HSESSION] }, { x1, x1r, DT_LEFT, T[S_HUSER] },
        { xid_l, xid_r, DT_RIGHT, T[S_HID] }, { xstate, L.list.right - S(16), DT_LEFT, T[S_HSTATE] },
    };
    for (int i = 0; i < 4; i++) {
        rh.left = hdr[i].l; rh.right = hdr[i].r;
        TxtR(dc, g_fSmallSemi, CLR_TXT3, rh, hdr[i].fl | DT_VCENTER, hdr[i].t);
    }
    RECT sep = { L.list.left + 1, L.list.top + L.headH, L.list.right - 1, L.list.top + L.headH + 1 };
    FillR(dc, sep, CLR_EDGE);

    if (!g_nrows) {                            // empty state
        int cyE = (L.inner.top + L.inner.bottom) / 2;
        RECT re1 = { L.list.left, cyE - S(64), L.list.right, cyE - S(4) };
        RECT re2 = { L.list.left, cyE + S(2),  L.list.right, cyE + S(24) };
        RECT re3 = { L.list.left, cyE + S(24), L.list.right, cyE + S(44) };
        TxtR(dc, g_fIcoBig, 0x565656, re1, DT_CENTER | DT_VCENTER, L"\uE7F4");
        TxtR(dc, g_fBody, CLR_TXT2, re2, DT_CENTER | DT_VCENTER, T[S_EMPTY1]);
        TxtR(dc, g_fSmall, CLR_TXT3, re3, DT_CENTER | DT_VCENTER,
             g_queried ? T[S_EMPTY3] : T[S_EMPTY2]);
    } else {
        SaveDC(dc);
        IntersectClipRect(dc, L.inner.left, L.inner.top, L.inner.right, L.inner.bottom);
        int y = L.inner.top - g_scroll;
        for (int i = 0; i < g_nrows; i++) {
            int rhh = g_rows[i].group ? L.grpH : L.rowH;
            if (y + rhh > L.inner.top && y < L.inner.bottom) {
                if (g_rows[i].group) {
                    RECT rg2 = { x0, y + S(6), x1r, y + rhh };
                    TxtR(dc, g_fSmallSemi, 0x8A8A8A, rg2, DT_LEFT | DT_VCENTER, g_rows[i].label);
                } else {
                    Session *s = &g_ses[g_rows[i].si];
                    RECT rr = { L.list.left + S(6), y + S(2), L.list.right - S(6), y + rhh - S(2) };
                    if (i == g_sel) {
                        RoundFill(g, rr, (float)S(4), A(CLR_HOV));
                        RECT pill = { rr.left + S(1), (y + rhh / 2) - S(8), rr.left + S(4), (y + rhh / 2) + S(8) };
                        RoundFill(g, pill, 1.5f, A(CLR_ACC));
                    } else if (i == g_hot) RoundFill(g, rr, (float)S(4), A(0x2D2D2D));
                    RECT c1 = { x0, y, x1 - S(8), y + rhh }, c2 = { x1, y, x1r, y + rhh };
                    RECT c3 = { xid_l, y, xid_r, y + rhh }, c4 = { xstate + S(16), y, L.list.right - S(16), y + rhh };
                    WCHAR idb[16]; swprintf(idb, 16, L"%d", s->id);
                    TxtR(dc, g_fMono, CLR_TXT2, c1, DT_VCENTER | DT_END_ELLIPSIS, s->name);
                    TxtR(dc, g_fBody, CLR_TXT, c2, DT_VCENTER | DT_END_ELLIPSIS, s->user[0] ? s->user : L"\u2014");
                    TxtR(dc, g_fMono, CLR_TXT2, c3, DT_VCENTER | DT_RIGHT, idb);
                    Dot(g, (float)(xstate + S(5)), y + rhh / 2.0f, (float)S(4),
                        A(s->st == ST_ACTIVE ? CLR_GREEN : s->st == ST_DISC ? CLR_AMBER : CLR_GRAY));
                    TxtR(dc, g_fSmall, CLR_TXT2, c4, DT_VCENTER | DT_END_ELLIPSIS,
                         s->st == ST_ACTIVE ? T[S_STACTIVE] : s->st == ST_DISC ? T[S_STDISC] : s->raw);
                }
            }
            y += rhh;
        }
        RestoreDC(dc, -1);
        if (g_scrollMax > 0) {                 // overlay scrollbar
            int vis, th, ty; ThumbGeom(&vis, &th, &ty);
            RECT sb = { L.list.right - S(9), ty, L.list.right - S(5), ty + th };
            RoundFill(g, sb, (float)S(2), A(g_dragSb ? 0x7A7A7A : 0x575757));
        }
    }

    // -- status bar
    RECT hl = { 0, L.status.top, rc.right, L.status.top + 1 };
    FillR(dc, hl, CLR_EDGE);
    int sx = S(14), scy = (L.status.top + L.status.bottom) / 2;
    if (g_busy) {
        GpPen *pen; GdipCreatePen1(A(CLR_ACC), (float)S(2) * 0.9f, 2, &pen);
        float rad = (float)S(7);
        GdipDrawArc(g, pen, sx, scy - rad, rad * 2, rad * 2, g_spin, 270);
        GdipDeletePen(pen);
        sx += S(24);
    }
    RECT rs = { sx, L.status.top, rc.right / 2 + S(60), L.status.bottom };
    TxtR(dc, g_fSmall, CLR_TXT2, rs, DT_VCENTER | DT_END_ELLIPSIS, g_status);
    RECT rf = { rc.right / 2, L.status.top, rc.right - S(14), L.status.bottom };
    TxtR(dc, g_fSmall, CLR_TXT3, rf, DT_VCENTER | DT_RIGHT, g_footer);

    GdipDeleteGraphics(g);
}

// ---------------------------------------------------------------- edit subclass
static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && (w == VK_RETURN || w == VK_F5)) { DoRefresh(); return 0; }
    if (m == WM_KEYDOWN && w == VK_TAB) { SetFocus(g_wnd); return 0; }
    if (m == WM_CHAR && (w == '\r' || w == '\t')) return 0;
    return CallWindowProcW(g_editProc, h, m, w, l);
}

// ---------------------------------------------------------------- window proc
static void SelectMove(int dir) {
    int i = g_sel;
    do i += dir; while (i >= 0 && i < g_nrows && g_rows[i].group);
    if (i < 0 || i >= g_nrows) return;
    g_sel = i; EnsureVisible(i);
    InvalidateRect(g_wnd, NULL, FALSE);
}
static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_NCCALCSIZE:
        if (w) return 0;                       // no default frame -> custom title bar
        break;
    case WM_NCHITTEST: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        ScreenToClient(h, &p);
        if (p.y < L.title.bottom && CtlAt(p.x, p.y) == HT_NONE) return HTCAPTION;
        return HTCLIENT;
    }
    case WM_NCACTIVATE: return TRUE;
    case WM_NCLBUTTONDBLCLK: return 0;         // fixed size: no dblclick-maximize
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO: {                   // fixed size: pin min == max
        MINMAXINFO *mi = (MINMAXINFO*)l;
        mi->ptMinTrackSize = mi->ptMaxTrackSize = mi->ptMaxSize = (POINT){ S(720), S(520) };
        return 0;
    }
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) { CalcLayout(); InvalidateRect(h, NULL, FALSE); }
        return 0;
    case WM_DPICHANGED: {
        g_dpi = HIWORD(w);
        MakeFonts();
        SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_fMono, TRUE);
        RECT *r = (RECT*)l;
        SetWindowPos(h, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC wdc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        HDC dc = CreateCompatibleDC(wdc);
        HBITMAP bmp = CreateCompatibleBitmap(wdc, rc.right, rc.bottom);
        HBITMAP ob = SelectObject(dc, bmp);
        Paint(dc);
        BitBlt(wdc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
        SelectObject(dc, ob); DeleteObject(bmp); DeleteDC(dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)w, REF(CLR_FIELD));
        SetTextColor((HDC)w, REF(CLR_TXT));
        return (LRESULT)g_brField;
    case WM_COMMAND:
        if ((HWND)l == g_edit) {
            if (HIWORD(w) == EN_SETFOCUS)  { g_editFocus = TRUE;  InvalidateRect(h, NULL, FALSE); }
            if (HIWORD(w) == EN_KILLFOCUS) { g_editFocus = FALSE; InvalidateRect(h, NULL, FALSE); }
        }
        return 0;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        if (!g_track) {
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t); g_track = TRUE;
        }
        if (g_dragSb) { ScrollToThumb(y - g_dragOff); return 0; }
        int ctl = CtlAt(x, y), row = RowAt(x, y);
        if (ctl != g_hotCtl || row != g_hot) {
            g_hotCtl = ctl; g_hot = row;
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_track = FALSE;
        if (g_hotCtl || g_hot >= 0) { g_hotCtl = HT_NONE; g_hot = -1; InvalidateRect(h, NULL, FALSE); }
        return 0;
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        SetCapture(h);
        g_downCtl = CtlAt(x, y);
        if (g_downCtl == HT_SCROLL) {
            int vis, th, ty; ThumbGeom(&vis, &th, &ty);
            BOOL onThumb = y >= ty && y < ty + th;
            g_dragSb  = TRUE;
            g_dragOff = onThumb ? y - ty : th / 2;
            if (!onThumb) ScrollToThumb(y - g_dragOff);   // jump to click position
        } else if (g_downCtl == HT_NONE) {
            POINT p = { x, y };
            if (PtInRect(&L.field, p)) { SetFocus(g_edit); return 0; }
            int row = RowAt(x, y);
            if (row >= 0 && row != g_sel) g_sel = row;
            SetFocus(h);
        }
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        ReleaseCapture();
        g_dragSb = FALSE;
        int ctl = CtlAt(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        int down = g_downCtl; g_downCtl = HT_NONE;
        if (ctl == down) switch (ctl) {
            case HT_MIN:     ShowWindow(h, SW_MINIMIZE); break;
            case HT_CLOSE:   PostMessageW(h, WM_CLOSE, 0, 0); break;
            case HT_REFRESH: DoRefresh(); break;
            case HT_SHADOW:  DoShadow(); break;
            case HT_CHECK:   g_ctrlMode = !g_ctrlMode; break;
        }
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        int row = RowAt(GET_X_LPARAM(l), GET_Y_LPARAM(l));
        if (row >= 0) { g_sel = row; DoShadow(); }
        return 0;
    }
    case WM_MOUSEWHEEL:
        ScrollTo(g_scroll - GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * L.rowH * 3);
        return 0;
    case WM_KEYDOWN:
        switch (w) {
        case VK_F5:     DoRefresh(); break;
        case VK_TAB:    SetFocus(g_edit); break;
        case VK_RETURN: DoShadow(); break;
        case VK_UP:     SelectMove(-1); break;
        case VK_DOWN:   SelectMove(1); break;
        case VK_PRIOR:  ScrollTo(g_scroll - (L.inner.bottom - L.inner.top)); break;
        case VK_NEXT:   ScrollTo(g_scroll + (L.inner.bottom - L.inner.top)); break;
        }
        return 0;
    case WM_TIMER:
        g_spin += 6; if (g_spin >= 360) g_spin -= 360;
        InvalidateRect(h, &L.status, FALSE);
        return 0;
    case WM_QUERY_DONE: {
        EnterCriticalSection(&g_cs);
        if (w == 0) { g_nses = g_pn; memcpy(g_ses, g_pses, g_pn * sizeof(Session)); }
        else wcscpy(g_errmsg, g_perr);
        LeaveCriticalSection(&g_cs);
        if (w == 0) {
            BuildRows(); UpdateFooter();
            g_queried = TRUE; g_err = FALSE; g_status[0] = 0;
        } else {
            g_err = TRUE; g_nses = 0; BuildRows();
            g_footer[0] = 0;
            wcscpy(g_status, T[S_QFAILED]);
        }
        SetBusy(FALSE); CalcLayout();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_SHADOW_DONE:
        if (w == 0) wcscpy(g_status, T[S_LAUNCHED]);
        else {
            EnterCriticalSection(&g_cs); wcscpy(g_errmsg, g_perr); LeaveCriticalSection(&g_cs);
            g_err = TRUE;
            wcscpy(g_status, T[S_SFAILED]);
        }
        SetBusy(FALSE); CalcLayout();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------------------------------------------------------- entry
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE prev, PWSTR args, int show) {
    (void)prev; (void)args;
    if (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_PORTUGUESE) T = STR_PT;
    InitializeCriticalSection(&g_cs);
    ULONG_PTR tok; GdipStartupInput gsi = { .ver = 1 };
    GdiplusStartup(&tok, &gsi, NULL);
    g_brField = CreateSolidBrush(REF(CLR_FIELD));

    HICON classIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));   // embedded app.ico
    WNDCLASSW wc = { CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, WndProc, 0, 0, hi,
                     classIcon, LoadCursorW(NULL, IDC_ARROW), NULL, NULL, L"RdpShadowWnd" };
    RegisterClassW(&wc);

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    HDC sdc = GetDC(NULL);
    g_dpi = GetDeviceCaps(sdc, LOGPIXELSX);
    ReleaseDC(NULL, sdc);
    int ww = S(720), wh = S(520);
    // no WS_CAPTION: stops Windows painting a ghost caption on inactive windows
    // (basic frame under RDP). THICKFRAME only buys the DWM shadow - resizing
    // is blocked by WM_GETMINMAXINFO and the hit test never returns edges.
    g_wnd = CreateWindowExW(0, wc.lpszClassName, W_APP,
        WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX,
        (wa.left + wa.right - ww) / 2, (wa.top + wa.bottom - wh) / 2, ww, wh,
        NULL, NULL, hi, NULL);
    g_dpi = GetDpiForWindow(g_wnd);
    MakeFonts();

    // dark titlebar area + rounded corners + frame shadow (all best-effort)
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (dwm) {
        HRESULT (WINAPI *setAttr)(HWND, DWORD, LPCVOID, DWORD) =
            (void*)GetProcAddress(dwm, "DwmSetWindowAttribute");
        HRESULT (WINAPI *extend)(HWND, const void*) =
            (void*)GetProcAddress(dwm, "DwmExtendFrameIntoClientArea");
        if (setAttr) {
            BOOL dark = TRUE; DWORD corner = 2;
            if (setAttr(g_wnd, 20, &dark, sizeof dark) != 0) setAttr(g_wnd, 19, &dark, sizeof dark);
            setAttr(g_wnd, 33, &corner, sizeof corner);      // Win11 rounded corners
        }
        if (extend) { struct { int l, r, t, b; } mg = { 0, 0, 1, 0 }; extend(g_wnd, &mg); }
    }
    // taskbar / Alt-Tab icons: pull the best-fit frames from the embedded .ico
    HICON icoBig = LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON icoSm = LoadImageW(hi, MAKEINTRESOURCEW(1), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    SendMessageW(g_wnd, WM_SETICON, ICON_BIG, (LPARAM)(icoBig ? icoBig : classIcon));
    SendMessageW(g_wnd, WM_SETICON, ICON_SMALL, (LPARAM)(icoSm ? icoSm : classIcon));

    g_edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        0, 0, 10, 10, g_wnd, NULL, hi, NULL);
    SendMessageW(g_edit, WM_SETFONT, (WPARAM)g_fMono, TRUE);
    SendMessageW(g_edit, 0x1501 /*EM_SETCUEBANNER*/, TRUE, (LPARAM)T[S_CUE]);
    g_editProc = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)EditProc);

    wcscpy(g_status, T[S_READY]);
    CalcLayout();
    ShowWindow(g_wnd, show);
    SetFocus(g_edit);
    DoRefresh();                               // list localhost right away

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
