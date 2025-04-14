#pragma once
#include <unordered_map>
#include <shellscalingapi.h>

template <typename T>
class CDialogDpiAware : public CDialogImpl<T> {
 public:
  CDialogDpiAware() {}
  ~CDialogDpiAware() {}

 protected:
  BEGIN_MSG_MAP(T)
  MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
  END_MSG_MAP()

  void InitCtrlRects() {
    ::GetWindowRect(m_hWnd, &m_rect);
    EnumChildWindows(
        m_hWnd,
        [](HWND hWnd, LPARAM lParam) {
          auto pThis = reinterpret_cast<T*>(lParam);
          RECT rect, rcDlg;
          ::GetClientRect(pThis->m_hWnd, &rcDlg);
          ::ClientToScreen(pThis->m_hWnd, (LPPOINT)&rcDlg.left);
          ::ClientToScreen(pThis->m_hWnd, (LPPOINT)&rcDlg.right);
          ::GetWindowRect(hWnd, &rect);
          rect.left -= rcDlg.left;
          rect.right -= rcDlg.left;
          rect.top -= rcDlg.top;
          rect.bottom -= rcDlg.top;
          pThis->m_controlOriginalRects[hWnd] = rect;
          return TRUE;
        },
        reinterpret_cast<LPARAM>(this));

    UINT newDpi = GetWindowDpi();
    if (newDpi != 96) {
      float scaleFactor = (float)newDpi / (float)96.0f;
      GetWindowRect(&m_rect);
      int width = (m_rect.right - m_rect.left) * scaleFactor;
      int height = (m_rect.bottom - m_rect.top) * scaleFactor;
      SetWindowPos(nullptr, m_rect.left, m_rect.top, width, height,
                   SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOREDRAW);
      ScaleControlsAndFonts(newDpi);
      Invalidate();
    }
    m_currentDpi = newDpi;
  }

 private:
  UINT GetWindowDpi() {
    HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96, dpiY = 96;
    if (SUCCEEDED(
            GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
      if (dpiX == 0)
        dpiX = 96;
      return dpiX;
    }
    return 96;
  }

  void ScaleControlsAndFonts(UINT newDpi) {
    const float scaleFactor = static_cast<float>(newDpi) / 96;
    for (const auto& [hWnd, originalRect] : m_controlOriginalRects) {
      int newX = static_cast<int>(originalRect.left * scaleFactor);
      int newY = static_cast<int>(originalRect.top * scaleFactor);
      int newWidth = static_cast<int>((originalRect.right - originalRect.left) *
                                      scaleFactor);
      int newHeight = static_cast<int>(
          (originalRect.bottom - originalRect.top) * scaleFactor);
      ::SetWindowPos(hWnd, nullptr, newX, newY, newWidth, newHeight,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    HFONT oldFont = (HFONT)::SendMessage(m_hWnd, WM_GETFONT, 0, 0);
    LOGFONT lf;
    if (!GetObject(oldFont, sizeof(lf), &lf)) {
      return;
    }
    lf.lfHeight = static_cast<int>(lf.lfHeight * scaleFactor);
    HFONT hNewFont = CreateFontIndirect(&lf);
    if (m_currentFont) {
      DeleteObject(m_currentFont);
    }
    ::SendMessage(m_hWnd, WM_SETFONT, (WPARAM)hNewFont, TRUE);
    for (const auto& [hWnd, rect] : m_controlOriginalRects) {
      ::SendMessage(hWnd, WM_SETFONT, (WPARAM)hNewFont, TRUE);
    }
    m_currentFont = hNewFont;
  }

  LRESULT OnDpiChanged(UINT, WPARAM wParam, LPARAM lParam, BOOL&) {
    UINT newDpi = HIWORD(wParam);
    if (newDpi == m_currentDpi)
      return 0;
    float scaleFactor = (float)newDpi / (float)96.0f;
    const RECT* pNewRect = reinterpret_cast<const RECT*>(lParam);
    int width = (m_rect.right - m_rect.left) * scaleFactor;
    int height = (m_rect.bottom - m_rect.top) * scaleFactor;
    SetWindowPos(nullptr, pNewRect->left, pNewRect->top, width, height,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    ScaleControlsAndFonts(newDpi);
    m_currentDpi = newDpi;
    Invalidate();
    return 0;
  }

  UINT m_currentDpi = 96;
  RECT m_rect;
  std::unordered_map<HWND, RECT> m_controlOriginalRects;
  HFONT m_currentFont = nullptr;
};
