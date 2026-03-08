/*
 * slop3d_activex.cpp — CSlop3DControl ActiveX implementation
 *
 * Raw COM, no ATL/MFC. Implements all interfaces needed for an
 * IE-embeddable, scriptable ActiveX control that renders the
 * slop3d software rasterizer via GDI.
 */

#include "slop3d_activex.h"
#include "slop3d_compat.h"

/* Pixels per HIMETRIC unit (approximate: 1 HIMETRIC = 0.01mm, 96 DPI) */
#define HIMETRIC_PER_INCH 2540
#define PIXELS_PER_INCH   96

/* ── Name-to-DISPID mapping table ───────────────────────────────────── */

typedef struct {
    const WCHAR* name;
    DISPID       id;
} DispIdEntry;

static const DispIdEntry g_dispIdTable[] = {
    { L"Init",           DISPID_S3D_INIT },
    { L"Shutdown",       DISPID_S3D_SHUTDOWN },
    { L"SetClearColor",  DISPID_S3D_SETCLEARCOLOR },
    { L"FrameBegin",     DISPID_S3D_FRAMEBEGIN },
    { L"SetCamera",      DISPID_S3D_SETCAMERA },
    { L"SetCameraFov",   DISPID_S3D_SETCAMERAFOV },
    { L"SetCameraClip",  DISPID_S3D_SETCAMERACLIP },
    { L"Start",          DISPID_S3D_START },
    { L"Stop",           DISPID_S3D_STOP },
    { L"Width",          DISPID_S3D_GETWIDTH },
    { L"Height",         DISPID_S3D_GETHEIGHT },
    { L"OnUpdate",       DISPID_S3D_ONUPDATE },
    { L"DrawTriangle",   DISPID_S3D_DRAWTRIANGLE },
    { NULL, 0 }
};

/* ── Helper: extract arg from DISPPARAMS (reversed order) ───────────── */

static float GetFloatArg(DISPPARAMS* p, UINT index) {
    /* DISPPARAMS stores args in reverse: rgvarg[cArgs-1] is first arg */
    VARIANT* pv = &p->rgvarg[p->cArgs - 1 - index];
    if (pv->vt == VT_R4) return pv->fltVal;
    if (pv->vt == VT_R8) return (float)pv->dblVal;
    if (pv->vt == VT_I4) return (float)pv->lVal;
    if (pv->vt == VT_I2) return (float)pv->iVal;
    /* Try coercing */
    {
        VARIANT converted;
        VariantInit(&converted);
        if (SUCCEEDED(VariantChangeType(&converted, pv, 0, VT_R4))) {
            return converted.fltVal;
        }
    }
    return 0.0f;
}

static int GetIntArg(DISPPARAMS* p, UINT index) {
    VARIANT* pv = &p->rgvarg[p->cArgs - 1 - index];
    if (pv->vt == VT_I4) return pv->lVal;
    if (pv->vt == VT_I2) return pv->iVal;
    if (pv->vt == VT_R8) return (int)pv->dblVal;
    if (pv->vt == VT_R4) return (int)pv->fltVal;
    {
        VARIANT converted;
        VariantInit(&converted);
        if (SUCCEEDED(VariantChangeType(&converted, pv, 0, VT_I4))) {
            return converted.lVal;
        }
    }
    return 0;
}

/* ══════════════════════════════════════════════════════════════════════
 * CSlop3DControl
 * ══════════════════════════════════════════════════════════════════════ */

CSlop3DControl::CSlop3DControl()
    : m_refCount(1),
      m_pClientSite(NULL),
      m_pInPlaceSite(NULL),
      m_pAdviseHolder(NULL),
      m_pViewAdviseSink(NULL),
      m_dwViewAdviseFlags(0),
      m_hwnd(NULL),
      m_hwndParent(NULL),
      m_hdcMem(NULL),
      m_hbmDIB(NULL),
      m_hbmOld(NULL),
      m_dibBits(NULL),
      m_timerId(0),
      m_running(FALSE),
      m_initialized(FALSE),
      m_pendingStart(FALSE),
      m_pOnUpdate(NULL)
{
    SetRect(&m_rcPos, 0, 0, S3D_WIDTH * 2, S3D_HEIGHT * 2);

    /* Default extent: 640x480 pixels in HIMETRIC */
    m_sizeExtent.cx = (S3D_WIDTH * 2) * HIMETRIC_PER_INCH / PIXELS_PER_INCH;
    m_sizeExtent.cy = (S3D_HEIGHT * 2) * HIMETRIC_PER_INCH / PIXELS_PER_INCH;

    InterlockedIncrement(&g_objectCount);
}

CSlop3DControl::~CSlop3DControl() {
    if (m_running) {
        KillTimer(m_hwnd, m_timerId);
        m_running = FALSE;
    }
    if (m_initialized) {
        s3d_shutdown();
    }
    DestroyDIB();
    if (m_pClientSite) m_pClientSite->Release();
    if (m_pInPlaceSite) m_pInPlaceSite->Release();
    if (m_pAdviseHolder) m_pAdviseHolder->Release();
    if (m_pOnUpdate) m_pOnUpdate->Release();
    if (m_pViewAdviseSink) m_pViewAdviseSink->Release();
    if (m_hwnd) DestroyWindow(m_hwnd);

    InterlockedDecrement(&g_objectCount);
}

/* ── IUnknown ───────────────────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;

    if (riid == IID_IUnknown)
        *ppv = (IDispatch*)this;
    else if (riid == IID_IDispatch)
        *ppv = (IDispatch*)this;
    else if (riid == IID_ISlop3D)
        *ppv = (IDispatch*)this;
    else if (riid == IID_IOleObject)
        *ppv = (IOleObject*)this;
    else if (riid == IID_IOleWindow || riid == IID_IOleInPlaceObject)
        *ppv = (IOleInPlaceObject*)this;
    else if (riid == IID_IOleInPlaceActiveObject)
        *ppv = (IOleInPlaceActiveObject*)this;
    else if (riid == IID_IOleControl)
        *ppv = (IOleControl*)this;
    else if (riid == IID_IViewObject || riid == IID_IViewObject2)
        *ppv = (IViewObject2*)this;
    else if (riid == IID_IPersist || riid == IID_IPersistStreamInit)
        *ppv = (IPersistStreamInit*)this;
    else if (riid == IID_IObjectSafety)
        *ppv = (IObjectSafety*)this;
    else {
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CSlop3DControl::AddRef() {
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) CSlop3DControl::Release() {
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0) {
        delete this;
        return 0;
    }
    return count;
}

/* ── IDispatch ──────────────────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::GetTypeInfoCount(UINT* pctinfo) {
    if (pctinfo) *pctinfo = 0;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetTypeInfo(UINT, LCID, ITypeInfo** ppTInfo) {
    if (ppTInfo) *ppTInfo = NULL;
    return E_NOTIMPL;
}

STDMETHODIMP CSlop3DControl::GetIDsOfNames(REFIID, LPOLESTR* rgszNames,
                                             UINT cNames, LCID, DISPID* rgDispId) {
    UINT i;
    if (!rgszNames || !rgDispId) return E_POINTER;

    for (i = 0; i < cNames; i++) {
        const DispIdEntry* entry;
        BOOL found = FALSE;
        for (entry = g_dispIdTable; entry->name != NULL; entry++) {
            if (lstrcmpiW(rgszNames[i], entry->name) == 0) {
                rgDispId[i] = entry->id;
                found = TRUE;
                break;
            }
        }
        if (!found) {
            rgDispId[i] = DISPID_UNKNOWN;
            return DISP_E_UNKNOWNNAME;
        }
    }
    return S_OK;
}

STDMETHODIMP CSlop3DControl::Invoke(DISPID dispId, REFIID, LCID, WORD wFlags,
                                     DISPPARAMS* pDispParams, VARIANT* pVarResult,
                                     EXCEPINFO*, UINT*) {
    switch (dispId) {

    case DISPID_S3D_INIT:
        s3d_init();
        m_initialized = TRUE;
        return S_OK;

    case DISPID_S3D_SHUTDOWN:
        s3d_shutdown();
        m_initialized = FALSE;
        return S_OK;

    case DISPID_S3D_SETCLEARCOLOR:
        if (pDispParams->cArgs < 4) return DISP_E_BADPARAMCOUNT;
        s3d_clear_color(
            (uint8_t)(GetIntArg(pDispParams, 0) & 0xFF),
            (uint8_t)(GetIntArg(pDispParams, 1) & 0xFF),
            (uint8_t)(GetIntArg(pDispParams, 2) & 0xFF),
            (uint8_t)(GetIntArg(pDispParams, 3) & 0xFF));
        return S_OK;

    case DISPID_S3D_FRAMEBEGIN:
        s3d_frame_begin();
        return S_OK;

    case DISPID_S3D_SETCAMERA:
        if (pDispParams->cArgs < 9) return DISP_E_BADPARAMCOUNT;
        s3d_camera_set(
            GetFloatArg(pDispParams, 0), GetFloatArg(pDispParams, 1),
            GetFloatArg(pDispParams, 2), GetFloatArg(pDispParams, 3),
            GetFloatArg(pDispParams, 4), GetFloatArg(pDispParams, 5),
            GetFloatArg(pDispParams, 6), GetFloatArg(pDispParams, 7),
            GetFloatArg(pDispParams, 8));
        return S_OK;

    case DISPID_S3D_SETCAMERAFOV:
        if (pDispParams->cArgs < 1) return DISP_E_BADPARAMCOUNT;
        s3d_camera_fov(GetFloatArg(pDispParams, 0));
        return S_OK;

    case DISPID_S3D_SETCAMERACLIP:
        if (pDispParams->cArgs < 2) return DISP_E_BADPARAMCOUNT;
        s3d_camera_clip(GetFloatArg(pDispParams, 0), GetFloatArg(pDispParams, 1));
        return S_OK;

    case DISPID_S3D_START:
        if (!m_initialized) {
            s3d_init();
            m_initialized = TRUE;
        }
        if (m_hwnd && !m_running) {
            m_timerId = SetTimer(m_hwnd, 1, 33, NULL); /* ~30fps */
            m_running = TRUE;
        } else if (!m_hwnd) {
            m_pendingStart = TRUE;
        }
        return S_OK;

    case DISPID_S3D_STOP:
        if (m_hwnd && m_running) {
            KillTimer(m_hwnd, m_timerId);
            m_running = FALSE;
        }
        return S_OK;

    case DISPID_S3D_GETWIDTH:
        if (pVarResult) {
            VariantInit(pVarResult);
            pVarResult->vt = VT_I4;
            pVarResult->lVal = s3d_get_width();
        }
        return S_OK;

    case DISPID_S3D_GETHEIGHT:
        if (pVarResult) {
            VariantInit(pVarResult);
            pVarResult->vt = VT_I4;
            pVarResult->lVal = s3d_get_height();
        }
        return S_OK;

    case DISPID_S3D_DRAWTRIANGLE:
        if (pDispParams->cArgs < 18) return DISP_E_BADPARAMCOUNT;
        s3d_draw_triangle(
            GetFloatArg(pDispParams, 0),  GetFloatArg(pDispParams, 1),
            GetFloatArg(pDispParams, 2),  GetFloatArg(pDispParams, 3),
            GetFloatArg(pDispParams, 4),  GetFloatArg(pDispParams, 5),
            GetFloatArg(pDispParams, 6),  GetFloatArg(pDispParams, 7),
            GetFloatArg(pDispParams, 8),  GetFloatArg(pDispParams, 9),
            GetFloatArg(pDispParams, 10), GetFloatArg(pDispParams, 11),
            GetFloatArg(pDispParams, 12), GetFloatArg(pDispParams, 13),
            GetFloatArg(pDispParams, 14), GetFloatArg(pDispParams, 15),
            GetFloatArg(pDispParams, 16), GetFloatArg(pDispParams, 17));
        return S_OK;

    case DISPID_S3D_ONUPDATE:
        if (wFlags & DISPATCH_PROPERTYPUT) {
            /* Store JS function as IDispatch: slop3d.OnUpdate = function() {...} */
            if (pDispParams->cArgs < 1) return DISP_E_BADPARAMCOUNT;
            if (m_pOnUpdate) { m_pOnUpdate->Release(); m_pOnUpdate = NULL; }
            if (pDispParams->rgvarg[0].vt == VT_DISPATCH && pDispParams->rgvarg[0].pdispVal) {
                m_pOnUpdate = pDispParams->rgvarg[0].pdispVal;
                m_pOnUpdate->AddRef();
            }
            return S_OK;
        }
        if (wFlags & DISPATCH_PROPERTYGET) {
            if (pVarResult) {
                VariantInit(pVarResult);
                if (m_pOnUpdate) {
                    pVarResult->vt = VT_DISPATCH;
                    pVarResult->pdispVal = m_pOnUpdate;
                    m_pOnUpdate->AddRef();
                } else {
                    pVarResult->vt = VT_NULL;
                }
            }
            return S_OK;
        }
        return S_OK;
    }

    return DISP_E_MEMBERNOTFOUND;
}

/* ── IOleObject ─────────────────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::SetClientSite(IOleClientSite* pClientSite) {
    if (m_pClientSite) m_pClientSite->Release();
    m_pClientSite = pClientSite;
    if (m_pClientSite) m_pClientSite->AddRef();
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetClientSite(IOleClientSite** ppClientSite) {
    if (!ppClientSite) return E_POINTER;
    *ppClientSite = m_pClientSite;
    if (m_pClientSite) m_pClientSite->AddRef();
    return S_OK;
}

STDMETHODIMP CSlop3DControl::SetHostNames(LPCOLESTR, LPCOLESTR) { return S_OK; }

STDMETHODIMP CSlop3DControl::Close(DWORD) {
    if (m_running) {
        KillTimer(m_hwnd, m_timerId);
        m_running = FALSE;
    }
    InPlaceDeactivate();
    return S_OK;
}

STDMETHODIMP CSlop3DControl::SetMoniker(DWORD, IMoniker*) { return E_NOTIMPL; }
STDMETHODIMP CSlop3DControl::GetMoniker(DWORD, DWORD, IMoniker**) { return E_NOTIMPL; }
STDMETHODIMP CSlop3DControl::InitFromData(IDataObject*, BOOL, DWORD) { return E_NOTIMPL; }
STDMETHODIMP CSlop3DControl::GetClipboardData(DWORD, IDataObject**) { return E_NOTIMPL; }

STDMETHODIMP CSlop3DControl::DoVerb(LONG iVerb, LPMSG, IOleClientSite*,
                                     LONG, HWND hwndParent, LPCRECT lprcPosRect) {
    switch (iVerb) {
    case OLEIVERB_INPLACEACTIVATE:
    case OLEIVERB_UIACTIVATE:
    case OLEIVERB_SHOW:
    case OLEIVERB_PRIMARY:
        if (lprcPosRect) m_rcPos = *lprcPosRect;
        return InPlaceActivate(iVerb);
    case OLEIVERB_HIDE:
        if (m_hwnd) ShowWindow(m_hwnd, SW_HIDE);
        return S_OK;
    }
    return E_NOTIMPL;
}

STDMETHODIMP CSlop3DControl::EnumVerbs(IEnumOLEVERB**) { return E_NOTIMPL; }
STDMETHODIMP CSlop3DControl::Update() { return S_OK; }
STDMETHODIMP CSlop3DControl::IsUpToDate() { return S_OK; }

STDMETHODIMP CSlop3DControl::GetUserClassID(CLSID* pClsid) {
    if (!pClsid) return E_POINTER;
    *pClsid = CLSID_Slop3DControl;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetUserType(DWORD, LPOLESTR* pszUserType) {
    if (!pszUserType) return E_POINTER;
    *pszUserType = (LPOLESTR)CoTaskMemAlloc(32 * sizeof(WCHAR));
    if (!*pszUserType) return E_OUTOFMEMORY;
    lstrcpyW(*pszUserType, L"slop3d Engine");
    return S_OK;
}

STDMETHODIMP CSlop3DControl::SetExtent(DWORD dwDrawAspect, SIZEL* psizel) {
    if (dwDrawAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    if (!psizel) return E_POINTER;
    m_sizeExtent = *psizel;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetExtent(DWORD dwDrawAspect, SIZEL* psizel) {
    if (dwDrawAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    if (!psizel) return E_POINTER;
    *psizel = m_sizeExtent;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::Advise(IAdviseSink* pAdvSink, DWORD* pdwConnection) {
    if (!m_pAdviseHolder) {
        HRESULT hr = CreateOleAdviseHolder(&m_pAdviseHolder);
        if (FAILED(hr)) return hr;
    }
    return m_pAdviseHolder->Advise(pAdvSink, pdwConnection);
}

STDMETHODIMP CSlop3DControl::Unadvise(DWORD dwConnection) {
    if (!m_pAdviseHolder) return OLE_E_NOCONNECTION;
    return m_pAdviseHolder->Unadvise(dwConnection);
}

STDMETHODIMP CSlop3DControl::EnumAdvise(IEnumSTATDATA** ppenumAdvise) {
    if (!m_pAdviseHolder) return E_FAIL;
    return m_pAdviseHolder->EnumAdvise(ppenumAdvise);
}

STDMETHODIMP CSlop3DControl::GetMiscStatus(DWORD dwAspect, DWORD* pdwStatus) {
    if (!pdwStatus) return E_POINTER;
    if (dwAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    *pdwStatus = OLEMISC_ACTIVATEWHENVISIBLE |
                 OLEMISC_SETCLIENTSITEFIRST |
                 OLEMISC_INSIDEOUT |
                 OLEMISC_RECOMPOSEONRESIZE;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::SetColorScheme(LOGPALETTE*) { return E_NOTIMPL; }

/* ── IOleWindow / IOleInPlaceObject ─────────────────────────────────── */

STDMETHODIMP CSlop3DControl::GetWindow(HWND* phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = m_hwnd;
    return m_hwnd ? S_OK : E_FAIL;
}

STDMETHODIMP CSlop3DControl::ContextSensitiveHelp(BOOL) { return E_NOTIMPL; }

STDMETHODIMP CSlop3DControl::InPlaceDeactivate() {
    if (m_running) {
        KillTimer(m_hwnd, m_timerId);
        m_running = FALSE;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = NULL;
    }
    DestroyDIB();
    if (m_pInPlaceSite) {
        m_pInPlaceSite->OnInPlaceDeactivate();
        m_pInPlaceSite->Release();
        m_pInPlaceSite = NULL;
    }
    return S_OK;
}

STDMETHODIMP CSlop3DControl::UIDeactivate() { return S_OK; }

STDMETHODIMP CSlop3DControl::SetObjectRects(LPCRECT lprcPosRect, LPCRECT) {
    if (lprcPosRect) {
        m_rcPos = *lprcPosRect;
        if (m_hwnd) {
            MoveWindow(m_hwnd, m_rcPos.left, m_rcPos.top,
                       m_rcPos.right - m_rcPos.left,
                       m_rcPos.bottom - m_rcPos.top, TRUE);
        }
    }
    return S_OK;
}

STDMETHODIMP CSlop3DControl::ReactivateAndUndo() { return E_NOTIMPL; }

/* ── IOleInPlaceActiveObject ────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::TranslateAccelerator(LPMSG) { return S_FALSE; }
STDMETHODIMP CSlop3DControl::OnFrameWindowActivate(BOOL) { return S_OK; }
STDMETHODIMP CSlop3DControl::OnDocWindowActivate(BOOL) { return S_OK; }
STDMETHODIMP CSlop3DControl::ResizeBorder(LPCRECT, IOleInPlaceUIWindow*, BOOL) { return S_OK; }
STDMETHODIMP CSlop3DControl::EnableModeless(BOOL) { return S_OK; }

/* ── IOleControl ────────────────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::GetControlInfo(CONTROLINFO* pCI) {
    if (!pCI) return E_POINTER;
    pCI->cb = sizeof(CONTROLINFO);
    pCI->hAccel = NULL;
    pCI->cAccel = 0;
    pCI->dwFlags = 0;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::OnMnemonic(LPMSG) { return S_OK; }
STDMETHODIMP CSlop3DControl::OnAmbientPropertyChange(DISPID) { return S_OK; }
STDMETHODIMP CSlop3DControl::FreezeEvents(BOOL) { return S_OK; }

/* ── IViewObject / IViewObject2 ─────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::Draw(DWORD dwDrawAspect, LONG, void*,
                                   DVTARGETDEVICE*, HDC, HDC hdcDraw,
                                   LPCRECTL lprcBounds, LPCRECTL,
                                   BOOL (STDMETHODCALLTYPE *)(ULONG_PTR),
                                   ULONG_PTR) {
    if (dwDrawAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    if (!lprcBounds || !hdcDraw) return E_INVALIDARG;

    if (m_hdcMem) {
        int mode = SetStretchBltMode(hdcDraw, COLORONCOLOR);
        StretchBlt(hdcDraw,
                   lprcBounds->left, lprcBounds->top,
                   lprcBounds->right - lprcBounds->left,
                   lprcBounds->bottom - lprcBounds->top,
                   m_hdcMem, 0, 0, S3D_WIDTH, S3D_HEIGHT, SRCCOPY);
        SetStretchBltMode(hdcDraw, mode);
    }
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetColorSet(DWORD, LONG, void*, DVTARGETDEVICE*,
                                          HDC, LOGPALETTE**) {
    return E_NOTIMPL;
}

STDMETHODIMP CSlop3DControl::Freeze(DWORD, LONG, void*, DWORD*) { return E_NOTIMPL; }
STDMETHODIMP CSlop3DControl::Unfreeze(DWORD) { return E_NOTIMPL; }

STDMETHODIMP CSlop3DControl::SetAdvise(DWORD, DWORD advf, IAdviseSink* pAdvSink) {
    if (m_pViewAdviseSink) m_pViewAdviseSink->Release();
    m_pViewAdviseSink = pAdvSink;
    m_dwViewAdviseFlags = advf;
    if (m_pViewAdviseSink) m_pViewAdviseSink->AddRef();
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetAdvise(DWORD* pAspects, DWORD* pAdvf, IAdviseSink** ppAdvSink) {
    if (pAspects) *pAspects = DVASPECT_CONTENT;
    if (pAdvf) *pAdvf = m_dwViewAdviseFlags;
    if (ppAdvSink) {
        *ppAdvSink = m_pViewAdviseSink;
        if (m_pViewAdviseSink) m_pViewAdviseSink->AddRef();
    }
    return S_OK;
}

STDMETHODIMP CSlop3DControl::GetExtent(DWORD dwDrawAspect, LONG, DVTARGETDEVICE*, LPSIZEL lpsizel) {
    if (dwDrawAspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    if (!lpsizel) return E_POINTER;
    *lpsizel = m_sizeExtent;
    return S_OK;
}

/* ── IPersistStreamInit ─────────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::GetClassID(CLSID* pClassID) {
    if (!pClassID) return E_POINTER;
    *pClassID = CLSID_Slop3DControl;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::IsDirty() { return S_FALSE; }
STDMETHODIMP CSlop3DControl::Load(LPSTREAM) { return S_OK; }
STDMETHODIMP CSlop3DControl::Save(LPSTREAM, BOOL) { return S_OK; }

STDMETHODIMP CSlop3DControl::GetSizeMax(ULARGE_INTEGER* pcbSize) {
    if (pcbSize) { pcbSize->LowPart = 0; pcbSize->HighPart = 0; }
    return S_OK;
}

STDMETHODIMP CSlop3DControl::InitNew() { return S_OK; }

/* ── IObjectSafety ──────────────────────────────────────────────────── */

STDMETHODIMP CSlop3DControl::GetInterfaceSafetyOptions(REFIID, DWORD* pdwSupportedOptions,
                                                        DWORD* pdwEnabledOptions) {
    if (!pdwSupportedOptions || !pdwEnabledOptions) return E_POINTER;
    *pdwSupportedOptions = INTERFACESAFE_FOR_UNTRUSTED_CALLER |
                           INTERFACESAFE_FOR_UNTRUSTED_DATA;
    *pdwEnabledOptions = *pdwSupportedOptions;
    return S_OK;
}

STDMETHODIMP CSlop3DControl::SetInterfaceSafetyOptions(REFIID, DWORD, DWORD) {
    return S_OK;
}

/* ── In-place activation ────────────────────────────────────────────── */

HRESULT CSlop3DControl::InPlaceActivate(LONG) {
    HRESULT hr;
    RECT rcPos, rcClip;
    IOleInPlaceFrame* pFrame = NULL;
    IOleInPlaceUIWindow* pUIWindow = NULL;
    OLEINPLACEFRAMEINFO frameInfo;

    if (m_hwnd) return S_OK; /* already active */

    if (!m_pClientSite) return E_FAIL;

    hr = m_pClientSite->QueryInterface(IID_IOleInPlaceSite, (void**)&m_pInPlaceSite);
    if (FAILED(hr)) return hr;

    hr = m_pInPlaceSite->CanInPlaceActivate();
    if (hr != S_OK) {
        m_pInPlaceSite->Release();
        m_pInPlaceSite = NULL;
        return E_FAIL;
    }

    m_pInPlaceSite->OnInPlaceActivate();

    frameInfo.cb = sizeof(frameInfo);
    m_pInPlaceSite->GetWindowContext(&pFrame, &pUIWindow, &rcPos, &rcClip, &frameInfo);
    if (pFrame) pFrame->Release();
    if (pUIWindow) pUIWindow->Release();

    m_pInPlaceSite->GetWindow(&m_hwndParent);
    m_rcPos = rcPos;

    /* Create the control window */
    m_hwnd = CreateWindowExA(0, SLOP3D_WND_CLASS, "slop3d",
                             WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                             m_rcPos.left, m_rcPos.top,
                             m_rcPos.right - m_rcPos.left,
                             m_rcPos.bottom - m_rcPos.top,
                             m_hwndParent, NULL, g_hInstance, this);

    if (!m_hwnd) return E_FAIL;

    /* Set up the GDI DIB for rendering */
    CreateDIB();

    /* If Start() was called before the window existed, begin now */
    if (m_pendingStart && !m_running) {
        m_timerId = SetTimer(m_hwnd, 1, 33, NULL); /* ~30fps */
        m_running = TRUE;
        m_pendingStart = FALSE;
    }

    return S_OK;
}

/* ── GDI Rendering ──────────────────────────────────────────────────── */

void CSlop3DControl::CreateDIB() {
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = S3D_WIDTH;
    bmi.bmiHeader.biHeight = -S3D_HEIGHT; /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    m_hdcMem = CreateCompatibleDC(NULL);
    m_hbmDIB = CreateDIBSection(m_hdcMem, &bmi, DIB_RGB_COLORS,
                                 (void**)&m_dibBits, NULL, 0);
    m_hbmOld = (HBITMAP)SelectObject(m_hdcMem, m_hbmDIB);
}

void CSlop3DControl::DestroyDIB() {
    if (m_hdcMem) {
        if (m_hbmOld) SelectObject(m_hdcMem, m_hbmOld);
        if (m_hbmDIB) DeleteObject(m_hbmDIB);
        DeleteDC(m_hdcMem);
        m_hdcMem = NULL;
        m_hbmDIB = NULL;
        m_hbmOld = NULL;
        m_dibBits = NULL;
    }
}

void CSlop3DControl::CopyFramebufferToDIB() {
    uint8_t* src = s3d_get_framebuffer();
    BYTE* dst = m_dibBits;
    int i, count;

    if (!src || !dst) return;

    count = S3D_WIDTH * S3D_HEIGHT;
    for (i = 0; i < count; i++) {
        /* RGBA (engine) -> BGRA (GDI DIB) */
        dst[0] = src[2]; /* B <- R */
        dst[1] = src[1]; /* G <- G */
        dst[2] = src[0]; /* R <- B */
        dst[3] = src[3]; /* A <- A */
        src += 4;
        dst += 4;
    }
}

/* ── Window Procedure ───────────────────────────────────────────────── */

LRESULT CALLBACK CSlop3DControl::WndProc(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam) {
    CSlop3DControl* pCtrl;

    if (msg == WM_CREATE) {
        CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
        pCtrl = (CSlop3DControl*)pcs->lpCreateParams;
        SetWindowLongA(hwnd, GWL_USERDATA, (LONG)pCtrl);
        return 0;
    }

    pCtrl = (CSlop3DControl*)GetWindowLongA(hwnd, GWL_USERDATA);
    if (!pCtrl) return DefWindowProcA(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_TIMER:
        if (pCtrl->m_running) {
            /* 1. Begin frame (clear buffers) */
            s3d_frame_begin();

            /* 2. Fire OnUpdate callback — game logic runs here */
            if (pCtrl->m_pOnUpdate) {
                DISPPARAMS dp;
                memset(&dp, 0, sizeof(dp));
                pCtrl->m_pOnUpdate->Invoke(DISPID_VALUE, IID_NULL,
                    LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                    &dp, NULL, NULL, NULL);
            }

            /* 3. Copy framebuffer to GDI DIB */
            pCtrl->CopyFramebufferToDIB();

            /* 4. Repaint */
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (pCtrl->m_hdcMem) {
            RECT rc;
            int mode;
            GetClientRect(hwnd, &rc);
            mode = SetStretchBltMode(hdc, COLORONCOLOR);
            StretchBlt(hdc, 0, 0, rc.right, rc.bottom,
                       pCtrl->m_hdcMem, 0, 0, S3D_WIDTH, S3D_HEIGHT, SRCCOPY);
            SetStretchBltMode(hdc, mode);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; /* we handle all painting */

    case WM_DESTROY:
        SetWindowLongA(hwnd, GWL_USERDATA, 0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}
