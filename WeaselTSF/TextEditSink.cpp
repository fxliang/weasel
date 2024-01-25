#include "stdafx.h"
#include "WeaselTSF.h"

static BOOL IsRangeCovered(TfEditCookie ec, ITfRange *pRangeTest, ITfRange *pRangeCover)
{
	LONG lResult;

	if (pRangeCover->CompareStart(ec, pRangeTest, TF_ANCHOR_START, &lResult) != S_OK || lResult > 0)
		return FALSE;
	if (pRangeCover->CompareEnd(ec, pRangeTest, TF_ANCHOR_END, &lResult) != S_OK || lResult < 0)
		return FALSE;
	return TRUE;
}

STDAPI WeaselTSF::OnEndEdit(ITfContext *pContext, TfEditCookie ecReadOnly, ITfEditRecord *pEditRecord)
{
	BOOL fSelectionChanged;
	IEnumTfRanges *pEnumTextChanges;
	ITfRange *pRange;

	/* did the selection change? */
	if (pEditRecord->GetSelectionStatus(&fSelectionChanged) == S_OK && fSelectionChanged)
	{
		if (_IsComposing())
		{
			/* if the caret moves out of composition range, stop the composition */
			TF_SELECTION tfSelection;
			ULONG cFetched;

			if (pContext->GetSelection(ecReadOnly, TF_DEFAULT_SELECTION, 1, &tfSelection, &cFetched) == S_OK && cFetched == 1)
			{
				ITfRange *pRangeComposition;
				if (_pComposition->GetRange(&pRangeComposition) == S_OK)
				{
					if (!IsRangeCovered(ecReadOnly, tfSelection.range, pRangeComposition))
						_EndComposition(pContext, true);
					pRangeComposition->Release();
				}
			}
		}
	}

	/* text modification? */
	if (pEditRecord->GetTextAndPropertyUpdates(TF_GTP_INCL_TEXT, NULL, 0, &pEnumTextChanges) == S_OK)
	{
		ULONG fetched{};
		if (pEnumTextChanges->Next(1, &pRange, &fetched) == S_OK)
		{
			if (fetched == 0)
			{
				_UpdateCompositionWindow(pContext);
			}
			pRange->Release();
		}
		pEnumTextChanges->Release();
	}
	return S_OK;
}

STDAPI WeaselTSF::OnLayoutChange(ITfContext *pContext, TfLayoutCode lcode, ITfContextView *pContextView)
{
	if (!_IsComposing())
		return S_OK;

	if (pContext != _pTextEditSinkContext)
		return S_OK;

	if (lcode == TF_LC_CHANGE)
		_UpdateCompositionWindow(pContext);
	return S_OK;
}

BOOL WeaselTSF::_InitTextEditSink(ITfDocumentMgr* pDocMgr)
{
	com_ptr<ITfSource> pSource;
	BOOL fRet{};

	/* clear out any previous sink first */
	if (_dwTextEditSinkCookie != TF_INVALID_COOKIE)
	{
		if (SUCCEEDED(_pTextEditSinkContext->QueryInterface(&pSource)) && pSource != nullptr)
		{
			pSource->UnadviseSink(_dwTextEditSinkCookie);
			pSource->UnadviseSink(_dwTextLayoutSinkCookie);
		}
		_pTextEditSinkContext.Release();
		_dwTextEditSinkCookie = TF_INVALID_COOKIE;
		_dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
	}
	if (pDocMgr == NULL)
		return TRUE;

	if (pDocMgr->GetTop(&_pTextEditSinkContext) != S_OK)
		return FALSE;

	if (_pTextEditSinkContext == NULL)
		return TRUE;

	if (SUCCEEDED(_pTextEditSinkContext->QueryInterface(&pSource)))
	{
		if (SUCCEEDED(pSource->AdviseSink(IID_ITfTextEditSink, (ITfTextEditSink*) this, &_dwTextEditSinkCookie)))
			fRet = TRUE;
		else
			_dwTextEditSinkCookie = TF_INVALID_COOKIE;
		if (SUBLANGID(pSource->AdviseSink(IID_ITfTextLayoutSink, (ITfTextLayoutSink*) this, &_dwTextLayoutSinkCookie)))
		{
			fRet = TRUE;
		}
		else
			_dwTextLayoutSinkCookie = TF_INVALID_COOKIE;
	}
	if (fRet == FALSE)
	{
		_pTextEditSinkContext.Release();
	}

	return fRet;
}
