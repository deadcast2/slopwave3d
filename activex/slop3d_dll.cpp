/*
 * slop3d_dll.cpp — DLL entry points, class factory, and registration
 */

#define INITGUID  /* instantiate GUIDs in this translation unit */
#include "slop3d_activex.h"

/* ── Globals ────────────────────────────────────────────────────────── */

LONG      g_objectCount = 0;
LONG      g_lockCount   = 0;
HINSTANCE g_hInstance    = NULL;

static char g_szModulePath[MAX_PATH] = {0};

/* String versions of GUIDs for registry (must match slop3d_guids.h) */
static const char CLSID_STR[]   = "{A1B2C3D4-E5F6-4A7B-8C9D-0E1F2A3B4C5D}";
static const char PROGID[]      = "Slop3D.Control";
static const char DESCRIPTION[] = "slop3d 3D Engine ActiveX Control";

/* ── Class Factory ──────────────────────────────────────────────────── */

class CSlop3DClassFactory : public IClassFactory
{
public:
    CSlop3DClassFactory() : m_refCount(1) {}

    /* IUnknown */
    STDMETHOD(QueryInterface)(REFIID riid, void** ppv) {
        if (!ppv) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IClassFactory) {
            *ppv = (IClassFactory*)this;
            AddRef();
            return S_OK;
        }
        *ppv = NULL;
        return E_NOINTERFACE;
    }

    STDMETHOD_(ULONG, AddRef)() {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHOD_(ULONG, Release)() {
        LONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) { delete this; return 0; }
        return count;
    }

    /* IClassFactory */
    STDMETHOD(CreateInstance)(IUnknown* pUnkOuter, REFIID riid, void** ppv) {
        CSlop3DControl* pCtrl;
        HRESULT hr;

        if (!ppv) return E_POINTER;
        *ppv = NULL;
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;

        pCtrl = new CSlop3DControl();
        if (!pCtrl) return E_OUTOFMEMORY;

        hr = pCtrl->QueryInterface(riid, ppv);
        pCtrl->Release(); /* QI AddRef'd if successful */
        return hr;
    }

    STDMETHOD(LockServer)(BOOL fLock) {
        if (fLock) InterlockedIncrement(&g_lockCount);
        else       InterlockedDecrement(&g_lockCount);
        return S_OK;
    }

private:
    LONG m_refCount;
};

/* ── Registry helpers ───────────────────────────────────────────────── */

static BOOL SetRegKey(HKEY hRoot, const char* subkey, const char* valueName,
                      const char* value) {
    HKEY hKey;
    LONG result = RegCreateKeyExA(hRoot, subkey, 0, NULL,
                                   REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL,
                                   &hKey, NULL);
    if (result != ERROR_SUCCESS) return FALSE;
    if (value) {
        RegSetValueExA(hKey, valueName, 0, REG_SZ,
                       (const BYTE*)value, lstrlenA(value) + 1);
    }
    RegCloseKey(hKey);
    return TRUE;
}

static void RecursiveDeleteKey(HKEY hRoot, const char* subkey) {
    char child[256];
    HKEY hKey;
    DWORD size;
    if (RegOpenKeyExA(hRoot, subkey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    size = sizeof(child);
    while (RegEnumKeyExA(hKey, 0, child, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
        char full[512];
        wsprintfA(full, "%s\\%s", subkey, child);
        RecursiveDeleteKey(hRoot, full);
        size = sizeof(child);
    }
    RegCloseKey(hKey);
    RegDeleteKeyA(hRoot, subkey);
}

static BOOL SetClsidKey(const char* suffix, const char* valueName, const char* value) {
    char key[256];
    wsprintfA(key, "CLSID\\%s%s", CLSID_STR, suffix);
    return SetRegKey(HKEY_CLASSES_ROOT, key, valueName, value);
}

/* ── DLL Exports ────────────────────────────────────────────────────── */

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        g_hInstance = hInstance;
        DisableThreadLibraryCalls(hInstance);
        GetModuleFileNameA(hInstance, g_szModulePath, MAX_PATH);

        /* Register window class */
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc   = CSlop3DControl::WndProc;
        wc.hInstance      = hInstance;
        wc.lpszClassName  = SLOP3D_WND_CLASS;
        wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
        wc.style          = CS_HREDRAW | CS_VREDRAW;
        RegisterClassA(&wc);
    }
    else if (dwReason == DLL_PROCESS_DETACH) {
        UnregisterClassA(SLOP3D_WND_CLASS, g_hInstance);
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    CSlop3DClassFactory* pFactory;
    HRESULT hr;

    if (!ppv) return E_POINTER;
    *ppv = NULL;

    if (!IsEqualCLSID(rclsid, CLSID_Slop3DControl))
        return CLASS_E_CLASSNOTAVAILABLE;

    pFactory = new CSlop3DClassFactory();
    if (!pFactory) return E_OUTOFMEMORY;

    hr = pFactory->QueryInterface(riid, ppv);
    pFactory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void) {
    return (g_objectCount == 0 && g_lockCount == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer(void) {
    char key[256];

    SetClsidKey("", NULL, DESCRIPTION);
    SetClsidKey("\\InprocServer32", NULL, g_szModulePath);
    SetClsidKey("\\InprocServer32", "ThreadingModel", "Apartment");
    SetClsidKey("\\ProgID", NULL, PROGID);
    SetClsidKey("\\Control", NULL, NULL);
    SetClsidKey("\\MiscStatus", NULL, "0");
    SetClsidKey("\\MiscStatus\\1", NULL, "131473"); /* OLEMISC flags */
    SetClsidKey("\\Implemented Categories\\{40FC6ED4-2438-11cf-A3DB-080036F12502}", NULL, NULL);
    SetClsidKey("\\Implemented Categories\\{7DD95801-9882-11CF-9FA9-00AA006C42C4}", NULL, NULL);
    SetClsidKey("\\Implemented Categories\\{7DD95802-9882-11CF-9FA9-00AA006C42C4}", NULL, NULL);

    /* ProgID -> CLSID mapping */
    SetRegKey(HKEY_CLASSES_ROOT, PROGID, NULL, DESCRIPTION);
    wsprintfA(key, "%s\\CLSID", PROGID);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, CLSID_STR);

    return S_OK;
}

STDAPI DllUnregisterServer(void) {
    char key[256];

    /* Remove ProgID */
    wsprintfA(key, "%s\\CLSID", PROGID);
    RecursiveDeleteKey(HKEY_CLASSES_ROOT, key);
    RegDeleteKeyA(HKEY_CLASSES_ROOT, PROGID);

    /* Remove entire CLSID tree recursively */
    wsprintfA(key, "CLSID\\%s", CLSID_STR);
    RecursiveDeleteKey(HKEY_CLASSES_ROOT, key);

    return S_OK;
}
