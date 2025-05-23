﻿#include "stdafx.h"
#include "InstallOptionsDlg.h"
#include <atlstr.h>
#include <ShlObj.h>
#pragma comment(lib, "Shell32.lib")

int uninstall(bool silent);

InstallOptionsDialog::InstallOptionsDialog()
    : installed(false), hant(false), user_dir() {}

InstallOptionsDialog::~InstallOptionsDialog() {}

LRESULT InstallOptionsDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
  cn_.Attach(GetDlgItem(IDC_RADIO_CN));
  tw_.Attach(GetDlgItem(IDC_RADIO_TW));
  remove_.Attach(GetDlgItem(IDC_REMOVE));
  default_dir_.Attach(GetDlgItem(IDC_RADIO_DEFAULT_DIR));
  custom_dir_.Attach(GetDlgItem(IDC_RADIO_CUSTOM_DIR));
  dir_.Attach(GetDlgItem(IDC_EDIT_DIR));

  CheckRadioButton(IDC_RADIO_CN, IDC_RADIO_TW,
                   (hant ? IDC_RADIO_TW : IDC_RADIO_CN));
  CheckRadioButton(
      IDC_RADIO_DEFAULT_DIR, IDC_RADIO_CUSTOM_DIR,
      (user_dir.empty() ? IDC_RADIO_DEFAULT_DIR : IDC_RADIO_CUSTOM_DIR));
  dir_.SetWindowTextW(user_dir.c_str());

  cn_.EnableWindow(!installed);
  tw_.EnableWindow(!installed);
  remove_.EnableWindow(installed);
  dir_.EnableWindow(user_dir.empty() ? FALSE : TRUE);

  button_custom_dir_.Attach(GetDlgItem(IDC_BUTTON_CUSTOM_DIR));
  button_custom_dir_.EnableWindow(user_dir.empty() ? FALSE : TRUE);

  ok_.Attach(GetDlgItem(IDOK));
  if (installed) {
    CString str;
    str.LoadStringW(IDS_STRING_MODIFY);
    ok_.SetWindowTextW(str);
  }

  ime_.Attach(GetDlgItem(IDC_CHECK_INSTIME));
  if (installed)
    ime_.EnableWindow(FALSE);

  InitCtrlRects();
  CenterWindow();
  return 0;
}

LRESULT InstallOptionsDialog::OnClose(UINT, WPARAM, LPARAM, BOOL&) {
  EndDialog(IDCANCEL);
  return 0;
}

LRESULT InstallOptionsDialog::OnOK(WORD, WORD code, HWND, BOOL&) {
  hant = (IsDlgButtonChecked(IDC_RADIO_TW) == BST_CHECKED);
  old_ime_support = (IsDlgButtonChecked(IDC_CHECK_INSTIME) == BST_CHECKED);
  if (IsDlgButtonChecked(IDC_RADIO_CUSTOM_DIR) == BST_CHECKED) {
    CStringW text;
    dir_.GetWindowTextW(text);
    user_dir = text;
  } else {
    user_dir.clear();
  }
  EndDialog(IDOK);
  return 0;
}

LRESULT InstallOptionsDialog::OnRemove(WORD, WORD code, HWND, BOOL&) {
  const bool non_silent = false;
  uninstall(non_silent);
  installed = false;
  ime_.EnableWindow(!installed);
  CString str;
  str.LoadStringW(IDS_STRING_INSTALL);
  ok_.SetWindowTextW(str);
  cn_.EnableWindow(!installed);
  tw_.EnableWindow(!installed);
  remove_.EnableWindow(installed);
  return 0;
}

LRESULT InstallOptionsDialog::OnUseDefaultDir(WORD, WORD code, HWND, BOOL&) {
  dir_.EnableWindow(FALSE);
  dir_.SetWindowTextW(L"");
  button_custom_dir_.EnableWindow(FALSE);
  return 0;
}

LRESULT InstallOptionsDialog::OnUseCustomDir(WORD, WORD code, HWND, BOOL&) {
  CShellFileOpenDialog fileOpenDlg(
      NULL, FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS);
  CStringW text;
  dir_.GetWindowTextW(text);
  if (!text.IsEmpty()) {
    PIDLIST_ABSOLUTE pidl;
    HRESULT hr = SHParseDisplayName(text, NULL, &pidl, 0, NULL);
    if (SUCCEEDED(hr)) {
      IShellItem* psi;
      hr = SHCreateShellItem(NULL, NULL, pidl, &psi);
      if (SUCCEEDED(hr)) {
        fileOpenDlg.GetPtr()->SetFolder(psi);
        psi->Release();
      }
      CoTaskMemFree(pidl);
    }
  }
  if (fileOpenDlg.DoModal(m_hWnd) == IDOK) {
    CComPtr<IShellItem> psi;
    if (SUCCEEDED(fileOpenDlg.GetPtr()->GetResult(&psi))) {
      LPWSTR path;
      if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
        dir_.SetWindowTextW(path);
        CoTaskMemFree(path);
      }
    }
  }
  ok_.SetFocus();
  return 0;
}
