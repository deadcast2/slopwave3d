/*
 * slop3d_activex.h — CSlop3DControl ActiveX class declaration
 */

#ifndef SLOP3D_ACTIVEX_H
#define SLOP3D_ACTIVEX_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ole2.h>
#include <olectl.h>
#include <oaidl.h>
#include <ocidl.h>

#include "slop3d_guids.h"

/* ── VC6 compatibility: missing types and interfaces ────────────────── */

/* ULONG_PTR doesn't exist in VC6 (added in Platform SDK for XP) */
#ifndef _W64
typedef unsigned long ULONG_PTR;
typedef long LONG_PTR;
#endif

/* IObjectSafety is not in VC6's headers */
#ifndef __IObjectSafety_INTERFACE_DEFINED__
#define __IObjectSafety_INTERFACE_DEFINED__

#define INTERFACESAFE_FOR_UNTRUSTED_CALLER  0x00000001
#define INTERFACESAFE_FOR_UNTRUSTED_DATA    0x00000002

DEFINE_GUID(IID_IObjectSafety,
    0xCB5BDC81, 0x93C1, 0x11CF,
    0x8F, 0x20, 0x00, 0x80, 0x5F, 0x2C, 0xD0, 0x64);

struct IObjectSafety : public IUnknown
{
    STDMETHOD(GetInterfaceSafetyOptions)(REFIID riid,
        DWORD* pdwSupportedOptions, DWORD* pdwEnabledOptions) = 0;
    STDMETHOD(SetInterfaceSafetyOptions)(REFIID riid,
        DWORD dwOptionSetMask, DWORD dwEnabledOptions) = 0;
};

#endif /* __IObjectSafety_INTERFACE_DEFINED__ */

/* ── The ActiveX Control ────────────────────────────────────────────── */

class CSlop3DControl :
    public IDispatch,
    public IOleObject,
    public IOleInPlaceObject,
    public IOleInPlaceActiveObject,
    public IOleControl,
    public IViewObject2,
    public IPersistStreamInit,
    public IObjectSafety
{
public:
    CSlop3DControl();
    ~CSlop3DControl();

    /* IUnknown */
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv);
    STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();

    /* IDispatch */
    STDMETHOD(GetTypeInfoCount)(UINT* pctinfo);
    STDMETHOD(GetTypeInfo)(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo);
    STDMETHOD(GetIDsOfNames)(REFIID riid, LPOLESTR* rgszNames, UINT cNames,
                             LCID lcid, DISPID* rgDispId);
    STDMETHOD(Invoke)(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                      DISPPARAMS* pDispParams, VARIANT* pVarResult,
                      EXCEPINFO* pExcepInfo, UINT* puArgErr);

    /* IOleObject */
    STDMETHOD(SetClientSite)(IOleClientSite* pClientSite);
    STDMETHOD(GetClientSite)(IOleClientSite** ppClientSite);
    STDMETHOD(SetHostNames)(LPCOLESTR szContainerApp, LPCOLESTR szContainerObj);
    STDMETHOD(Close)(DWORD dwSaveOption);
    STDMETHOD(SetMoniker)(DWORD dwWhichMoniker, IMoniker* pmk);
    STDMETHOD(GetMoniker)(DWORD dwAssign, DWORD dwWhichMoniker, IMoniker** ppmk);
    STDMETHOD(InitFromData)(IDataObject* pDataObject, BOOL fCreation, DWORD dwReserved);
    STDMETHOD(GetClipboardData)(DWORD dwReserved, IDataObject** ppDataObject);
    STDMETHOD(DoVerb)(LONG iVerb, LPMSG lpmsg, IOleClientSite* pActiveSite,
                      LONG lindex, HWND hwndParent, LPCRECT lprcPosRect);
    STDMETHOD(EnumVerbs)(IEnumOLEVERB** ppEnumOleVerb);
    STDMETHOD(Update)();
    STDMETHOD(IsUpToDate)();
    STDMETHOD(GetUserClassID)(CLSID* pClsid);
    STDMETHOD(GetUserType)(DWORD dwFormOfType, LPOLESTR* pszUserType);
    STDMETHOD(SetExtent)(DWORD dwDrawAspect, SIZEL* psizel);
    STDMETHOD(GetExtent)(DWORD dwDrawAspect, SIZEL* psizel);
    STDMETHOD(Advise)(IAdviseSink* pAdvSink, DWORD* pdwConnection);
    STDMETHOD(Unadvise)(DWORD dwConnection);
    STDMETHOD(EnumAdvise)(IEnumSTATDATA** ppenumAdvise);
    STDMETHOD(GetMiscStatus)(DWORD dwAspect, DWORD* pdwStatus);
    STDMETHOD(SetColorScheme)(LOGPALETTE* pLogpal);

    /* IOleWindow (base of IOleInPlaceObject) */
    STDMETHOD(GetWindow)(HWND* phwnd);
    STDMETHOD(ContextSensitiveHelp)(BOOL fEnterMode);

    /* IOleInPlaceObject */
    STDMETHOD(InPlaceDeactivate)();
    STDMETHOD(UIDeactivate)();
    STDMETHOD(SetObjectRects)(LPCRECT lprcPosRect, LPCRECT lprcClipRect);
    STDMETHOD(ReactivateAndUndo)();

    /* IOleInPlaceActiveObject */
    STDMETHOD(TranslateAccelerator)(LPMSG lpmsg);
    STDMETHOD(OnFrameWindowActivate)(BOOL fActivate);
    STDMETHOD(OnDocWindowActivate)(BOOL fActivate);
    STDMETHOD(ResizeBorder)(LPCRECT prcBorder, IOleInPlaceUIWindow* pUIWindow, BOOL fFrameWindow);
    STDMETHOD(EnableModeless)(BOOL fEnable);

    /* IOleControl */
    STDMETHOD(GetControlInfo)(CONTROLINFO* pCI);
    STDMETHOD(OnMnemonic)(LPMSG pMsg);
    STDMETHOD(OnAmbientPropertyChange)(DISPID dispID);
    STDMETHOD(FreezeEvents)(BOOL bFreeze);

    /* IViewObject */
    STDMETHOD(Draw)(DWORD dwDrawAspect, LONG lindex, void* pvAspect,
                    DVTARGETDEVICE* ptd, HDC hdcTargetDev, HDC hdcDraw,
                    LPCRECTL lprcBounds, LPCRECTL lprcWBounds,
                    BOOL (STDMETHODCALLTYPE *pfnContinue)(ULONG_PTR dwContinue),
                    ULONG_PTR dwContinue);
    STDMETHOD(GetColorSet)(DWORD dwDrawAspect, LONG lindex, void* pvAspect,
                           DVTARGETDEVICE* ptd, HDC hicTargetDev,
                           LOGPALETTE** ppColorSet);
    STDMETHOD(Freeze)(DWORD dwDrawAspect, LONG lindex, void* pvAspect, DWORD* pdwFreeze);
    STDMETHOD(Unfreeze)(DWORD dwFreeze);
    STDMETHOD(SetAdvise)(DWORD aspects, DWORD advf, IAdviseSink* pAdvSink);
    STDMETHOD(GetAdvise)(DWORD* pAspects, DWORD* pAdvf, IAdviseSink** ppAdvSink);

    /* IViewObject2 */
    STDMETHOD(GetExtent)(DWORD dwDrawAspect, LONG lindex, DVTARGETDEVICE* ptd, LPSIZEL lpsizel);

    /* IPersistStreamInit */
    STDMETHOD(GetClassID)(CLSID* pClassID);
    STDMETHOD(IsDirty)();
    STDMETHOD(Load)(LPSTREAM pStm);
    STDMETHOD(Save)(LPSTREAM pStm, BOOL fClearDirty);
    STDMETHOD(GetSizeMax)(ULARGE_INTEGER* pcbSize);
    STDMETHOD(InitNew)();

    /* IObjectSafety */
    STDMETHOD(GetInterfaceSafetyOptions)(REFIID riid, DWORD* pdwSupportedOptions,
                                         DWORD* pdwEnabledOptions);
    STDMETHOD(SetInterfaceSafetyOptions)(REFIID riid, DWORD dwOptionSetMask,
                                         DWORD dwEnabledOptions);

    /* Window procedure (static, called by Windows) */
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    /* In-place activation helper */
    HRESULT InPlaceActivate(LONG iVerb);
    void    CreateDIB();
    void    DestroyDIB();
    void    CopyFramebufferToDIB();

    /* Reference count */
    LONG m_refCount;

    /* Embedding */
    IOleClientSite*    m_pClientSite;
    IOleInPlaceSite*   m_pInPlaceSite;
    IOleAdviseHolder*  m_pAdviseHolder;
    IAdviseSink*       m_pViewAdviseSink;
    DWORD              m_dwViewAdviseFlags;

    /* Window */
    HWND  m_hwnd;
    HWND  m_hwndParent;
    RECT  m_rcPos;
    SIZEL m_sizeExtent;  /* in HIMETRIC */

    /* GDI rendering */
    HDC     m_hdcMem;
    HBITMAP m_hbmDIB;
    HBITMAP m_hbmOld;
    BYTE*   m_dibBits;

    /* Animation */
    UINT_PTR m_timerId;
    BOOL     m_running;
    BOOL     m_initialized;
    BOOL     m_pendingStart;

    /* OnUpdate callback (JS function stored as IDispatch) */
    IDispatch* m_pOnUpdate;
};

/* Window class name */
#define SLOP3D_WND_CLASS "Slop3DWndClass"

/* Global counters for DllCanUnloadNow */
extern LONG g_objectCount;
extern LONG g_lockCount;
extern HINSTANCE g_hInstance;

#endif /* SLOP3D_ACTIVEX_H */
