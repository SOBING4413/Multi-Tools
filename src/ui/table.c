/* table.c - custom owner-drawn data table (sorting, filtering, selection, scrollbar, keyboard) */
#include "ui/ui.h"
#include "core/util.h"
#include "core/settings.h"

/* ============================ table ============================ */
void tbl_init(Table* t)
{
    ZeroMemory(t, sizeof(*t));
    t->sel = -1; t->hover = -1; t->sortcol = -1; t->sortasc = 1;
}

static void row_free(TblRow* r)
{
    int i;
    for (i = 0; i < TBL_MAXC; i++) { free(r->c[i]); r->c[i] = NULL; }
}

void tbl_clear(Table* t)
{
    int i;
    for (i = 0; i < t->n; i++) row_free(&t->rows[i]);
    t->n = 0;
    t->nview = 0;
    t->sel = -1; t->hover = -1; t->scroll = 0;
}

void tbl_free(Table* t)
{
    tbl_clear(t);
    free(t->rows); free(t->view);
    t->rows = NULL; t->view = NULL; t->cap = 0;
}

TblRow* tbl_add(Table* t, int sev, void* user)
{
    TblRow* r;
    if (t->n == t->cap) {
        int nc = t->cap ? t->cap * 2 : 128;
        TblRow* nr = (TblRow*)realloc(t->rows, sizeof(TblRow) * (size_t)nc);
        int* nv = (int*)realloc(t->view, sizeof(int) * (size_t)nc);
        if (!nr || !nv) return NULL;
        t->rows = nr; t->view = nv; t->cap = nc;
    }
    r = &t->rows[t->n++];
    ZeroMemory(r, sizeof(*r));
    r->sev = sev;
    r->user = user;
    return r;
}

void tbl_set(TblRow* r, int col, const wchar_t* text, double key)
{
    if (col < 0 || col >= TBL_MAXC) return;
    free(r->c[col]);
    r->c[col] = wdup(text ? text : L"");
    r->key[col] = key;
}

static Table* g_sort_t = NULL;

static int row_cmp(const void* a, const void* b)
{
    const Table* t = g_sort_t;
    int ia = *(const int*)a, ib = *(const int*)b, c = t->sortcol, r;
    const TblRow* x = &t->rows[ia];
    const TblRow* y = &t->rows[ib];
    if (t->col[c].kind == 1 || t->col[c].kind == 2) {
        double d = x->key[c] - y->key[c];
        r = d < 0 ? -1 : (d > 0 ? 1 : 0);
    } else r = _wcsicmp(x->c[c] ? x->c[c] : L"", y->c[c] ? y->c[c] : L"");
    if (!t->sortasc) r = -r;
    if (r == 0) r = ia - ib;
    return r;
}

static BOOL row_matches(const Table* t, const TblRow* r)
{
    int i;
    if (t->pred && !t->pred(r, t->pred_ctx)) return FALSE;
    if (!t->filter[0]) return TRUE;
    for (i = 0; i < t->ncols; i++) if (r->c[i] && wcontains_i(r->c[i], t->filter)) return TRUE;
    return FALSE;
}

void tbl_rebuild(Table* t)
{
    int i;
    t->nview = 0;
    for (i = 0; i < t->n; i++) if (row_matches(t, &t->rows[i])) t->view[t->nview++] = i;
    if (t->sortcol >= 0 && t->sortcol < t->ncols && t->nview > 1) {
        g_sort_t = t;
        qsort(t->view, (size_t)t->nview, sizeof(int), row_cmp);
    }
    if (t->sel >= 0) {   /* drop selection if filtered out */
        int found = 0;
        for (i = 0; i < t->nview; i++) if (t->view[i] == t->sel) { found = 1; break; }
        if (!found) t->sel = -1;
    }
}

static int tbl_rowh(void) { return S(30); }
static int tbl_headh(void) { return S(32); }
static int tbl_sbw(void) { return S(10); }

static int tbl_visible_h(const Table* t) { return (t->rc.bottom - t->rc.top) - tbl_headh(); }
static int tbl_content_h(const Table* t) { return t->nview * tbl_rowh(); }
static int tbl_max_scroll(const Table* t) { int m = tbl_content_h(t) - tbl_visible_h(t); return m > 0 ? m : 0; }

static void col_widths(const Table* t, int avail, int* out)
{
    int i, sum = 0, used = 0;
    for (i = 0; i < t->ncols; i++) sum += t->col[i].w;
    if (sum <= 0) sum = 1;
    for (i = 0; i < t->ncols; i++) {
        out[i] = (i == t->ncols - 1) ? avail - used : avail * t->col[i].w / sum;
        used += out[i];
    }
}

void tbl_draw(Table* t, const RECT* rc)
{
    int cw[TBL_MAXC], avail, i, y, hh = tbl_headh(), rh = tbl_rowh(), first, last, x;
    BOOL has_sb;
    t->rc = *rc;
    w_card_rc(rc);
    avail = (rc->right - rc->left) - S(2) - tbl_sbw();
    col_widths(t, avail, cw);
    if (t->scroll > tbl_max_scroll(t)) t->scroll = tbl_max_scroll(t);
    if (t->scroll < 0) t->scroll = 0;
    gfx_clip(rc->left + 1, rc->top + 1, rc->right - 1, rc->bottom - 1);

    /* header */
    gfx_rect(rc->left + 1, rc->top + 1, rc->right - rc->left - 2, hh, rgb_mix(g_th.card2, g_th.card, 0.3f), 255);
    gfx_rect(rc->left + 1, rc->top + hh, rc->right - rc->left - 2, 1, g_th.border2, 255);
    x = rc->left + S(1);
    for (i = 0; i < t->ncols; i++) {
        RECT hr;
        const wchar_t* title = T2(t->col[i].en, t->col[i].id);
        hr.left = x + S(12); hr.right = x + cw[i] - S(8); hr.top = rc->top + 1; hr.bottom = rc->top + hh;
        gfx_text_rc(&hr, title, F_UI_SB, i == t->sortcol ? g_th.accent : g_th.dim, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | (t->col[i].right ? DT_RIGHT : DT_LEFT));
        if (i == t->sortcol) {
            float ax = (float)(hr.left + (t->col[i].right ? 0 : gfx_text_w(title, F_UI_SB) + S(8))), ay = (float)(rc->top + hh / 2);
            if (t->col[i].right) ax = (float)(hr.right - gfx_text_w(title, F_UI_SB) - S(10));
            gfx_line(ax - 3, ay + (t->sortasc ? 1.5f : -1.5f), ax, ay + (t->sortasc ? -1.5f : 1.5f), 1.5f, g_th.accent, 255);
            gfx_line(ax + 3, ay + (t->sortasc ? 1.5f : -1.5f), ax, ay + (t->sortasc ? -1.5f : 1.5f), 1.5f, g_th.accent, 255);
        }
        { RECT hot; hot.left = x; hot.right = x + cw[i]; hot.top = rc->top; hot.bottom = rc->top + hh; (void)hot; }
        x += cw[i];
    }

    /* rows */
    gfx_clip(rc->left + 1, rc->top + hh + 1, rc->right - 1, rc->bottom - 1);
    first = t->scroll / rh;
    last = (t->scroll + tbl_visible_h(t)) / rh + 1;
    if (last > t->nview) last = t->nview;
    for (i = first; i < last; i++) {
        const TblRow* r = &t->rows[t->view[i]];
        int ri = t->view[i], k;
        RECT row;
        row.left = rc->left + 1; row.right = rc->right - 1;
        row.top = rc->top + hh + i * rh - t->scroll; row.bottom = row.top + rh;
        if (i % 2 == 1) gfx_rect(row.left, row.top, row.right - row.left, rh, g_th.card2, 70);
        if (ri == t->hover) gfx_rect(row.left, row.top, row.right - row.left, rh, g_th.accent, 14);
        if (ri == t->sel) {
            gfx_rect(row.left, row.top, row.right - row.left, rh, g_th.accent, 38);
            gfx_rect(row.left, row.top, S(3), rh, g_th.accent, 255);
        }
        x = rc->left + 1;
        for (k = 0; k < t->ncols; k++) {
            RECT cr;
            const wchar_t* s = r->c[k] ? r->c[k] : L"";
            cr.left = x + S(12); cr.right = x + cw[k] - S(8); cr.top = row.top; cr.bottom = row.bottom;
            if (t->col[k].kind == 2 && s[0]) {
                int bw = gfx_text_w(s, F_UI_SB) + S(18);
                RECT br;
                if (bw > cw[k] - S(10)) bw = cw[k] - S(10);
                br.left = cr.left - S(4); br.right = br.left + bw; br.top = row.top + S(5); br.bottom = row.bottom - S(5);
                w_badge(&br, s, sev_color(r->sev));
            } else {
                gfx_text_rc(&cr, s, F_UI_S, (t->col[k].kind == 1 || k == 0) ? g_th.text : rgb_mix(g_th.text, g_th.dim, 0.35f),
                            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | (t->col[k].right ? DT_RIGHT : DT_LEFT));
            }
            x += cw[k];
        }
    }
    if (t->nview == 0) {
        RECT er = *rc;
        er.top += hh;
        gfx_text_rc(&er, T2(t->empty_en ? t->empty_en : L"No data", t->empty_id ? t->empty_id : L"Tidak ada data"), F_UI, g_th.muted, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }

    /* scrollbar */
    has_sb = tbl_max_scroll(t) > 0;
    gfx_clip(rc->left + 1, rc->top + hh + 1, rc->right - 1, rc->bottom - 1);
    if (has_sb) {
        int track_h = tbl_visible_h(t) - S(8);
        int th = MT_MAX(S(28), (int)((double)track_h * tbl_visible_h(t) / tbl_content_h(t)));
        int ty = rc->top + hh + S(4) + (int)((double)(track_h - th) * t->scroll / tbl_max_scroll(t));
        gfx_rrect((float)(rc->right - tbl_sbw() - S(2)), (float)ty, (float)S(6), (float)th, (float)S(3), t->drag ? g_th.accent : g_th.border2, 255);
    }
    gfx_unclip();
    (void)y;
}

static int tbl_row_at(const Table* t, int x, int y)
{
    int rel;
    if (x < t->rc.left || x >= t->rc.right - tbl_sbw()) return -1;
    if (y < t->rc.top + tbl_headh() || y >= t->rc.bottom) return -1;
    rel = (y - t->rc.top - tbl_headh() + t->scroll) / tbl_rowh();
    if (rel < 0 || rel >= t->nview) return -1;
    return t->view[rel];
}

static void ensure_visible(Table* t)
{
    int i, rh = tbl_rowh();
    for (i = 0; i < t->nview; i++) if (t->view[i] == t->sel) {
        int top = i * rh, bot = top + rh;
        if (top < t->scroll) t->scroll = top;
        else if (bot > t->scroll + tbl_visible_h(t)) t->scroll = bot - tbl_visible_h(t);
        break;
    }
}

BOOL tbl_mouse(Table* t, UINT msg, int x, int y)
{
    POINT p;
    int i;
    p.x = x; p.y = y;
    if (msg == WM_LBUTTONUP) {
        if (t->drag) { t->drag = 0; return TRUE; }
        return FALSE;
    }
    if (msg == WM_MOUSEMOVE) {
        if (t->drag) {
            int track_h = tbl_visible_h(t) - S(8);
            int th = MT_MAX(S(28), (int)((double)track_h * tbl_visible_h(t) / MT_MAX(1, tbl_content_h(t))));
            int top = t->rc.top + tbl_headh() + S(4);
            double f = (double)(y - t->drag_dy - top) / (double)MT_MAX(1, track_h - th);
            if (f < 0) f = 0;
            if (f > 1) f = 1;
            t->scroll = (int)(f * tbl_max_scroll(t));
            return TRUE;
        }
        i = PtInRect(&t->rc, p) ? tbl_row_at(t, x, y) : -1;
        if (i != t->hover) { t->hover = i; return TRUE; }
        return FALSE;
    }
    if (!PtInRect(&t->rc, p)) return FALSE;
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
        if (y < t->rc.top + tbl_headh()) {
            int cw[TBL_MAXC], cx = t->rc.left + 1, k, avail = (t->rc.right - t->rc.left) - S(2) - tbl_sbw();
            col_widths(t, avail, cw);
            for (k = 0; k < t->ncols; k++) {
                if (x >= cx && x < cx + cw[k]) {
                    if (t->sortcol == k) t->sortasc = !t->sortasc; else { t->sortcol = k; t->sortasc = (t->col[k].kind == 0); }
                    tbl_rebuild(t);
                    return TRUE;
                }
                cx += cw[k];
            }
            return TRUE;
        }
        if (x >= t->rc.right - tbl_sbw() - S(4) && tbl_max_scroll(t) > 0) {
            int track_h = tbl_visible_h(t) - S(8);
            int th = MT_MAX(S(28), (int)((double)track_h * tbl_visible_h(t) / tbl_content_h(t)));
            int ty = t->rc.top + tbl_headh() + S(4) + (int)((double)(track_h - th) * t->scroll / tbl_max_scroll(t));
            t->drag = 1;
            t->drag_dy = (y >= ty && y <= ty + th) ? y - ty : th / 2;
            return tbl_mouse(t, WM_MOUSEMOVE, x, y);
        }
        i = tbl_row_at(t, x, y);
        t->sel = i;
        return TRUE;
    }
    return FALSE;
}

void tbl_wheel(Table* t, int delta)
{
    t->scroll -= delta / 120 * tbl_rowh() * 3;
    if (t->scroll < 0) t->scroll = 0;
    if (t->scroll > tbl_max_scroll(t)) t->scroll = tbl_max_scroll(t);
}

BOOL tbl_key(Table* t, WPARAM vk)
{
    int i, cur = -1, nv = t->nview, page = MT_MAX(1, tbl_visible_h(t) / tbl_rowh());
    if (nv == 0) return FALSE;
    for (i = 0; i < nv; i++) if (t->view[i] == t->sel) { cur = i; break; }
    switch (vk) {
    case VK_UP:    cur = cur < 0 ? 0 : MT_MAX(0, cur - 1); break;
    case VK_DOWN:  cur = cur < 0 ? 0 : MT_MIN(nv - 1, cur + 1); break;
    case VK_PRIOR: cur = cur < 0 ? 0 : MT_MAX(0, cur - page); break;
    case VK_NEXT:  cur = cur < 0 ? 0 : MT_MIN(nv - 1, cur + page); break;
    case VK_HOME:  cur = 0; break;
    case VK_END:   cur = nv - 1; break;
    default: return FALSE;
    }
    t->sel = t->view[cur];
    ensure_visible(t);
    return TRUE;
}

TblRow* tbl_selected(Table* t)
{
    if (t->sel < 0 || t->sel >= t->n) return NULL;
    return &t->rows[t->sel];
}
