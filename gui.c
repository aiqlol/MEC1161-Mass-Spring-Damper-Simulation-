#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "msd.h"
#include "data.h"

#define NFIELDS 9                                          // number of input boxes
#define MAX_GUI_STEPS 2000000.0                            // largest run the window allows

enum { ID_PRESET = 100, ID_METHOD, ID_PLOTVAR, ID_RUN, ID_EXPORT, ID_PLAY, ID_EDIT0 = 200 };  // control IDs
#define ID_TIMER 1                                         // animation timer
#define ANIM_H 170                                         // height of the animation strip (pixels)
enum { F_M, F_C, F_K, F_X0, F_V0, F_F0, F_W, F_T, F_DT }; // index of each input box

static const char *LABELS[NFIELDS] = {
    "Mass m (kg)", "Damping c (N*s/m)", "Stiffness k (N/m)",
    "Initial displacement x0 (m)", "Initial velocity v0 (m/s)",
    "Force amplitude F0 (N)", "Force frequency w (rad/s)",
    "Total time t_total (s)", "Time step dt (s)"
};

typedef struct { const char *name; double v[NFIELDS]; } Preset;   // one ready-made case
static const Preset PRESETS[] = {                          //  m    c      k   x0 v0 F0  w      t   dt
    {"Custom (type your own values)",    {1, 0.5,   20, 1, 0, 0, 0,     10, 0.01}},
    {"Free vibration (c = 0)",           {1, 0,     20, 1, 0, 0, 0,     10, 0.01}},
    {"Underdamped (c = 1)",              {1, 1,     20, 1, 0, 0, 0,     10, 0.01}},
    {"Critically damped (c = 8.944)",    {1, 8.944, 20, 1, 0, 0, 0,     10, 0.01}},
    {"Overdamped (c = 25)",              {1, 25,    20, 1, 0, 0, 0,     10, 0.01}},
    {"Forced, off resonance (w = 2)",    {1, 0.5,   20, 0, 0, 4, 2,     30, 0.01}},
    {"Forced, at resonance (w = 4.472)", {1, 0.5,   20, 0, 0, 4, 4.472, 30, 0.01}},
};
#define NPRESETS ((int)(sizeof PRESETS / sizeof PRESETS[0]))

static HWND hEdit[NFIELDS], hPreset, hMethod, hPlotVar, hInfo, hStatus, hPlay;   // control handles
static SimulationData results;                             // results of the last run
static int have_results = 0;                               // 1 after a successful run
static int status_error = 0;                               // 1 = show status in red
static int filling = 0;                                    // 1 while a preset fills the boxes
static char run_method[40] = "";                           // method used for the last run
static RECT plotRect;                                      // where the graph is drawn
static Params run_p;                                       // inputs of the last run
static double run_xmax = 0;                                // largest |x| of the last run
static int anim_frame = 0;                                 // state shown in the animation
static int playing = 0;                                    // 1 while the animation runs
static DWORD anim_start;                                   // clock time (ms) that matches t = 0

static HWND make(HWND parent, const char *cls, const char *text, DWORD style,
                 int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExA(strcmp(cls, "EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, cls, text,
                             WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                             (HMENU)(INT_PTR)id, GetModuleHandleA(NULL), NULL);   // create one control
    SendMessageA(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);   // standard font
    return c;
}

static void set_status(const char *msg, int is_error)
{
    status_error = is_error;                               // colour used when the label repaints
    SetWindowTextA(hStatus, msg);                          // show the message
    InvalidateRect(hStatus, NULL, TRUE);                   // repaint in the new colour
}

static int read_field(int i, double *out)
{
    char buf[64], *end;
    GetWindowTextA(hEdit[i], buf, sizeof buf);             // text in box i
    char *s = buf;
    while (isspace((unsigned char)*s)) s++;                // skip leading spaces
    if (*s == '\0') return 0;                              // empty box
    double v = strtod(s, &end);                            // convert to a number
    while (isspace((unsigned char)*end)) end++;            // skip trailing spaces
    if (end == s || *end != '\0' || !isfinite(v)) return 0;   // not a clean number
    *out = v;
    return 1;
}

static int read_params(Params *p, char *msg, size_t len)
{
    double v[NFIELDS];
    for (int i = 0; i < NFIELDS; i++)                      // every box must hold a number
        if (!read_field(i, &v[i])) { snprintf(msg, len, "\"%s\" must be a number.", LABELS[i]); return 0; }
    if (v[F_M] <= 0)  { snprintf(msg, len, "Mass m must be greater than 0."); return 0; }
    if (v[F_C] < 0)   { snprintf(msg, len, "Damping c cannot be negative."); return 0; }
    if (v[F_K] <= 0)  { snprintf(msg, len, "Stiffness k must be greater than 0."); return 0; }
    if (v[F_F0] < 0)  { snprintf(msg, len, "Force amplitude F0 cannot be negative."); return 0; }
    if (v[F_F0] > 0 && v[F_W] <= 0) { snprintf(msg, len, "Force frequency w must be greater than 0 when F0 > 0."); return 0; }
    if (v[F_T] <= 0)  { snprintf(msg, len, "Total time must be greater than 0."); return 0; }
    if (v[F_DT] <= 0 || v[F_DT] > v[F_T]) { snprintf(msg, len, "Time step dt must be > 0 and no larger than the total time."); return 0; }
    if (v[F_T] / v[F_DT] > MAX_GUI_STEPS) { snprintf(msg, len, "Too many steps. Use a larger dt or a shorter total time."); return 0; }
    p->m = v[F_M]; p->c = v[F_C]; p->k = v[F_K];           // pack the inputs
    p->x0 = v[F_X0]; p->v0 = v[F_V0];
    p->F0 = v[F_F0]; p->w = (v[F_F0] > 0) ? v[F_W] : 0.0;  // no force: frequency unused
    p->t_total = v[F_T]; p->dt = v[F_DT];
    return 1;
}

static void update_info(void)
{
    double m, c, k, F0, w;
    char buf[256];
    if (!read_field(F_M, &m) || !read_field(F_C, &c) || !read_field(F_K, &k) || m <= 0 || c < 0 || k <= 0) {
        SetWindowTextA(hInfo, "Enter m > 0, c >= 0 and k > 0 to see\nthe natural frequency and damping regime.");
        return;
    }
    double wn = sqrt(k / m);                               // natural frequency
    double zeta = c / (2.0 * sqrt(m * k));                 // damping ratio
    const char *regime = (c == 0) ? "UNDAMPED (free vibration)"
                       : (fabs(zeta - 1.0) < 0.01) ? "CRITICALLY DAMPED"
                       : (zeta < 1.0) ? "UNDERDAMPED" : "OVERDAMPED";
    int n = snprintf(buf, sizeof buf, "wn = %.4f rad/s     zeta = %.4f\nRegime: %s", wn, zeta, regime);
    if (read_field(F_F0, &F0) && F0 > 0 && read_field(F_W, &w) && w > 0)   // forced case
        snprintf(buf + n, sizeof buf - n, "\nForced: w / wn = %.3f%s", w / wn,
                 fabs(w / wn - 1.0) < 0.05 ? "  (RESONANCE)" : "");
    SetWindowTextA(hInfo, buf);                            // show the summary
}

static void fill_preset(int idx)
{
    char buf[32];
    filling = 1;                                           // stop the boxes switching to "Custom"
    for (int i = 0; i < NFIELDS; i++) {
        snprintf(buf, sizeof buf, "%g", PRESETS[idx].v[i]);   // value as text
        SetWindowTextA(hEdit[i], buf);                     // put it in box i
    }
    filling = 0;
    update_info();                                         // refresh the summary
}

static void stop_anim(HWND hwnd)
{
    if (playing) KillTimer(hwnd, ID_TIMER);                // stop the ticks
    playing = 0;
    SetWindowTextA(hPlay, "Play animation");
}

static void start_anim(HWND hwnd)
{
    if (!have_results) { set_status("Run a simulation first.", 1); return; }
    if (anim_frame >= (int)results.count - 1) anim_frame = 0;   // at the end: start again
    double t_now = results.states[anim_frame].t - results.states[0].t;   // time already shown
    anim_start = GetTickCount() - (DWORD)(t_now * 1000.0); // clock time that matches t = 0
    SetTimer(hwnd, ID_TIMER, 30, NULL);                    // about 33 frames per second
    playing = 1;
    SetWindowTextA(hPlay, "Stop");
}

static void anim_tick(HWND hwnd)
{
    double el = (GetTickCount() - anim_start) / 1000.0;    // real seconds since t = 0
    int f = (int)(el / run_p.dt + 0.5);                    // matching state number
    if (f >= (int)results.count - 1) { f = (int)results.count - 1; stop_anim(hwnd); }   // last state: stop
    anim_frame = f;
    InvalidateRect(hwnd, &plotRect, FALSE);                // redraw animation and graph
}

static void run_simulation(HWND hwnd)
{
    Params p;
    char msg[160];
    stop_anim(hwnd);                                       // a new run stops the old animation
    if (!read_params(&p, msg, sizeof msg)) { set_status(msg, 1); return; }   // bad input
    int n = num_steps(&p) + 1;                             // states including t = 0
    if (!simulation_data_allocate(&results, (size_t)n)) { set_status("Not enough memory for this run.", 1); return; }
    int euler = (SendMessageA(hMethod, CB_GETCURSEL, 0, 0) == 1);   // 0 = RK4, 1 = Euler
    simulate(results.states, n, &p, euler ? step_euler : step_rk4);   // run with the chosen solver
    strcpy(run_method, euler ? "Euler" : "RK4");
    have_results = 1;
    double xmax = 0;
    for (int i = 0; i < n; i++)                            // largest displacement
        if (fabs(results.states[i].x) > xmax) xmax = fabs(results.states[i].x);
    run_p = p;                                             // kept for the animation
    run_xmax = xmax;
    anim_frame = 0;                                        // animation starts at t = 0
    snprintf(msg, sizeof msg, "Done: %d points with %s.\nx at the end = %.6f m, largest |x| = %.6f m",
             n, run_method, results.states[n - 1].x, xmax);
    set_status(msg, 0);
    InvalidateRect(hwnd, &plotRect, FALSE);                // redraw the graph
}

static void export_csv(void)
{
    if (!have_results) { set_status("Run a simulation first.", 1); return; }
    if (simulation_data_export_csv(&results, "simulation_results.csv"))   // Syed's CSV writer
        set_status("Saved simulation_results.csv in the program's folder.", 0);
    else
        set_status("Could not save simulation_results.csv. Is it open in Excel?", 1);
}

static double value_of(const State *s, int var)
{
    return var == 0 ? s->x : var == 1 ? s->v : s->a;       // displacement, velocity or acceleration
}

static void nice_label(char *buf, size_t len, double v)
{
    if (fabs(v) < 1e-12) v = 0;                            // print 0, not -1e-17
    snprintf(buf, len, "%.3g", v);
}

static void line(HDC dc, int x0, int y0, int x1, int y1)
{
    MoveToEx(dc, x0, y0, NULL);                            // start point
    LineTo(dc, x1, y1);                                    // end point
}

static void draw_anim(HDC dc, int W)
{
    const State *s = &results.states[anim_frame];          // state to show
    int gy = ANIM_H - 30;                                  // ground level
    int cy = gy - 25;                                      // centre height of the block
    int wall = 30;                                         // wall position
    int restL = W / 2 - 30;                                // block left edge when x = 0
    int A = (int)(0.4 * (restL - wall));                   // pixels for the largest |x|
    int bx = restL + (run_xmax > 0 ? (int)(s->x / run_xmax * A) : 0);   // block left edge now
    char buf[96];
    HPEN thick = CreatePen(PS_SOLID, 3, RGB(40, 40, 40));
    HPEN thin = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    HPEN spring = CreatePen(PS_SOLID, 2, RGB(0, 90, 200));
    HPEN dash = CreatePen(PS_DASH, 1, RGB(110, 110, 110));
    HPEN red = CreatePen(PS_SOLID, 2, RGB(200, 0, 0));
    HBRUSH blockFill = CreateSolidBrush(RGB(170, 200, 240));
    SelectObject(dc, thin);
    for (int i = wall - 10; i < W - 20; i += 10) line(dc, i, gy + 10, i + 10, gy);   // ground hatching
    for (int i = gy - 80; i < gy; i += 10) line(dc, wall - 10, i + 10, wall, i);     // wall hatching
    SelectObject(dc, thick);
    line(dc, wall, gy - 80, wall, gy);                     // wall
    line(dc, wall, gy, W - 20, gy);                        // ground
    SelectObject(dc, dash);
    line(dc, restL, gy - 70, restL, gy + 8);               // equilibrium mark
    TextOutA(dc, restL - 12, gy + 10, "x = 0", 5);
    SelectObject(dc, spring);                              // spring: zigzag above the centre
    int sy = cy - 12;
    line(dc, wall, sy, wall + 10, sy);                     // straight end at the wall
    MoveToEx(dc, wall + 10, sy, NULL);
    for (int i = 1; i <= 14; i++)                          // 14 zigzag segments
        LineTo(dc, wall + 10 + (bx - 10 - wall - 10) * i / 14, i == 14 ? sy : sy + ((i % 2) ? -8 : 8));
    LineTo(dc, bx, sy);                                    // straight end at the block
    SelectObject(dc, thin);                                // damper: below the centre
    int dy = cy + 12;
    int c0 = wall + 10;                                    // cylinder start
    int c1 = wall + (int)(0.4 * (restL + A - wall)) + 4;   // cylinder end (piston never leaves it)
    int px = wall + (int)(0.4 * (bx - wall));              // piston follows the block
    line(dc, wall, dy, c0, dy);                            // rod from the wall
    line(dc, c0, dy - 8, c1, dy - 8);                      // cylinder top
    line(dc, c0, dy + 8, c1, dy + 8);                      // cylinder bottom
    line(dc, c0, dy - 8, c0, dy + 8);                      // cylinder closed end
    SelectObject(dc, thick);
    line(dc, px, dy - 6, px, dy + 6);                      // piston
    SelectObject(dc, thin);
    line(dc, px, dy, bx, dy);                              // piston rod to the block
    SelectObject(dc, thick);
    SelectObject(dc, blockFill);
    Rectangle(dc, bx, gy - 50, bx + 60, gy);               // the mass
    TextOutA(dc, bx + 26, gy - 32, "m", 1);
    if (run_p.F0 > 0) {                                    // force arrow above the block
        double F = run_p.F0 * sin(run_p.w * s->t);         // F(t) = F0 sin(wt)
        int len = (int)(F / run_p.F0 * 50);                // arrow length: 50 px at F = F0
        int ax = bx + 30, ay = gy - 62;
        SelectObject(dc, red);
        line(dc, ax, ay, ax + len, ay);                    // arrow shaft
        if (len > 4 || len < -4) {                         // arrow head in the force direction
            int h = len > 0 ? -7 : 7;
            line(dc, ax + len, ay, ax + len + h, ay - 5);
            line(dc, ax + len, ay, ax + len + h, ay + 5);
        }
        TextOutA(dc, ax - 12, ay - 20, "F(t)", 4);
    }
    snprintf(buf, sizeof buf, "t = %.2f s     x = %+.4f m", s->t, s->x);
    TextOutA(dc, 10, 8, buf, (int)strlen(buf));            // time and position now
    const char *note = "Motion scaled to fit the panel";
    TextOutA(dc, W - 190, 8, note, (int)strlen(note));
    SelectObject(dc, GetStockObject(BLACK_PEN));
    SelectObject(dc, GetStockObject(WHITE_BRUSH));
    line(dc, 0, ANIM_H - 1, W, ANIM_H - 1);                // line between animation and graph
    DeleteObject(thick); DeleteObject(thin); DeleteObject(spring);
    DeleteObject(dash); DeleteObject(red); DeleteObject(blockFill);
}

static void draw_plot(HDC dc, int W, int H)
{
    RECT all = {0, 0, W, H};
    FillRect(dc, &all, (HBRUSH)GetStockObject(WHITE_BRUSH));   // white background
    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dc, TRANSPARENT);
    if (!have_results) {
        DrawTextA(dc, "Choose a case or type values, then press \"Run simulation\".", -1, &all,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    draw_anim(dc, W);                                      // animation strip at the top
    int var = (int)SendMessageA(hPlotVar, CB_GETCURSEL, 0, 0);   // which quantity to plot
    static const char *TITLES[3] = {"Displacement x (m)", "Velocity v (m/s)", "Acceleration a (m/s^2)"};
    int n = (int)results.count;
    double t0 = results.states[0].t, t1 = results.states[n - 1].t;
    double lo = value_of(&results.states[0], var), hi = lo;
    for (int i = 1; i < n; i++) {                          // range of the plotted quantity
        double y = value_of(&results.states[i], var);
        if (y < lo) lo = y;
        if (y > hi) hi = y;
    }
    if (hi - lo < 1e-12) { hi += 1; lo -= 1; }             // flat line: give it some height
    double pad = 0.05 * (hi - lo); hi += pad; lo -= pad;   // small margin above and below
    if (t1 <= t0) t1 = t0 + 1;
    int L = 70, R = W - 20, T = ANIM_H + 35, B = H - 45;   // graph area below the animation
    char buf[64];
    snprintf(buf, sizeof buf, "%s vs time  (%s)", TITLES[var], run_method);
    TextOutA(dc, L, ANIM_H + 10, buf, (int)strlen(buf));   // graph title
    HPEN grid = CreatePen(PS_DOT, 1, RGB(200, 200, 200));
    HPEN axis = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    HPEN line = CreatePen(PS_SOLID, 2, RGB(0, 90, 200));
    SelectObject(dc, grid);
    for (int g = 0; g <= 5; g++) {                         // 6 grid lines each way, with labels
        int gx = L + (R - L) * g / 5, gy = B - (B - T) * g / 5;
        MoveToEx(dc, gx, T, NULL); LineTo(dc, gx, B);
        MoveToEx(dc, L, gy, NULL); LineTo(dc, R, gy);
        nice_label(buf, sizeof buf, t0 + (t1 - t0) * g / 5);
        TextOutA(dc, gx - 10, B + 6, buf, (int)strlen(buf));   // time label
        nice_label(buf, sizeof buf, lo + (hi - lo) * g / 5);
        TextOutA(dc, 8, gy - 7, buf, (int)strlen(buf));    // value label
    }
    TextOutA(dc, (L + R) / 2 - 30, B + 24, "Time t (s)", 10);   // x-axis title
    SelectObject(dc, axis);
    SelectObject(dc, GetStockObject(NULL_BRUSH));         // hollow frame, keeps the grid
    Rectangle(dc, L, T, R + 1, B + 1);                     // frame around the graph
    if (lo < 0 && hi > 0) {                                // zero line
        int zy = B - (int)((0 - lo) / (hi - lo) * (B - T));
        MoveToEx(dc, L, zy, NULL); LineTo(dc, R, zy);
    }
    SelectObject(dc, line);
    int stride = n > 8000 ? n / 8000 : 1;                  // limit points drawn on long runs
    for (int i = 0; i < n; i += stride) {                  // the curve
        int px = L + (int)((results.states[i].t - t0) / (t1 - t0) * (R - L));
        int py = B - (int)((value_of(&results.states[i], var) - lo) / (hi - lo) * (B - T));
        if (i == 0) MoveToEx(dc, px, py, NULL); else LineTo(dc, px, py);
    }
    HPEN mark = CreatePen(PS_SOLID, 1, RGB(200, 0, 0));
    SelectObject(dc, mark);                                // red line at the animation time
    int mx = L + (int)((results.states[anim_frame].t - t0) / (t1 - t0) * (R - L));
    MoveToEx(dc, mx, T, NULL); LineTo(dc, mx, B);
    SelectObject(dc, GetStockObject(BLACK_PEN));
    DeleteObject(grid); DeleteObject(axis); DeleteObject(line); DeleteObject(mark);
}

static void layout(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    plotRect.left = 340; plotRect.top = 45;                // right-hand panel
    plotRect.right = rc.right - 12; plotRect.bottom = rc.bottom - 12;
    if (plotRect.right < plotRect.left + 100) plotRect.right = plotRect.left + 100;
    if (plotRect.bottom < plotRect.top + 100) plotRect.bottom = plotRect.top + 100;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        HWND lbl;
        lbl = make(hwnd, "STATIC", "Case:", 0, 12, 14, 40, 20, 0);
        (void)lbl;
        hPreset = make(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 55, 10, 270, 300, ID_PRESET);
        for (int i = 0; i < NPRESETS; i++)
            SendMessageA(hPreset, CB_ADDSTRING, 0, (LPARAM)PRESETS[i].name);   // list the cases
        SendMessageA(hPreset, CB_SETCURSEL, 0, 0);
        for (int i = 0; i < NFIELDS; i++) {                // one label + box per input
            make(hwnd, "STATIC", LABELS[i], 0, 12, 52 + i * 30, 185, 20, 0);
            hEdit[i] = make(hwnd, "EDIT", "", ES_AUTOHSCROLL | WS_TABSTOP, 200, 48 + i * 30, 125, 24, ID_EDIT0 + i);
        }
        make(hwnd, "STATIC", "Method", 0, 12, 52 + NFIELDS * 30, 185, 20, 0);
        hMethod = make(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 200, 48 + NFIELDS * 30, 125, 200, ID_METHOD);
        SendMessageA(hMethod, CB_ADDSTRING, 0, (LPARAM)"RK4 (recommended)");
        SendMessageA(hMethod, CB_ADDSTRING, 0, (LPARAM)"Forward Euler");
        SendMessageA(hMethod, CB_SETCURSEL, 0, 0);
        hInfo = make(hwnd, "STATIC", "", SS_LEFT, 12, 90 + NFIELDS * 30, 315, 50, 0);
        make(hwnd, "BUTTON", "Run simulation", BS_DEFPUSHBUTTON | WS_TABSTOP, 12, 148 + NFIELDS * 30, 150, 32, ID_RUN);
        make(hwnd, "BUTTON", "Export CSV", BS_PUSHBUTTON | WS_TABSTOP, 175, 148 + NFIELDS * 30, 150, 32, ID_EXPORT);
        hStatus = make(hwnd, "STATIC", "Pick a case from the list, or type your own values.", SS_LEFT,
                       12, 192 + NFIELDS * 30, 315, 60, 0);
        make(hwnd, "STATIC", "Show:", 0, 340, 14, 40, 20, 0);
        hPlotVar = make(hwnd, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_TABSTOP, 382, 10, 200, 200, ID_PLOTVAR);
        SendMessageA(hPlotVar, CB_ADDSTRING, 0, (LPARAM)"Displacement x");
        SendMessageA(hPlotVar, CB_ADDSTRING, 0, (LPARAM)"Velocity v");
        SendMessageA(hPlotVar, CB_ADDSTRING, 0, (LPARAM)"Acceleration a");
        SendMessageA(hPlotVar, CB_SETCURSEL, 0, 0);
        hPlay = make(hwnd, "BUTTON", "Play animation", BS_PUSHBUTTON | WS_TABSTOP, 600, 9, 130, 26, ID_PLAY);
        simulation_data_init(&results);                    // empty results store
        fill_preset(0);                                    // start with the Custom values
        layout(hwnd);
        return 0;
    }
    case WM_SIZE:
        layout(hwnd);                                      // graph follows the window size
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)lp)->ptMinTrackSize.x = 800;        // smallest window allowed
        ((MINMAXINFO *)lp)->ptMinTrackSize.y = 640;
        return 0;
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if ((id == ID_RUN && code == BN_CLICKED) || id == IDOK) run_simulation(hwnd);   // button or Enter key
        else if (id == ID_EXPORT && code == BN_CLICKED) export_csv();
        else if (id == ID_PLAY && code == BN_CLICKED) { if (playing) stop_anim(hwnd); else start_anim(hwnd); }
        else if (id == ID_PRESET && code == CBN_SELCHANGE) fill_preset((int)SendMessageA(hPreset, CB_GETCURSEL, 0, 0));
        else if (id == ID_PLOTVAR && code == CBN_SELCHANGE) InvalidateRect(hwnd, &plotRect, FALSE);
        else if (id >= ID_EDIT0 && id < ID_EDIT0 + NFIELDS && code == EN_CHANGE && !filling) {
            SendMessageA(hPreset, CB_SETCURSEL, 0, 0);     // typing makes it a Custom case
            update_info();
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == ID_TIMER && playing) anim_tick(hwnd);    // next animation frame
        return 0;
    case WM_CTLCOLORSTATIC:
        if ((HWND)lp == hStatus) {                         // status text: red for errors
            SetTextColor((HDC)wp, status_error ? RGB(190, 0, 0) : RGB(0, 110, 0));
            SetBkMode((HDC)wp, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        int W = plotRect.right - plotRect.left, H = plotRect.bottom - plotRect.top;
        HDC mem = CreateCompatibleDC(dc);                  // draw off-screen to avoid flicker
        HBITMAP bmp = CreateCompatibleBitmap(dc, W, H);
        HGDIOBJ old = SelectObject(mem, bmp);
        draw_plot(mem, W, H);
        BitBlt(dc, plotRect.left, plotRect.top, W, H, mem, 0, 0, SRCCOPY);   // copy to the window
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        stop_anim(hwnd);                                   // stop the timer
        simulation_data_free(&results);                    // release the results array
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show)
{
    (void)hPrev; (void)cmd;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WndProc;                              // message handler
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);        // standard grey background
    wc.lpszClassName = "MSDWindow";
    if (!RegisterClassA(&wc)) return 1;
    HWND hwnd = CreateWindowExA(0, "MSDWindow", "Mass-Spring-Damper Simulator",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                                1100, 720, NULL, NULL, hInst, NULL);   // main window
    if (hwnd == NULL) return 1;
    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);
    MSG m;
    while (GetMessageA(&m, NULL, 0, 0) > 0) {              // message loop
        if (!IsDialogMessageA(hwnd, &m)) {                 // Tab moves between boxes
            TranslateMessage(&m);
            DispatchMessageA(&m);
        }
    }
    return (int)m.wParam;
}
