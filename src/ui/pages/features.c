/* features.c - generic category pages (System, Network, Cleanup, Security, Tools):
   feature tabs, options, input row, Run/Stop, output log and launcher grids */
#include "ui/internal.h"
#include "ui/pages/pages.h"
#include <shlobj.h>
#include <commdlg.h>
#include <shellapi.h>
#include <process.h>

static const int k_fp_of_page[PG_COUNT] = { -1, -1, -1, FP_SYSTEM, -1, FP_NETWORK, FP_SECURITY, -1, -1, FP_CLEANUP, FP_TOOLS, -1, -1 };
static int  fp_sel[FP_COUNT];
static int  fp_opt[FP_COUNT][12];
static wchar_t* fp_text[FP_COUNT][12];
static int  g_grid_scroll = 0;

static int cur_fp(void) { return (g_page >= 0 && g_page < PG_COUNT) ? k_fp_of_page[g_page] : -1; }

static const FeatureDef* cur_feat(int fp)
{
    int n;
    const FeatureDef* f = features_for(fp, &n);
    if (!f || fp_sel[fp] < 0 || fp_sel[fp] >= n) return NULL;
    return &f[fp_sel[fp]];
}

static wchar_t* fp_text_slot(int fp, int idx)
{
    if (!fp_text[fp][idx]) {
        int n;
        const FeatureDef* f = features_for(fp, &n);
        wchar_t tmp[MAX_PATH * 2];
        tmp[0] = 0;
        if (f && idx < n && f[idx].in_default) expand_env(f[idx].in_default, tmp, MAX_PATH * 2);
        fp_text[fp][idx] = (wchar_t*)calloc(MAX_PATH * 2, sizeof(wchar_t));
        if (fp_text[fp][idx]) wcopy(fp_text[fp][idx], MAX_PATH * 2, tmp);
    }
    return fp_text[fp][idx];
}

typedef struct FpLay {
    RECT pill[30]; int npill;
    RECT desc;
    RECT opt[12]; int nopt;
    RECT label, edit, browse;
    RECT run, stop, clear, copy, save, prog;
    RECT body;
    BOOL has_input, is_grid;
} FpLay;

static void fp_layout(int fp, FpLay* L)
{
    int n, i, x, y, h;
    const FeatureDef* defs = features_for(fp, &n);
    const FeatureDef* f = cur_feat(fp);
    int left = g_cont.left, right = g_cont.right;
    ZeroMemory(L, sizeof(*L));
    x = left; y = g_cont.top;
    h = S(32);
    for (i = 0; i < n && i < 30; i++) {
        int w = w_text_width(feat_name(&defs[i]), F_UI_B) + S(30);
        if (x + w > right && x > left) { x = left; y += h + S(8); }
        L->pill[i] = rc_make(x, y, x + w, y + h);
        x += w + S(8);
    }
    L->npill = i;
    y += h + S(12);
    L->desc = rc_make(left, y, right, y + S(22));
    y += S(30);
    L->is_grid = f && f->grid;
    if (f && f->nopt > 0) {
        x = left;
        for (i = 0; i < f->nopt && i < 12; i++) {
            int w = w_text_width(T2(f->opt_en[i], f->opt_id[i]), F_UI_S) + S(26);
            if (x + w > right && x > left) { x = left; y += S(34); }
            L->opt[i] = rc_make(x, y, x + w, y + S(28));
            x += w + S(6);
        }
        L->nopt = i;
        y += S(28) + S(12);
    }
    if (f && f->in_kind != IN_NONE && !L->is_grid) {
        int lw = w_text_width(T2(f->in_label_en, f->in_label_id), F_UI_S) + S(14);
        int ew = MT_MIN(S(460), right - left - lw - ((f->in_kind == IN_FILE || f->in_kind == IN_FOLDER) ? S(120) : 0));
        L->has_input = TRUE;
        L->label = rc_make(left, y, left + lw, y + S(36));
        L->edit = rc_make(left + lw, y, left + lw + ew, y + S(36));
        if (f->in_kind == IN_FILE || f->in_kind == IN_FOLDER) L->browse = rc_make(left + lw + ew + S(8), y, left + lw + ew + S(8) + S(108), y + S(36));
        y += S(36) + S(12);
    }
    if (!L->is_grid) {
        int bx = left;
        L->run = rc_make(bx, y, bx + S(120), y + S(36));
        L->stop = rc_make(bx + S(128), y, bx + S(128) + S(104), y + S(36));
        L->save = rc_make(right - S(36), y, right, y + S(36));
        L->copy = rc_make(right - S(36) * 2 - S(6), y, right - S(36) - S(6), y + S(36));
        L->clear = rc_make(right - S(36) * 3 - S(12), y, right - S(36) * 2 - S(12), y + S(36));
        L->prog = rc_make(bx + S(248), y + S(15), L->clear.left - S(16), y + S(21));
        y += S(36) + S(12);
    }
    L->body = rc_make(left, y, right, g_cont.bottom);
    if (L->body.bottom < L->body.top + S(80)) L->body.bottom = L->body.top + S(80);
}

static void fp_select(int fp, int idx)
{
    const FeatureDef* f;
    wchar_t buf[MAX_PATH * 2];
    fp_sel[fp] = idx;
    f = cur_feat(fp);
    g_grid_scroll = 0;
    if (f && f->in_kind != IN_NONE) {
        wchar_t* t = fp_text_slot(fp, idx);
        wcopy(buf, MAX_PATH * 2, t ? t : L"");
        edit_set(g_ed_feat, buf);
    }
    if (f && !f->grid && !job_busy(fp)) {
        log_clear(fp);
        log_append(fp, OS_TITLE, L"\n  ");
        log_append(fp, OS_TITLE, feat_name(f));
        log_append(fp, OS_DIM, L"\n  ");
        log_append(fp, OS_DIM, feat_desc(f));
        log_append(fp, OS_DIM, T2(L"\n\n  Press Run to start.\n", L"\n\n  Tekan Jalankan untuk memulai.\n"));
    }
    pages_layout();
    g_dirty = TRUE;
}

typedef struct FeatJob { const FeatureDef* f; FeatArgs a; int slot; } FeatJob;

static unsigned __stdcall feat_thread(void* p)
{
    FeatJob* j = (FeatJob*)p;
    MtOut o;
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    out_init(&o, j->slot, j->slot, &g_jobs[j->slot].cancel);
    j->f->run(&o, &j->a);
    out_progress(&o, -1);
    CoUninitialize();
    PostMessageW(g_hwnd, WM_APP_DONE, (WPARAM)j->slot, 0);
    free(j);
    return 0;
}

static BOOL g_dev_run = FALSE;
static int g_dev_slot = -1;
static wchar_t g_dev_out[MAX_PATH];

static void fp_run(int fp)
{
    const FeatureDef* f = cur_feat(fp);
    FeatJob* j;
    if (!f || !f->run || job_busy(fp)) return;
    if ((f->flags & FF_CONFIRM) && !g_dev_run && !confirm_box(T2(f->confirm_en, f->confirm_id))) return;
    j = (FeatJob*)calloc(1, sizeof(FeatJob));
    if (!j) return;
    j->f = f; j->slot = fp;
    j->a.opt = fp_opt[fp][fp_sel[fp]];
    if (f->in_kind != IN_NONE) edit_get(g_ed_feat, j->a.text, MT_ARRAY(j->a.text));
    log_clear(fp);
    if (!job_run(fp, feat_thread, j)) free(j);
    g_dirty = TRUE;
}

static void fp_save_log(int fp)
{
    OPENFILENAMEW ofn;
    wchar_t path[MAX_PATH] = L"multitool_output.txt";
    wchar_t* t;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text (*.txt)\0*.txt\0All files\0*.*\0";
    ofn.lpstrDefExt = L"txt";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER;
    if (!GetSaveFileNameW(&ofn)) return;
    t = log_text(fp);
    {
        size_t i, n = wcslen(t);
        Buf b;
        HANDLE h;
        buf_init(&b);
        buf_add(&b, "\xEF\xBB\xBF", 3);
        for (i = 0; i < n; i++) {
            wchar_t one[2];
            char* u;
            if (t[i] == L'\r') { buf_add(&b, "\r\n", 2); continue; }
            one[0] = t[i]; one[1] = 0;
            u = w_to_utf8(one);
            if (u) { buf_add(&b, u, strlen(u)); free(u); }
        }
        h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD w;
            WriteFile(h, b.data, (DWORD)b.len, &w, NULL);
            CloseHandle(h);
            ui_toast(0, T2(L"Saved: %ls", L"Disimpan: %ls"), path);
        } else ui_toast(4, T2(L"Cannot write the file.", L"Tidak dapat menulis file."));
        buf_free(&b);
    }
    free(t);
}

static void grid_launch(const GridItem* it)
{
    HINSTANCE r = ShellExecuteW(NULL, L"open", it->cmd, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32 && it->params) r = ShellExecuteW(NULL, L"open", it->params, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)r > 32) ui_toast(0, T2(L"%ls opened", L"%ls dibuka"), T2(it->en, it->id));
    else ui_toast(4, T2(L"Could not open %ls (not available on this PC?)", L"Gagal membuka %ls (mungkin tidak tersedia di PC ini)"), T2(it->en, it->id));
}

static void paint_feature_page(int fp)
{
    FpLay L;
    int n, i;
    const FeatureDef* defs = features_for(fp, &n);
    const FeatureDef* f = cur_feat(fp);
    BOOL busy = job_busy(fp);
    fp_layout(fp, &L);
    for (i = 0; i < L.npill; i++) w_pill(&L.pill[i], feat_name(&defs[i]), HID_PILL0 + i, i == fp_sel[fp], F_UI_B);
    if (!f) return;
    gfx_text_rc(&L.desc, feat_desc(f), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    for (i = 0; i < L.nopt; i++) w_pill(&L.opt[i], T2(f->opt_en[i], f->opt_id[i]), HID_OPT0 + i, i == fp_opt[fp][fp_sel[fp]], F_UI_S);
    if (L.has_input) {
        gfx_text_rc(&L.label, T2(f->in_label_en, f->in_label_id), F_UI_S, g_th.dim, DT_SINGLELINE | DT_VCENTER);
        w_input_frame(&L.edit, GetFocus() == g_ed_feat);
        if (f->in_kind == IN_FILE || f->in_kind == IN_FOLDER) w_btn(&L.browse, T2(L"Browse", L"Telusuri"), ICO_FOLDER, HID_BROWSE, BTN_SOFT, 0);
    }
    if (L.is_grid) {
        int tw = S(236), th = S(46), gap = S(10), x, y, cols, k;
        RECT area = L.body;
        int total_h = 0, cy;
        gfx_clip(area.left, area.top, area.right, area.bottom);
        cols = MT_MAX(1, (area.right - area.left + gap) / (tw + gap));
        /* measure */
        cy = 0; k = 0;
        for (i = 0; i < f->ngrid; i++) {
            if (!f->grid[i].cmd) { if (k) { cy += th + gap; k = 0; } cy += S(32); }
            else { k++; if (k == cols) { cy += th + gap; k = 0; } }
        }
        if (k) cy += th + gap;
        total_h = cy;
        { int maxs = MT_MAX(0, total_h - (area.bottom - area.top)); if (g_grid_scroll > maxs) g_grid_scroll = maxs; if (g_grid_scroll < 0) g_grid_scroll = 0; }
        x = area.left; y = area.top - g_grid_scroll; k = 0;
        for (i = 0; i < f->ngrid; i++) {
            const GridItem* it = &f->grid[i];
            if (!it->cmd) {
                RECT hr;
                if (k) { y += th + gap; k = 0; }
                hr = rc_make(area.left, y + S(6), area.right, y + S(28));
                gfx_text_rc(&hr, T2(it->en, it->id), F_UI_SB, g_th.accent, DT_SINGLELINE | DT_VCENTER);
                y += S(32);
                x = area.left;
            } else {
                RECT tr = rc_make(x, y, x + tw, y + th), vis;
                int id = HID_TILE0 + i;
                float hov = g_hv[id];
                RECT trc = tr;
                if (tr.bottom > area.top && tr.top < area.bottom) {
                    gfx_rrect((float)tr.left, (float)tr.top, (float)tw, (float)th, (float)S(10), rgb_mix(g_th.card, g_th.card2, 0.3f + hov * 0.7f), 255);
                    gfx_rrect_stroke((float)tr.left, (float)tr.top, (float)tw, (float)th, (float)S(10), 1.0f, rgb_mix(g_th.border, g_th.accent, hov * 0.8f), 255);
                    gfx_circle((float)(tr.left + S(22)), (float)(tr.top + th / 2), (float)S(5), rgb_mix(g_th.muted, g_th.accent, hov), 255);
                    trc.left += S(40); trc.right -= S(10);
                    gfx_text_rc(&trc, T2(it->en, it->id), F_UI, rgb_mix(g_th.text, g_th.accent, hov * 0.4f), DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                    if (IntersectRect(&vis, &tr, &area)) hot_add(&vis, id);
                }
                k++;
                x += tw + gap;
                if (k == cols) { k = 0; x = area.left; y += th + gap; }
            }
        }
        gfx_unclip();
        return;
    }
    {
        BOOL stoppable = (f->flags & FF_STOPPABLE) != 0;
        w_btn(&L.run, busy ? T2(L"Running...", L"Berjalan...") : T2(L"Run", L"Jalankan"), ICO_RUN, HID_RUN, BTN_PRIMARY, busy ? BF_DISABLED : 0);
        if (stoppable || busy) w_btn(&L.stop, T2(L"Stop", L"Stop"), ICO_STOP, HID_STOP, BTN_DANGER, busy ? 0 : BF_DISABLED);
        w_btn(&L.clear, L"", ICO_CLEAR, HID_CLEAR, BTN_SOFT, 0);
        w_btn(&L.copy, L"", ICO_COPY, HID_COPY, BTN_SOFT, 0);
        w_btn(&L.save, L"", ICO_SAVE, HID_SAVE, BTN_SOFT, 0);
        if (busy) {
            int p10 = g_jobs[fp].progress10;
            if (L.prog.right > L.prog.left + S(40)) w_progress(&L.prog, p10 >= 0 ? p10 / 1000.0 : -1.0, g_th.accent);
        }
    }
    w_card_rc(&L.body);
}

/* ---------------------------------------------------------------- page interface */
int pg_features_fp(int page) { return (page >= 0 && page < PG_COUNT) ? k_fp_of_page[page] : -1; }

void pg_features_select(int fp, int idx) { fp_select(fp, idx); }

void pg_features_enter(void)
{
    int fp = cur_fp();
    g_grid_scroll = 0;
    if (fp >= 0 && fp_sel[fp] >= 0) fp_select(fp, fp_sel[fp]);   /* refreshes the input text + hint */
}

void pg_features_layout(void)
{
    int fp = cur_fp();
    FpLay L;
    const FeatureDef* f;
    if (fp < 0) return;
    f = cur_feat(fp);
    fp_layout(fp, &L);
    if (f && L.has_input) edit_place(g_ed_feat, &L.edit, TRUE);
    if (f && !f->grid) {
        RECT lr = rc_make(L.body.left + 2, L.body.top + 2, L.body.right - 2, L.body.bottom - 2);
        log_place(fp, &lr);
    }
}

void pg_features_paint(void)
{
    int fp = cur_fp();
    if (fp >= 0) paint_feature_page(fp);
}

BOOL pg_features_anim(void)
{
    int fp = cur_fp();
    return fp >= 0 && job_busy(fp);
}

BOOL pg_features_wheel(int delta)
{
    int fp = cur_fp();
    const FeatureDef* f = fp >= 0 ? cur_feat(fp) : NULL;
    if (f && f->grid) { g_grid_scroll -= delta / 120 * S(60); if (g_grid_scroll < 0) g_grid_scroll = 0; return TRUE; }
    return FALSE;
}

void pg_features_command(HWND ed)
{
    int fp = cur_fp();
    if (ed == g_ed_feat && fp >= 0) {
        wchar_t* slot = fp_text_slot(fp, fp_sel[fp]);
        if (slot) GetWindowTextW(g_ed_feat, slot, MAX_PATH * 2);
    }
}

BOOL pg_features_click(int id)
{
    int fp = cur_fp();
    if (fp < 0) return FALSE;
    if (id >= HID_PILL0 && id < HID_PILL0 + 30) { fp_select(fp, id - HID_PILL0); return TRUE; }
    if (id >= HID_OPT0 && id < HID_OPT0 + 16) { fp_opt[fp][fp_sel[fp]] = id - HID_OPT0; g_dirty = TRUE; return TRUE; }
    if (id >= HID_TILE0 && id < HID_TILE0 + 100) {
        const FeatureDef* f = cur_feat(fp);
        if (f && f->grid && id - HID_TILE0 < f->ngrid && f->grid[id - HID_TILE0].cmd) grid_launch(&f->grid[id - HID_TILE0]);
        return TRUE;
    }
    switch (id) {
    case HID_RUN: fp_run(fp); return TRUE;
    case HID_STOP: job_stop(fp); return TRUE;
    case HID_CLEAR: log_clear(fp); return TRUE;
    case HID_COPY: { wchar_t* t = log_text(fp); copy_to_clipboard(g_hwnd, t); free(t); ui_toast(0, T2(L"Output copied to clipboard", L"Output disalin ke clipboard")); return TRUE; }
    case HID_SAVE: fp_save_log(fp); return TRUE;
    case HID_BROWSE: {
        const FeatureDef* f = cur_feat(fp);
        wchar_t p[MAX_PATH * 2];
        if (f && f->in_kind == IN_FILE && pick_file(p, MT_ARRAY(p), T2(L"Select a file", L"Pilih file"))) edit_set(g_ed_feat, p);
        else if (f && f->in_kind == IN_FOLDER && pick_folder(p, MAX_PATH, T2(L"Select a folder", L"Pilih folder"))) edit_set(g_ed_feat, p);
        return TRUE; }
    default: return FALSE;
    }
}

/* ---------------------------------------------------------------- developer test hook (--run=...) */
void pg_features_dev_run(int fp, int idx, const wchar_t* outfile, const wchar_t* text, int opt)
{
    if (fp < 0 || fp >= FP_COUNT) return;
    g_dev_run = TRUE;
    g_dev_slot = fp;
    wcopy(g_dev_out, MAX_PATH, outfile);
    fp_select(fp, idx);
    if (text && text[0]) { edit_set(g_ed_feat, text); }
    fp_opt[fp][idx] = opt;
    fp_run(fp);
}

void pg_features_app_done(int slot)
{
    if (g_dev_run && slot == g_dev_slot) {   /* dump the output and quit (automated tests only) */
        wchar_t* t = log_text(slot);
        char* u8 = w_to_utf8(t);
        HANDLE h = CreateFileW(g_dev_out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE && u8) { DWORD w; WriteFile(h, u8, (DWORD)strlen(u8), &w, NULL); CloseHandle(h); }
        free(u8); free(t);
        PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
    }
}
