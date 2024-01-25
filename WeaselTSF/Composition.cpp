#include "stdafx.h"
#include "WeaselTSF.h"
#include "EditSession.h"
#include "ResponseParser.h"
#include "CandidateList.h"

/* Start Composition */
class CStartCompositionEditSession: public CEditSession
{
public:
	CStartCompositionEditSession(WeaselTSF* pTextService, ITfContext* pContext, BOOL inlinePreeditEnabled)
		: CEditSession(pTextService, pContext), _inlinePreeditEnabled(inlinePreeditEnabled)
	{

	}

	/* ITfEditSession */
	STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
	BOOL _inlinePreeditEnabled;
};

STDAPI CStartCompositionEditSession::DoEditSession(TfEditCookie ec)
{
	HRESULT hr = E_FAIL;
	com_ptr<ITfInsertAtSelection> pInsertAtSelection;
	com_ptr<ITfRange> pRangeComposition;
	if (_pContext->QueryInterface(IID_ITfInsertAtSelection, (LPVOID *) &pInsertAtSelection) != S_OK)
		return hr;
	if (pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pRangeComposition) != S_OK)
		return hr;
	
	com_ptr<ITfContextComposition> pContextComposition;
	com_ptr<ITfComposition> pComposition;
	if (_pContext->QueryInterface(IID_ITfContextComposition, (LPVOID *) &pContextComposition) != S_OK)
		return hr;
	if ((pContextComposition->StartComposition(ec, pRangeComposition, _pTextService, &pComposition) == S_OK)
		&& (pComposition != NULL))
	{
		_pTextService->_SetComposition(pComposition);

		/* WORKAROUND:
		 *   CUAS does not provide a correct GetTextExt() position unless the composition is filled with characters.
		 *   So we insert a zero width space here.
		 *   The workaround is only needed when inline preedit is not enabled.
		 *   See https://github.com/rime/weasel/pull/883#issuecomment-1567625762
		*/
		if (!_inlinePreeditEnabled)
		{
			pRangeComposition->SetText(ec, TF_ST_CORRECTION, L" ", 1);
			pRangeComposition->Collapse(ec, TF_ANCHOR_START);
		}

		/* set selection */
		TF_SELECTION tfSelection;
		tfSelection.range = pRangeComposition;
		tfSelection.style.ase = TF_AE_NONE;
		tfSelection.style.fInterimChar = FALSE;
		_pContext->SetSelection(ec, 1, &tfSelection);
	}
	
	return hr;
}

void WeaselTSF::_StartComposition(ITfContext* pContext)
{
	com_ptr<CStartCompositionEditSession> pStartCompositionEditSession;
	pStartCompositionEditSession.Attach(new CStartCompositionEditSession(this, pContext, _cand->style().inline_preedit));
	_cand->StartUI();
	if (pStartCompositionEditSession != nullptr)
	{
		HRESULT hr;
		pContext->RequestEditSession(_tfClientId, pStartCompositionEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
		SetBit(WeaselFlag::FIRST_KEY_COMPOSITION);
	}
}

/* End Composition */
class CEndCompositionEditSession: public CEditSession
{
public:
	CEndCompositionEditSession(WeaselTSF* pTextService, ITfContext* pContext, ITfComposition* pComposition, BOOL clear = TRUE)
		: CEditSession(pTextService, pContext), _pComposition{ pComposition }, _clear{ clear }
	{

	}

	/* ITfEditSession */
	STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
	com_ptr<ITfComposition> _pComposition;
	BOOL _clear;
};

STDAPI CEndCompositionEditSession::DoEditSession(TfEditCookie ec)
{
	/* Clear the dummy text we set before, if any. */
	if (_pComposition == nullptr) return S_OK;

	if (_pTextService->GetBit(WeaselFlag::SUPPORT_DISPLAY_ATTRIBUTE))
		_pTextService->_ClearCompositionDisplayAttributes(ec, _pContext);

	ITfRange *pCompositionRange;
	if (_clear && _pComposition->GetRange(&pCompositionRange) == S_OK)
		pCompositionRange->SetText(ec, 0, L"", 0);
	
	_pComposition->EndComposition(ec);
	_pTextService->_FinalizeComposition();
	return S_OK;
}

void WeaselTSF::_EndComposition(ITfContext* pContext, BOOL clear)
{
	com_ptr<CEndCompositionEditSession> pEditSession;
	HRESULT hr;
	pEditSession.Attach(new CEndCompositionEditSession(this, pContext, _pComposition, clear));

	_cand->EndUI();
	if (pEditSession)
	{
		pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	}
}

/* Get Text Extent */
class CGetTextExtentEditSession: public CEditSession
{
public:
	CGetTextExtentEditSession(WeaselTSF* pTextService, ITfContext* pContext, ITfContextView* pContextView, ITfComposition* pComposition)
		: CEditSession(pTextService, pContext), _pContextView{ pContextView }, _pComposition{ pComposition }
	{

	}

	/* ITfEditSession */
	STDMETHODIMP DoEditSession(TfEditCookie ec);
	
private:
	void CalculatePosition(_Inout_ RECT& rc);

private:
	com_ptr<ITfContextView> _pContextView;
	com_ptr<ITfComposition> _pComposition;
};

STDAPI CGetTextExtentEditSession::DoEditSession(TfEditCookie ec)
{
	com_ptr<ITfRange> pRange;
	RECT rc;
	BOOL fClipped;
	HRESULT hr;

	if (_pComposition == nullptr || FAILED(_pComposition->GetRange(&pRange)))
	{
		// composition end
		// note: selection.range is always an empty range
		hr = _pContext->GetEnd(ec, &pRange);
	}

	if (SUCCEEDED(_pContextView->GetTextExt(ec, pRange, &rc, &fClipped)) && (rc.left != 0 || rc.top != 0))
	{
		CalculatePosition(rc);
		_pTextService->_SetCompositionPosition(rc);
	}
	else if (_pTextService->GetRect().left != 0)
	{
		_pTextService->_SetCompositionPosition(_pTextService->GetRect());
	}
	return S_OK;
}

void CGetTextExtentEditSession::CalculatePosition(RECT& rc)
{
	_pTextService->SetRect(rc);
	if (_pTextService->GetBit(WeaselFlag::INLINE_PREEDIT))				// 仅嵌入式候选框记录首码坐标
	{
		static RECT rcFirst{};
		if (_pTextService->GetBit(WeaselFlag::FIRST_KEY_COMPOSITION))	// 记录首码坐标
		{
			rcFirst = rc;
		}
		else if (_pTextService->GetBit(WeaselFlag::FOCUS_CHANGED))		// 焦点变化时记录坐标
		{
			rcFirst = rc;
			_pTextService->ResetBit(WeaselFlag::FOCUS_CHANGED);
		}
		else if (5 < abs(rcFirst.top - rc.top))							// 个别应用获取坐标时文交替出现Y轴坐标相差±5，需要过滤掉
		{
			rcFirst = rc;
		}
		else if (rc.left < rcFirst.left && rc.top != rcFirst.top)		// 换行时更新坐标
		{
			rcFirst = rc;
		}
		else															// 非首码时用首码坐标替换
		{
			rc = rcFirst;
		}
	}
}

/* Composition Window Handling */
BOOL WeaselTSF::_UpdateCompositionWindow(ITfContext* pContext)
{
	com_ptr<ITfContextView> pContextView;
	if (FAILED(pContext->GetActiveView(&pContextView)))
		return FALSE;
	com_ptr<CGetTextExtentEditSession> pEditSession;
	pEditSession.Attach(new CGetTextExtentEditSession(this, pContext, pContextView, _pComposition));
	if (pEditSession == nullptr)
	{
		return FALSE;
	}
	HRESULT hr;
	pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_SYNC | TF_ES_READ, &hr);
	return SUCCEEDED(hr);
}

void WeaselTSF::_SetCompositionPosition(RECT &rc)
{
	/* Test if rect is valid.
	 * If it is invalid during CUAS test, we need to apply CUAS workaround */
	static RECT rect{};
	if (!_fCUASWorkaroundTested)
	{
		::SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);
		_fCUASWorkaroundTested = TRUE;		
		if ((abs(rc.left - rect.left) < 2 && abs(rc.top - rect.top) < 2)
			|| (abs(rc.left - rect.right) < 5 && abs(rc.top - rect.bottom) < 5))
		{
			_fCUASWorkaroundEnabled = TRUE;
			return;
		}
	}
	if (rc.left == 0 || abs(rc.left - rect.right) < 5)
	{
		rc.left = rc.right = rect.right / 3.0;
		rc.top = rc.bottom = rect.bottom / 3.0 * 2;
	}
	m_client.UpdateInputPosition(rc);
	_cand->UpdateInputPosition(rc);
}

/* Inline Preedit */
class CInlinePreeditEditSession: public CEditSession
{
public:
	CInlinePreeditEditSession(com_ptr<WeaselTSF> pTextService, com_ptr<ITfContext> pContext, com_ptr<ITfComposition> pComposition, const std::shared_ptr<weasel::Context> context)
		: CEditSession(pTextService, pContext), _pComposition(pComposition), _context(context)
	{
	}

	/* ITfEditSession */
	STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
	com_ptr<ITfComposition> _pComposition;
	const std::shared_ptr<weasel::Context> _context;
};

STDAPI CInlinePreeditEditSession::DoEditSession(TfEditCookie ec)
{
	std::wstring preedit = _context->preedit.str;

	com_ptr<ITfRange> pRangeComposition;
	if ((_pComposition->GetRange(&pRangeComposition)) != S_OK)
		return E_FAIL;

	if ((pRangeComposition->SetText(ec, 0, preedit.c_str(), preedit.length())) != S_OK)
		return E_FAIL;

	/* TODO: Check the availability and correctness of these values */
	int sel_cursor = -1;
	for (size_t i = 0; i < _context->preedit.attributes.size(); i++)
	{
		if (_context->preedit.attributes.at(i).type == weasel::HIGHLIGHTED)
		{
			sel_cursor = _context->preedit.attributes.at(i).range.cursor;
			break;
		}
	}

	if (_pTextService->GetBit(WeaselFlag::SUPPORT_DISPLAY_ATTRIBUTE))
		_pTextService->_SetCompositionDisplayAttributes(ec, _pContext, pRangeComposition);

	/* Set caret */
	LONG cch;
	TF_SELECTION tfSelection;
	pRangeComposition->ShiftStart(ec, sel_cursor, &cch, NULL);
	pRangeComposition->Collapse(ec, TF_ANCHOR_START);		
	tfSelection.range = pRangeComposition;
	tfSelection.style.ase = TF_AE_NONE;
	tfSelection.style.fInterimChar = FALSE;
	_pContext->SetSelection(ec, 1, &tfSelection);

	return S_OK;
}

BOOL WeaselTSF::_ShowInlinePreedit(ITfContext* pContext, const std::shared_ptr<weasel::Context> context)
{
	com_ptr<CInlinePreeditEditSession> pEditSession;
	pEditSession.Attach(new CInlinePreeditEditSession(this, pContext, _pComposition, context));
	if (pEditSession != NULL)
	{
		HRESULT hr;
		pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	}
	return TRUE;
}

/* Update Composition */
class CInsertTextEditSession : public CEditSession
{
public:
	CInsertTextEditSession(com_ptr<WeaselTSF> pTextService, com_ptr<ITfContext> pContext, com_ptr<ITfComposition> pComposition, const std::wstring &text)
		: CEditSession(pTextService, pContext), _text(text), _pComposition(pComposition)
	{
	}

	/* ITfEditSession */
	STDMETHODIMP DoEditSession(TfEditCookie ec);

private:
	std::wstring _text;
	com_ptr<ITfComposition> _pComposition;
};

STDMETHODIMP CInsertTextEditSession::DoEditSession(TfEditCookie ec)
{
	com_ptr<ITfRange> pRange;
	TF_SELECTION tfSelection;
	HRESULT hRet = S_OK;

	if (FAILED(_pComposition->GetRange(&pRange)))
		return E_FAIL;

	if (FAILED(pRange->SetText(ec, 0, _text.c_str(), _text.length())))
		return E_FAIL;

	/* update the selection to an insertion point just past the inserted text. */
	pRange->Collapse(ec, TF_ANCHOR_END);

	tfSelection.range = pRange;
	tfSelection.style.ase = TF_AE_NONE;
	tfSelection.style.fInterimChar = FALSE;

	_pContext->SetSelection(ec, 1, &tfSelection);

	return hRet;
}

BOOL WeaselTSF::_InsertText(ITfContext* pContext, const std::wstring& text)
{
	com_ptr<CInsertTextEditSession> pEditSession;
	pEditSession.Attach(new CInsertTextEditSession(this, pContext, _pComposition, text));
	HRESULT hr;	

	if (pEditSession)
	{
		pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	}

	return TRUE;
}

void WeaselTSF::_UpdateComposition(ITfContext* pContext)
{
	HRESULT hr;

	_pEditSessionContext = pContext;
	
	_pEditSessionContext->RequestEditSession(_tfClientId, this, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
	_UpdateCompositionWindow(pContext);

}

/* Composition State */
STDAPI WeaselTSF::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition *pComposition)
{
	// NOTE:
	// This will be called when an edit session ended up with an empty composition string,
	// Even if it is closed normally.
	// Silly M$.

	_AbortComposition();
	return S_OK;
}

void WeaselTSF::_AbortComposition(bool clear)
{
	m_client.ClearComposition();
	if (_IsComposing()) {
		_EndComposition(_pEditSessionContext, clear);
	}
	_cand->Destroy();
}

void WeaselTSF::_FinalizeComposition()
{
	_pComposition = nullptr;
}

void WeaselTSF::_SetComposition(ITfComposition* pComposition)
{
	_pComposition = pComposition;
}

BOOL WeaselTSF::_IsComposing()
{
	return _pComposition != NULL;
}
