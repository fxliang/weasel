#pragma once

#include "resource.h"
#include "UIStyleSettings.h"
#include <CDialogDpiAware.h>

class UIStyleSettingsDialog : public CDialogDpiAware<UIStyleSettingsDialog> {
 public:
  enum { IDD = IDD_STYLE_SETTING };

  UIStyleSettingsDialog(UIStyleSettings* settings);
  ~UIStyleSettingsDialog();

 protected:
  BEGIN_MSG_MAP(UIStyleSettingsDialog)
  CHAIN_MSG_MAP(CDialogDpiAware<UIStyleSettingsDialog>)
  MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
  MESSAGE_HANDLER(WM_CLOSE, OnClose)
  COMMAND_ID_HANDLER(IDOK, OnOK)
  COMMAND_ID_HANDLER(IDC_SELECT_FONT, OnSelectFont)
  COMMAND_HANDLER(IDC_COLOR_SCHEME, LBN_SELCHANGE, OnColorSchemeSelChange)
  END_MSG_MAP()

  LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnClose(UINT, WPARAM, LPARAM, BOOL&);
  LRESULT OnOK(WORD, WORD code, HWND, BOOL&);
  LRESULT OnSelectFont(WORD, WORD code, HWND, BOOL&);
  LRESULT OnColorSchemeSelChange(WORD, WORD, HWND, BOOL&);

  void Populate();
  void Preview(int index);

  UIStyleSettings* settings_;
  bool loaded_;
  std::vector<ColorSchemeInfo> preset_;

  CListBox color_schemes_;
  CStatic preview_;
  CImage image_;
  CButton select_font_;
};
