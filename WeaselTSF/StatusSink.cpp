module;
#include "stdafx.h"
#include "test.h"
#ifdef TEST
#ifdef _M_X64
#define WEASEL_ENABLE_LOGGING
#include "logging.h"
#endif
#endif // TEST
module WeaselTSF;

HRESULT WeaselTSF::OnStatusChange(__in ITfContext* pContext, __in DWORD dwFlags)
{
#ifdef TEST
#ifdef _M_X64
	LOG(INFO) << std::format("WeaselTSF::OnStatusChange. dwFlags = {:#x}", dwFlags);
#endif // _M_X64
#endif // TEST
	return S_OK;
}

BOOL WeaselTSF::_InitStatusSink()
{
	com_ptr<ITfSource> pSource;

	if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
	{
		return FALSE;
	}

	if (FAILED(pSource->AdviseSink(IID_ITfStatusSink, (ITfStatusSink*)this, &_dwStatusSinkCookie)))
	{
		return FALSE;
	}

	return TRUE;
}

void WeaselTSF::_UninitStatusSink()
{
	com_ptr<ITfSource> pSource;

	if (FAILED(_pThreadMgr->QueryInterface(&pSource)))
	{
		return;
	}

	if (FAILED(pSource->UnadviseSink(_dwStatusSinkCookie)))
	{
		return;
	}
}