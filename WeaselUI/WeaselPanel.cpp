#include "WeaselPanel.h"
#include "FullScreenLayout.h"
#include "HorizontalLayout.h"
#include "VHorizontalLayout.h"
#include "VerticalLayout.h"
#include <filesystem>
#include <memory>
#include <resource.h>
#include <windowsx.h>
#include <wrl/client.h>

namespace fs = std::filesystem;

using namespace weasel;

#define DPI_SCALE(t) (int)(t * m_pD2D->m_dpiScaleLayout)
#define COLORTRANSPARENT(color) ((color & 0xff000000) == 0)
#define COLORNOTTRANSPARENT(color) ((color & 0xff000000) != 0)
#define TRANS_COLOR 0x00000000
#define HALF_ALPHA_COLOR(color)                                                \
  ((((color & 0xff000000) >> 25) & 0xff) << 24) | (color & 0x00ffffff)

void LoadIconIfNeed(wstring &oicofile, const wstring &icofile, HICON &hIcon,
                    UINT id) {
  if (oicofile == icofile)
    return;
  oicofile = icofile;
  const int STATUS_ICON_SIZE = GetSystemMetrics(SM_CXICON);
  HINSTANCE hInstance = GetModuleHandle(NULL);
  if (!icofile.empty() && fs::exists(fs::path(icofile)))
    hIcon =
        (HICON)LoadImage(hInstance, icofile.c_str(), IMAGE_ICON,
                         STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_LOADFROMFILE);
  else
    hIcon =
        (HICON)LoadImage(hInstance, MAKEINTRESOURCE(id), IMAGE_ICON,
                         STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
}

WeaselPanel::WeaselPanel(UI &ui)
    : m_hWnd(nullptr), m_ctx(ui.ctx()), m_octx(ui.octx()), m_layout(nullptr),
      m_pD2D(nullptr), m_status(ui.status()), m_in_server(ui.InServer()),
      m_style(ui.style()), m_uiCallback(ui.uiCallback()), m_ostyle(ui.ostyle()),
      m_candidateCount(0), m_lastCandidateCount(0), hide_candidates(false) {
  // Prepare shared graphics resources early to reduce first paint latency.
  m_pD2D = std::make_shared<D2D>(m_style);
  auto hInstance = GetModuleHandle(nullptr);
  m_iconAlpha = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_EN));
  m_iconEnabled = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ZH));
  m_iconFull = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FULL_SHAPE));
  m_iconHalf = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_HALF_SHAPE));
  m_iconDisabled = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_RELOAD));
}

BOOL WeaselPanel::IsWindow() const { return ::IsWindow(m_hWnd); }

HWND WeaselPanel::hwnd() const { return m_hWnd; }

void WeaselPanel::_ClearTimers() {
  if (!m_hWnd)
    return;
  if (m_clickTimer) {
    ::KillTimer(m_hWnd, AUTOREV_TIMER);
    m_clickTimer = 0;
  }
  if (m_autoHideTimer) {
    ::KillTimer(m_hWnd, AUTOHIDE_TIMER);
    m_autoHideTimer = 0;
  }
}

void WeaselPanel::ShowWithTimeout(size_t millisec) {
  if (!m_hWnd)
    return;
  ShowWindow(SW_SHOWNA);
  // cancel existing auto-hide timer if any
  if (m_autoHideTimer) {
    ::KillTimer(m_hWnd, m_autoHideTimer);
    m_autoHideTimer = 0;
  }
  m_autoHideTimer =
      ::SetTimer(m_hWnd, AUTOHIDE_TIMER, static_cast<UINT>(millisec), NULL);
}

bool WeaselPanel::IsCountingDown() const { return m_autoHideTimer != 0; }

void WeaselPanel::ShowWindow(int nCmdShow) {
  ::ShowWindow(m_hWnd, nCmdShow);
  if (m_pD2D) {
    if (nCmdShow != SW_HIDE) {
      // ensure window resources exist when showing
      if (!m_pD2D->swapChain) {
        m_pD2D->AttachWindow(m_hWnd);
        if (!m_style.font_face.empty())
          m_pD2D->InitDirectWriteResources();
      }
    }
  }
}

void WeaselPanel::DestroyWindow() {
  // clear timers before releasing window resources
  _ClearTimers();

  if (m_pD2D) {
    m_pD2D->ReleaseWindowResources();
    // mark D2D as detached from any window
    m_pD2D->m_hWnd = nullptr;
  }
  ::DestroyWindow(m_hWnd);
  m_hWnd = nullptr;
}

void WeaselPanel::ReleaseAllResources() {
  // ensure timers are killed before releasing window/device resources
  _ClearTimers();

  if (m_pD2D) {
    m_pD2D->ReleaseWindowResources();
    // explicitly reset shared devices
    DeviceResources::Get().Reset();
    m_pD2D.reset();
  }
}

void WeaselPanel::MoveTo(RECT rc) {
  if (!m_hWnd || !m_layout)
    return;
  m_redraw_by_monitor_change = false;
  bool should_reset_sticky =
      (m_ctx.empty() || (abs(rc.left - m_inputPos.left) > 50) ||
       (abs(rc.bottom - m_inputPos.bottom) > 50));
  if (should_reset_sticky && m_sticky) {
    m_sticky = false;
    m_inputPos = rc;
    m_inputPos.bottom += 6;
    _Reposition(true);
    RedrawWindow();
    return;
  }
  if (m_style.ascii_tip_follow_cursor && m_ctx.empty() && (!m_status.composing) &&
      m_layout->ShouldDisplayStatusIcon()) {
    POINT p;
    ::GetCursorPos(&p);
    RECT irc{p.x - STATUS_ICON_SIZE, p.y - STATUS_ICON_SIZE, p.x, p.y};
    m_inputPos = irc;
    _Reposition(true);
    RedrawWindow();
  } else if (!(rc.left == m_inputPos.left && rc.bottom != m_inputPos.bottom &&
               abs(rc.bottom - m_inputPos.bottom) < 6) ||
             m_layout->ShouldDisplayStatusIcon()) {
    m_inputPos = rc;
    m_inputPos.bottom += 6;
    bool m_istorepos_buf = m_istorepos;
    _Reposition(true);
    if (m_istorepos != m_istorepos_buf || !m_ctx.aux.empty() ||
        m_layout->ShouldDisplayStatusIcon() || m_redraw_by_monitor_change) {
      RedrawWindow();
    }
  }
}

void WeaselPanel::_ResizeWindow() {
  CSize &size = m_layout->GetContentSize();
  SetWindowPos(m_hWnd, 0, 0, 0, size.cx, size.cy,
               SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW);
  m_pD2D->OnResize(size.cx, size.cy);
}

void WeaselPanel::_CreateLayout() {
  the<Layout> layout;
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT ||
      m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT_FULLSCREEN) {
    layout =
        std::make_unique<VHorizontalLayout>(m_style, m_ctx, m_status, m_pD2D);
  } else {
    if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL ||
        m_style.layout_type == UIStyle::LAYOUT_VERTICAL_FULLSCREEN) {
      layout =
          std::make_unique<VerticalLayout>(m_style, m_ctx, m_status, m_pD2D);
    } else if (m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL ||
               m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL_FULLSCREEN) {
      layout =
          std::make_unique<HorizontalLayout>(m_style, m_ctx, m_status, m_pD2D);
    }
  }
  if (IS_FULLSCREENLAYOUT(m_style)) {
    layout = std::make_unique<FullScreenLayout>(
        m_style, m_ctx, m_status, m_inputPos, std::move(layout), m_pD2D);
  }
  m_layout = std::move(layout);
}

void WeaselPanel::_UpdateHideCandidates() {
  bool should_show_icon =
      (m_status.ascii_mode || !m_status.composing || !m_ctx.aux.empty());
  m_candidateCount = (BYTE)m_ctx.cinfo.candies.size();
  // When candidate list vanishes, release sticky top/bottom placement state.
  if (m_lastCandidateCount > 0 && m_candidateCount == 0) {
    m_sticky = false;
  }
  m_lastCandidateCount = m_candidateCount;
  // check if to hide candidates window
  // show tips status, two kind of situation: 1) only aux strings, don't care
  // icon status; 2)only icon(ascii mode switching)
  bool show_tips =
      m_in_server &&
      ((!m_ctx.aux.empty() && m_ctx.cinfo.empty() && m_ctx.preedit.empty()) ||
       (m_ctx.empty() && should_show_icon));
  // show schema menu status: schema_id == L".default"
  bool show_schema_menu = m_status.schema_id == L".default";
  bool margin_negative =
      (DPI_SCALE(m_style.margin_x) < 0 || DPI_SCALE(m_style.margin_y) < 0);
  // when to hide_cadidates?
  // 1. margin_negative, and not in show tips mode( ascii switching /
  // half-full switching / simp-trad switching / error tips), and not in
  // schema menu
  // 2. inline preedit without candidates
  bool inline_no_candidates =
      (m_style.inline_preedit && m_candidateCount == 0) && !show_tips;
  hide_candidates = inline_no_candidates ||
                    (margin_negative && !show_tips && !show_schema_menu);
}

void WeaselPanel::Refresh() {
  if (!m_hWnd)
    return;
  if (!m_pD2D)
    m_pD2D = std::make_shared<D2D>(m_style);
  m_pD2D->AttachWindow(m_hWnd);
  if (!m_style.font_face.empty() &&
      (m_ostyle != m_style || !m_pD2D->pTextFormat))
    m_pD2D->InitDirectWriteResources();
  m_ostyle = m_style;
  _UpdateHideCandidates();
  auto hr = m_pD2D->direct3dDevice
                ? m_pD2D->direct3dDevice->GetDeviceRemovedReason()
                : DXGI_ERROR_DEVICE_REMOVED;
  if (hr != S_OK) {
    DEBUG << "Device removed detected: " << HRESULTToString(hr);
    DeviceResources::Get().Reset();
    if (m_pD2D->swapChain) // only reinit window resources if attached
      m_pD2D->InitDirect2D();
  }
  _CreateLayout();
  m_layout->DoLayout();
  _ResizeWindow();
  _Reposition();
  RedrawWindow();
}

BOOL WeaselPanel::Create(HWND parent) {
  if (m_hWnd)
    return !!m_hWnd;
  m_hoverIndex = -1;
  POINT cursor_pos = {0, 0};
  if (::GetCursorPos(&cursor_pos)) {
    m_lastCursorPos = CPoint(cursor_pos.x, cursor_pos.y);
  } else {
    m_lastCursorPos = CPoint(-1, -1);
  }
  HINSTANCE hInstance = GetModuleHandle(nullptr);
  WNDCLASS wc = {};
  wc.lpfnWndProc = WeaselPanel::WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"WeaselPanel";
  RegisterClass(&wc);
  m_hWnd = CreateWindowEx(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
          WS_EX_NOREDIRECTIONBITMAP,
      L"WeaselPanel", L"WeaselPanel", WS_POPUP | WS_CLIPSIBLINGS,
      CW_USEDEFAULT, CW_USEDEFAULT, 10, 10, parent, nullptr, hInstance, this);
  if (m_hWnd) {
    if (!m_pD2D)
      m_pD2D = std::make_shared<D2D>(m_style);
    m_pD2D->AttachWindow(m_hWnd);
    if (!m_style.font_face.empty())
      m_pD2D->InitDirectWriteResources();
    m_ostyle = m_style;
  }
  return !!m_hWnd;
}

void WeaselPanel::_UpdateOffsetY(CRect &arc, CRect &prc) {
  if (m_istorepos) {
    std::vector<CRect> rects(m_candidateCount);
    std::vector<int> btmys(m_candidateCount);
    for (int i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
      rects[i] = m_layout->GetCandidateRect(i);
      btmys[i] = rects[i].bottom;
    }
    m_offsety_preedit = m_candidateCount && !m_layout->IsInlinePreedit() &&
                                !m_ctx.preedit.str.empty()
                            ? rects.back().bottom - prc.bottom
                            : 0;
    m_offsety_aux = m_candidateCount && !m_ctx.aux.str.empty()
                        ? rects.back().bottom - arc.bottom
                        : 0;
    int base_gap =
        !m_ctx.aux.str.empty()
            ? arc.Height() + m_style.spacing
            : (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty()
                   ? prc.Height() + m_style.spacing
                   : 0);
    for (int i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
      m_offsetys[i] = i == 0 ? btmys.back() - base_gap - rects[i].bottom
                             : rects[i - 1].top + m_offsetys[i - 1] -
                                   DPI_SCALE(m_style.candidate_spacing) -
                                   rects[i].bottom;
    }
  }
}

void WeaselPanel::DoPaint() {
  if (!m_pD2D || !m_layout)
    return;
  auto hr = m_pD2D->direct3dDevice->GetDeviceRemovedReason();
  if (FAILED(hr)) {
    DEBUG << HRESULTToString(hr);
    m_pD2D->InitDirect2D();
  }
  m_pD2D->dc->BeginDraw();
  m_pD2D->dc->Clear(D2D1::ColorF({0.0f, 0.0f, 0.0f, 0.0f}));
  if (!hide_candidates) {
    const bool should_draw_background =
        ((!m_ctx.empty() && !m_style.inline_preedit) ||
         (m_style.inline_preedit && (m_candidateCount || !m_ctx.aux.empty())));
    if (should_draw_background) {
      CRect &rc = m_layout->GetContentRect();
      IsToRoundStruct roundInfo;
      _HighlightRect(rc, DPI_SCALE(m_style.round_corner_ex),
                     DPI_SCALE(m_style.border), m_style.back_color,
                     m_style.shadow_color, m_style.border_color, roundInfo);
    }
    CRect &prc = m_layout->GetPreeditRect();
    CRect &arc = m_layout->GetAuxiliaryRect();
    // if vertical auto reverse triggered
    _UpdateOffsetY(arc, prc);
    if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.empty()) {
      _DrawPreedit(m_ctx.preedit, true);
    }
    if (!m_ctx.aux.empty()) {
      _DrawPreedit(m_ctx.aux, false);
    }
    if (m_candidateCount) {
      _DrawCandidates();
    }
    if (m_layout->ShouldDisplayStatusIcon()) {
      ComPtr<ID2D1Bitmap1> pBitmap;
#define LOADICON(x, icon, id) LoadIconIfNeed(m_##x, m_style.##x, icon, id)
      LOADICON(current_ascii_icon, m_iconAlpha, IDI_EN);
      LOADICON(current_zhung_icon, m_iconEnabled, IDI_ZH);
      LOADICON(current_full_icon, m_iconFull, IDI_FULL_SHAPE);
      LOADICON(current_half_icon, m_iconHalf, IDI_HALF_SHAPE);
#undef LOADICON

      HICON &ico =
          m_status.disabled ? m_iconDisabled
          : m_status.ascii_mode
              ? m_iconAlpha
              : (m_status.type == SCHEMA
                     ? m_iconEnabled
                     : (m_status.full_shape ? m_iconFull : m_iconHalf));
      m_pD2D->GetBmpFromIcon(ico, pBitmap);
      // Draw the bitmap
      auto iconRect = m_layout->GetStatusIconRect();
      if (m_istorepos)
        iconRect.OffsetRect(0, m_offsety_preedit);
      D2D1_RECT_F iconRectf = D2D1::RectF(iconRect.left, iconRect.top,
                                          iconRect.right, iconRect.bottom);
      m_pD2D->dc->DrawBitmap(pBitmap.Get(), iconRectf);
    }
  }

  HR(m_pD2D->dc->EndDraw());
  // Make the swap chain available to the composition engine
  HRESULT hrPresent = m_pD2D->swapChain->Present(1, 0); // sync
  if (hrPresent == DXGI_ERROR_DEVICE_REMOVED ||
      hrPresent == DXGI_ERROR_DEVICE_RESET) {
    DEBUG << "Device lost during Present: " << HRESULTToString(hrPresent);
    DeviceResources::Get().Reset();
    m_pD2D->InitDirect2D();
  } else {
    HR(hrPresent);
  }
}

bool WeaselPanel::_DrawPreedit(const Text &text, bool isPreedit) {
  bool drawn = false;
  wstring const &t = text.str;
  PtTextFormat &pTextFormat = m_pD2D->pPreeditFormat;

  if (!t.empty()) {
    const TextRange &range = m_layout->GetPreeditRange();
    if (range.start < range.end) {
      auto before_str = t.substr(0, range.start);
      auto hilited_str = t.substr(range.start, range.end - range.start);
      auto after_str = t.substr(range.end);

      // Use precomputed rectangles - make copies when we need to modify them
      CRect rc_before = isPreedit ? m_layout->GetPreeditBeforeRect()
                                  : m_layout->GetAuxBeforeRect();
      CRect rc_hi = isPreedit ? m_layout->GetPreeditHiliteRect()
                              : m_layout->GetAuxHiliteRect();
      CRect rc_after = isPreedit ? m_layout->GetPreeditAfterRect()
                                 : m_layout->GetAuxAfterRect();

      // Apply m_istorepos offset if needed
      if (m_istorepos) {
        int offsetY = isPreedit ? m_offsety_preedit : m_offsety_aux;
        rc_before.OffsetRect(0, offsetY);
        rc_hi.OffsetRect(0, offsetY);
        rc_after.OffsetRect(0, offsetY);
      }

      // Use DPI_SCALE macro for consistency with other code
      auto padx = DPI_SCALE(m_style.hilite_padding_x);
      auto pady = DPI_SCALE(m_style.hilite_padding_y);

      if (range.start > 0 && !rc_before.IsRectNull()) {
        _TextOut(rc_before, before_str, before_str.length(), m_style.text_color,
                 pTextFormat);
      }
      if (!rc_hi.IsRectNull()) {
        // zzz[yyy]
        CRect rc_hib = rc_hi;
        rc_hib.InflateRect(padx, pady);
        const IsToRoundStruct &roundInfo = m_layout->GetTextRoundInfo();
        _HighlightRect(rc_hib, DPI_SCALE(m_style.round_corner),
                       DPI_SCALE(m_style.border), m_style.hilited_back_color,
                       m_style.hilited_shadow_color, 0, roundInfo);
        _TextOut(rc_hi, hilited_str, hilited_str.length(),
                 m_style.hilited_text_color, pTextFormat);
      }
      if (range.end < static_cast<int>(t.length()) && !rc_after.IsRectNull()) {
        // zzz[yyy]xxx
        _TextOut(rc_after, after_str, after_str.length(), m_style.text_color,
                 pTextFormat);
      }
    } else {
      // No highlighted text, use the base rectangle from layout
      CRect rcText =
          isPreedit ? m_layout->GetPreeditRect() : m_layout->GetAuxiliaryRect();

      // Apply m_istorepos offset if needed
      if (m_istorepos) {
        int offsetY = isPreedit ? m_offsety_preedit : m_offsety_aux;
        rcText.OffsetRect(0, offsetY);
      }

      _TextOut(rcText, t.c_str(), t.length(), m_style.text_color, pTextFormat);
    }
    if (m_candidateCount && !m_style.inline_preedit &&
        COLORNOTTRANSPARENT(m_style.prevpage_color) &&
        COLORNOTTRANSPARENT(m_style.nextpage_color)) {
      const std::wstring pre = L"<";
      const std::wstring next = L">";
      CRect prc = m_layout->GetPrepageRect();
      if (m_istorepos)
        prc.OffsetRect(0, m_offsety_preedit);
      // clickable color / disabled color
      int color =
          m_ctx.cinfo.currentPage ? m_style.prevpage_color : m_style.text_color;
      _TextOut(prc, pre.c_str(), pre.length(), color, pTextFormat);

      CRect nrc = m_layout->GetNextpageRect();
      if (m_istorepos)
        nrc.OffsetRect(0, m_offsety_preedit);
      // clickable color / disabled color
      color = m_ctx.cinfo.is_last_page ? m_style.text_color
                                       : m_style.nextpage_color;
      _TextOut(nrc, next.c_str(), next.length(), color, pTextFormat);
    }
    drawn = true;
  }
  return drawn;
}

CRect WeaselPanel::_GetInflatedCandRect(int i) {
  CRect rc = m_layout->GetCandidateRect(i);
  if (m_istorepos)
    rc.OffsetRect(0, m_offsetys[i]);
  const auto padx = DPI_SCALE(m_style.hilite_padding_x);
  const auto pady = DPI_SCALE(m_style.hilite_padding_y);
  rc.InflateRect(padx, pady);
  return rc;
}

bool WeaselPanel::_DrawCandidates() {
  bool drawn = false;
  const vector<Text> &candidates(m_ctx.cinfo.candies);
  const vector<Text> &comments(m_ctx.cinfo.comments);
  const vector<Text> &labels(m_ctx.cinfo.labels);
  PtTextFormat &txtFormat = m_pD2D->pTextFormat;
  PtTextFormat &labeltxtFormat = m_pD2D->pLabelFormat;
  PtTextFormat &commenttxtFormat = m_pD2D->pCommentFormat;
  auto padx = DPI_SCALE(m_style.hilite_padding_x);
  auto pady = DPI_SCALE(m_style.hilite_padding_y);
  const auto hilitefunc = [&](int i, uint32_t back_color, uint32_t shadow_color,
                              uint32_t border_color, uint32_t border = 0) {
    auto rect = _GetInflatedCandRect(i);
    const IsToRoundStruct &roundInfo = m_layout->GetRoundInfo(i);
    _HighlightRect(rect, DPI_SCALE(m_style.round_corner), DPI_SCALE(border),
                   back_color, shadow_color, border_color, roundInfo);
  };
  for (auto i = 0; i < m_candidateCount; i++) {
    if (i == m_hoverIndex)
      continue;
    bool hilited = (i == m_ctx.cinfo.highlighted);
    int shadow_color = hilited ? m_style.hilited_candidate_shadow_color
                               : m_style.candidate_shadow_color;
    if (COLORNOTTRANSPARENT(shadow_color))
      hilitefunc(i, 0, shadow_color, 0);
    drawn = true;
  }
  if (m_hoverIndex >= 0) {
    hilitefunc(m_hoverIndex,
               HALF_ALPHA_COLOR(m_style.hilited_candidate_back_color),
               HALF_ALPHA_COLOR(m_style.hilited_candidate_shadow_color),
               HALF_ALPHA_COLOR(m_style.hilited_candidate_border_color));
  }
  // draw highlighted background and text
  const auto drawText = [&](int i, const vector<Text> &texts, int color,
                            PtTextFormat &textFormat, CRect rc) {
    const auto &text = texts.at(i).str;
    if (COLORTRANSPARENT(color) || rc.IsRectNull() || text.empty() ||
        !textFormat.Get())
      return;
    if (m_istorepos)
      rc.OffsetRect(0, m_offsetys[i]);
    _TextOut(rc, text, text.length(), color, textFormat);
  };
  for (auto i = 0; i < candidates.size(); i++) {
    bool hilited = (i == m_ctx.cinfo.highlighted);
    int label_text_color =
        hilited ? m_style.hilited_label_text_color : m_style.label_text_color;
    int candidate_text_color = hilited ? m_style.hilited_candidate_text_color
                                       : m_style.candidate_text_color;
    int comment_text_color = hilited ? m_style.hilited_comment_text_color
                                     : m_style.comment_text_color;
    int back_color = hilited ? m_style.hilited_candidate_back_color
                             : m_style.candidate_back_color;
    int border_color = hilited ? m_style.hilited_candidate_border_color
                               : m_style.candidate_border_color;
    hilitefunc(i, back_color, 0, border_color, m_style.border);
    drawText(i, labels, label_text_color, labeltxtFormat,
             m_layout->GetCandidateLabelRect(i));
    drawText(i, candidates, candidate_text_color, txtFormat,
             m_layout->GetCandidateTextRect(i));
    drawText(i, comments, comment_text_color, commenttxtFormat,
             m_layout->GetCandidateCommentRect(i));
    drawn = true;
  }
  // draw highlight mark
  if (COLORNOTTRANSPARENT(m_style.hilited_mark_color)) {
    CRect rc = _GetInflatedCandRect(m_ctx.cinfo.highlighted);
    if (!m_style.mark_text.empty()) {
      int vgap =
          m_layout->mark_height ? (rc.Height() - m_layout->mark_height) / 2 : 0;
      int hgap =
          m_layout->mark_width ? (rc.Width() - m_layout->mark_width) / 2 : 0;
      CRect hlRc;
      if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT ||
          m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT_FULLSCREEN)
        hlRc = CRect(rc.left + hgap, rc.top + pady,
                     rc.left + hgap + m_layout->mark_width,
                     rc.top + pady + m_layout->mark_height);
      else
        hlRc = CRect(rc.left + padx, rc.top + vgap,
                     rc.left + padx + m_layout->mark_width, rc.bottom - vgap);
      _TextOut(hlRc, m_style.mark_text.c_str(), m_style.mark_text.length(),
               m_style.hilited_mark_color, txtFormat);
    } else {
      int height = MIN(rc.Height() - pady * 2,
                       rc.Height() - DPI_SCALE(m_style.round_corner) * 2);
      int width = MIN(rc.Width() - padx * 2,
                      rc.Width() - DPI_SCALE(m_style.round_corner) * 2);
      width = MIN(width, static_cast<int>(rc.Width() * 0.618));
      height = MIN(height, static_cast<int>(rc.Height() * 0.618));
      if (m_bar_scale != 1.0f) {
        width = static_cast<int>(width * m_bar_scale);
        height = static_cast<int>(height * m_bar_scale);
      }

      CRect mkrc;
      int mark_radius;
      if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT ||
          m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT_FULLSCREEN) {
        int x = rc.left + (rc.Width() - width) / 2;
        mkrc = CRect(x, rc.top, x + width, rc.top + m_layout->mark_height);
        mark_radius = mkrc.Height() / 2;
      } else {
        int y = rc.top + (rc.Height() - height) / 2;
        mkrc = CRect(rc.left, y, rc.left + m_layout->mark_width, y + height);
        mark_radius = mkrc.Width() / 2;
      }
      IsToRoundStruct roundInfo;
      _HighlightRect(mkrc, mark_radius, 0, m_style.hilited_mark_color, 0, 0,
                     roundInfo);
    }
  }
  return drawn;
}

void WeaselPanel::_TextOut(CRect &rc, const wstring &text, size_t cch,
                           uint32_t color, PtTextFormat &pTextFormat) {
  if (!pTextFormat.Get())
    return;
  m_pD2D->SetBrushColor(color);

  ComPtr<IDWriteTextLayout> pTextLayout;
  m_pD2D->m_pWriteFactory->CreateTextLayout(
      text.c_str(), cch, pTextFormat.Get(), rc.Width(), rc.Height(),
      reinterpret_cast<IDWriteTextLayout **>(pTextLayout.GetAddressOf()));
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT ||
      m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT_FULLSCREEN) {
    DWRITE_FLOW_DIRECTION flow = m_style.vertical_text_left_to_right
                                     ? DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT
                                     : DWRITE_FLOW_DIRECTION_RIGHT_TO_LEFT;
    pTextLayout->SetReadingDirection(DWRITE_READING_DIRECTION_TOP_TO_BOTTOM);
    pTextLayout->SetFlowDirection(flow);
  } else {
    pTextLayout->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT);
    pTextLayout->SetFlowDirection(DWRITE_FLOW_DIRECTION_TOP_TO_BOTTOM);
  }
  float offsetx = (float)rc.left;
  float offsety = (float)rc.top;

  DWRITE_OVERHANG_METRICS omt;
  pTextLayout->GetOverhangMetrics(&omt);
  if (m_style.layout_type != UIStyle::LAYOUT_VERTICAL_TEXT && omt.left > 0)
    offsetx += omt.left;
  if ((m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT ||
       m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT_FULLSCREEN) &&
      omt.top > 0)
    offsety += omt.top;

  m_pD2D->DrawTextLayout(pTextLayout, offsetx, offsety, color, false);
  // draw rectangle for debug
  // m_pD2D->dc->DrawRectangle(
  //     D2D1::RectF((float)rc.left, (float)rc.top, (float)rc.right,
  //                 (float)rc.bottom),
  //     m_pD2D->m_pBrush.Get(), 1.0f); // 1.0f is the border width
}

void WeaselPanel::_HighlightRect(const RECT &rect, float radius,
                                 uint32_t border, uint32_t back_color,
                                 uint32_t shadow_color, uint32_t border_color,
                                 const IsToRoundStruct &roundInfo) {
  if (roundInfo.Hemispherical)
    radius = DPI_SCALE(m_style.round_corner_ex) - DPI_SCALE(border) / 2.0f;
  // draw shadow
  if (COLORNOTTRANSPARENT(shadow_color) && m_style.shadow_radius)
    m_pD2D->FillGeometry(rect, shadow_color, radius, roundInfo, true);
  // draw back color
  if (COLORNOTTRANSPARENT(back_color))
    m_pD2D->FillGeometry(rect, back_color, radius, roundInfo);
  // draw border
  if (COLORNOTTRANSPARENT(border_color) && border) {
    float hb = -(float)border / 2;
    ComPtr<ID2D1PathGeometry> pGeometry;
    HR(m_pD2D->CreateRoundedRectanglePath(rect, radius + hb, roundInfo,
                                          pGeometry));
    m_pD2D->SetBrushColor(border_color);
    m_pD2D->dc->DrawGeometry(pGeometry.Get(), m_pD2D->m_pBrush.Get(), border);
  }
}

void WeaselPanel::_Reposition(bool adj) {
  if (!m_layout || !m_hWnd)
    return;
  RECT rcWorkArea;
  memset(&rcWorkArea, 0, sizeof(rcWorkArea));
  HMONITOR hMonitor = MonitorFromRect(&m_inputPos, MONITOR_DEFAULTTONEAREST);
  if (hMonitor) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(hMonitor, &info)) {
      rcWorkArea = info.rcWork;
    }
    if (hMonitor != m_hMonitor) {
      m_hMonitor = hMonitor;
      m_redraw_by_monitor_change = true;
    }
  }
  CRect rcWindow;
  GetWindowRect(m_hWnd, &rcWindow);
  int width = rcWindow.Width();
  int height = rcWindow.Height();
  rcWorkArea.right -= width;
  rcWorkArea.bottom -= height;
  int x = m_inputPos.left;
  int y = m_inputPos.bottom;
  if (DPI_SCALE(m_style.shadow_radius)) {
    x -= (DPI_SCALE(m_style.shadow_offset_x) >= 0 ||
          COLORTRANSPARENT(m_style.shadow_color))
             ? m_layout->offsetX
             : (m_layout->offsetX / 2);
    if (adj)
      y -= (DPI_SCALE(m_style.shadow_offset_y) > 0 ||
            COLORTRANSPARENT(m_style.shadow_color))
               ? m_layout->offsetY
               : (m_layout->offsetY / 2);
  }
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT &&
      !m_style.vertical_text_left_to_right) {
    x += m_layout->offsetX - width;
    if (DPI_SCALE(m_style.shadow_offset_x) < 0)
      x += m_layout->offsetX;
  }
  if (adj)
    m_istorepos = false;
  if (x > rcWorkArea.right)
    x = rcWorkArea.right;
  if (x < rcWorkArea.left)
    x = rcWorkArea.left;
  if (y > rcWorkArea.bottom || m_sticky) {
    if (!m_sticky)
      m_sticky = true;
    y = m_inputPos.top - height - 6;
    if (DPI_SCALE(m_style.shadow_radius) &&
        DPI_SCALE(m_style.shadow_offset_y) > 0)
      y -= DPI_SCALE(m_style.shadow_offset_y);
    m_istorepos = (m_style.vertical_auto_reverse &&
                   m_style.layout_type == UIStyle::LAYOUT_VERTICAL);
    if (DPI_SCALE(m_style.shadow_radius) > 0)
      y += (DPI_SCALE(m_style.shadow_offset_y) < 0 ||
            COLORTRANSPARENT(m_style.shadow_color))
               ? m_layout->offsetY
               : (m_layout->offsetY / 2);
  }
  if (y < rcWorkArea.top)
    y = rcWorkArea.top;
  m_inputPos.bottom = y;
  SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, 0, 0,
               SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
}

static HBITMAP CopyDCToBitmap(HDC hDC, LPRECT lpRect) {
  if (!hDC || !lpRect || IsRectEmpty(lpRect))
    return NULL;
  HDC hMemDC;
  HBITMAP hBitmap, hOldBitmap;
  int nX, nY, nX2, nY2;
  int nWidth, nHeight;

  nX = lpRect->left;
  nY = lpRect->top;
  nX2 = lpRect->right;
  nY2 = lpRect->bottom;
  nWidth = nX2 - nX;
  nHeight = nY2 - nY;

  hMemDC = CreateCompatibleDC(hDC);
  hBitmap = CreateCompatibleBitmap(hDC, nWidth, nHeight);
  hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
  StretchBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, nX, nY, nWidth, nHeight,
             SRCCOPY);
  hBitmap = (HBITMAP)SelectObject(hMemDC, hOldBitmap);

  DeleteDC(hMemDC);
  DeleteObject(hOldBitmap);
  return hBitmap;
}

void WeaselPanel::_CaptureRect(CRect &rect) {
  HDC ScreenDC = ::GetDC(NULL);
  CRect rc;
  GetWindowRect(m_hWnd, &rc);
  POINT WindowPosAtScreen = {rc.left, rc.top};
  rect.OffsetRect(WindowPosAtScreen);
  // capture input window
  if (::OpenClipboard(m_hWnd)) {
    HBITMAP bmp = CopyDCToBitmap(ScreenDC, LPRECT(rect));
    EmptyClipboard();
    SetClipboardData(CF_BITMAP, bmp);
    CloseClipboard();
    DeleteObject(bmp);
  }
  ::ReleaseDC(m_hWnd, ScreenDC);
}

void WeaselPanel::OnDestroy() {
  // ensure timers are cleared and then reset state
  _ClearTimers();

  m_hoverIndex = -1;
  m_layout.reset();
  m_sticky = false;
}

HRESULT WeaselPanel::OnScroll(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  int delta = GET_WHEEL_DELTA_WPARAM(wParam);
  if (m_uiCallback && delta != 0) {
    bool scroll_down = delta < 0;
    m_uiCallback(NULL, NULL, NULL, &scroll_down);
  }
  return 0;
}

LRESULT WeaselPanel::OnMouseMove(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (m_style.hover_type == UIStyle::NONE)
    return 0;
  CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
  bool hovered = false;
  bool hover_index_change = false;
  CPoint ptScreen = point;
  ClientToScreen(m_hWnd, &ptScreen);
  if (ptScreen == m_lastCursorPos)
    return 0;
  m_lastCursorPos = ptScreen;
  for (int i = 0; i < m_candidateCount; i++) {
    CRect rect = _GetInflatedCandRect(i);
    if (rect.PtInRect(point)) {
      hovered = true;
      if (i != m_ctx.cinfo.highlighted) {
        if (m_style.hover_type == UIStyle::HoverType::HILITE) {
          if (m_uiCallback) {
            size_t hover_index = i;
            m_uiCallback(nullptr, &hover_index, nullptr, nullptr);
          }
        } else if (m_hoverIndex != i) {
          m_hoverIndex = i;
          hover_index_change = true;
        }
      } else if (m_style.hover_type == UIStyle::HoverType::SEMI_HILITE &&
                 m_hoverIndex != -1) {
        m_hoverIndex = -1;
        hover_index_change = true;
      }
    }
  }
  if (!hovered && m_hoverIndex >= 0) {
    m_hoverIndex = -1;
    hover_index_change = true;
  }
  if (hover_index_change)
    RedrawWindow();
  return 0;
}

LRESULT WeaselPanel::OnMouseActive(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  return MA_NOACTIVATE;
}

LRESULT WeaselPanel::OnLeftClickUp(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (hide_candidates)
    return 0;
  CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

  ::KillTimer(m_hWnd, AUTOREV_TIMER);
  m_bar_scale = 1.0;
  m_clickTimer = 0;

  auto rect = _GetInflatedCandRect(m_ctx.cinfo.highlighted);
  if (rect.PtInRect(point)) {
    size_t i = m_ctx.cinfo.highlighted;
    if (m_uiCallback) {
      m_uiCallback(&i, nullptr, nullptr, nullptr);
      if (!m_status.composing)
        DestroyWindow();
    }
  } else {
    RedrawWindow();
  }
  return 0;
}

LRESULT WeaselPanel::OnLeftClickDown(UINT uMsg, WPARAM wParam, LPARAM lParam) {
  if (hide_candidates)
    return 0;
  CPoint point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
  auto padx = DPI_SCALE(m_style.hilite_padding_x);
  auto pady = DPI_SCALE(m_style.hilite_padding_y);
  // capture
  if (m_style.click_to_capture) {
    CRect rcw;
    GetClientRect(m_hWnd, &rcw);
    auto recth = _GetInflatedCandRect(m_ctx.cinfo.highlighted);
    if (recth.PtInRect(point))
      _CaptureRect(recth);
    else {
      CRect crc(rcw);
      // if shadow_color transparent, decrease the capture rectangle size
      if (COLORTRANSPARENT(m_style.shadow_color) &&
          DPI_SCALE(m_style.shadow_radius) != 0) {
        int shadow_gap =
            (m_style.shadow_offset_x == m_style.shadow_offset_y == 0)
                ? 2 * DPI_SCALE(m_style.shadow_radius)
                : DPI_SCALE(m_style.shadow_radius) +
                      DPI_SCALE(m_style.shadow_radius) / 2;
        int ofx = padx + abs(DPI_SCALE(m_style.shadow_offset_x)) + shadow_gap >
                          abs(DPI_SCALE(m_style.margin_x))
                      ? padx + abs(DPI_SCALE(m_style.shadow_offset_x)) +
                            shadow_gap - abs(DPI_SCALE(m_style.margin_x))
                      : 0;
        int ofy = pady + abs(DPI_SCALE(m_style.shadow_offset_y)) + shadow_gap >
                          abs(DPI_SCALE(m_style.margin_y))
                      ? pady + abs(DPI_SCALE(m_style.shadow_offset_y)) +
                            shadow_gap - abs(DPI_SCALE(m_style.margin_y))
                      : 0;
        crc.DeflateRect(m_layout->offsetX - ofx, m_layout->offsetY - ofy);
      }
      _CaptureRect(crc);
    }
  }
  // page buttons, and click to select
  {
    if (!m_style.inline_preedit && m_candidateCount != 0 &&
        COLORNOTTRANSPARENT(m_style.prevpage_color) &&
        COLORNOTTRANSPARENT(m_style.nextpage_color)) {
      // click prepage
      if (m_ctx.cinfo.currentPage != 0) {
        CRect prc = m_layout->GetPrepageRect();
        if (m_istorepos)
          prc.OffsetRect(0, m_offsety_preedit);
        if (prc.PtInRect(point)) {
          bool nextPage = false;
          if (m_uiCallback)
            m_uiCallback(NULL, NULL, &nextPage, NULL);
          return 0;
        }
      }
      // click nextpage
      if (!m_ctx.cinfo.is_last_page) {
        CRect prc = m_layout->GetNextpageRect();
        if (m_istorepos)
          prc.OffsetRect(0, m_offsety_preedit);
        if (prc.PtInRect(point)) {
          bool nextPage = true;
          if (m_uiCallback)
            m_uiCallback(NULL, NULL, &nextPage, NULL);
          return 0;
        }
      }
    }
    // select by click relative actions
    for (size_t i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
      auto rect = _GetInflatedCandRect(i);
      if (rect.PtInRect(point)) {
        m_bar_scale = 0.8f;
        //  modify highlighted
        if (i != m_ctx.cinfo.highlighted) {
          if (m_uiCallback)
            m_uiCallback(NULL, &i, NULL, NULL);
        } else {
          RedrawWindow();
        }
        m_clickTimer = ::SetTimer(m_hWnd, AUTOREV_TIMER, 1000, NULL);
        return 0;
      }
    }
  }
  return 0;
}

LRESULT CALLBACK WeaselPanel::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                         LPARAM lParam) {
  if (uMsg == WM_NCCREATE) {
    auto self = static_cast<WeaselPanel *>(
        reinterpret_cast<LPCREATESTRUCT>(lParam)->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  auto self =
      reinterpret_cast<WeaselPanel *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  if (self)
    return self->MsgHandler(hwnd, uMsg, wParam, lParam);
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT WeaselPanel::MsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam) {
  switch (uMsg) {
  case WM_PAINT:
    DoPaint();
    break;
  case WM_DESTROY:
    OnDestroy();
    return 0;
  case WM_NCDESTROY:
    SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  case WM_MOUSEMOVE:
    OnMouseMove(uMsg, wParam, lParam);
    break;
  case WM_MOUSEACTIVATE:
    return OnMouseActive(uMsg, wParam, lParam);
  case WM_MOUSEWHEEL:
    return OnScroll(uMsg, wParam, lParam);
  case WM_LBUTTONUP:
    return OnLeftClickUp(uMsg, wParam, lParam);
  case WM_LBUTTONDOWN:
    return OnLeftClickDown(uMsg, wParam, lParam);
  case WM_TIMER:
    if (wParam == AUTOREV_TIMER) {
      ::KillTimer(m_hWnd, AUTOREV_TIMER);
      m_clickTimer = 0;
      m_bar_scale = 1.0f;
      InvalidateRect(m_hWnd, nullptr, TRUE);
      return 0;
    } else if (wParam == AUTOHIDE_TIMER) {
      ::KillTimer(m_hWnd, AUTOHIDE_TIMER);
      m_autoHideTimer = 0;
      // hide the panel on auto-hide
      ShowWindow(SW_HIDE);
      return 0;
    }
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
