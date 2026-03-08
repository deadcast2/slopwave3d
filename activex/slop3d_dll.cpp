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

static void DeleteRegTree(HKEY hRoot, const char* subkey) {
    RegDeleteKeyA(hRoot, subkey);
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

    /* CLSID entry */
    wsprintfA(key, "CLSID\\%s", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, DESCRIPTION);

    /* InprocServer32 */
    wsprintfA(key, "CLSID\\%s\\InprocServer32", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, g_szModulePath);
    SetRegKey(HKEY_CLASSES_ROOT, key, "ThreadingModel", "Apartment");

    /* ProgID */
    wsprintfA(key, "CLSID\\%s\\ProgID", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, PROGID);

    /* Control marker */
    wsprintfA(key, "CLSID\\%s\\Control", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, NULL);

    /* MiscStatus */
    wsprintfA(key, "CLSID\\%s\\MiscStatus", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, "0");
    wsprintfA(key, "CLSID\\%s\\MiscStatus\\1", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, "131473"); /* OLEMISC flags */

    /* Implemented Categories */
    wsprintfA(key, "CLSID\\%s\\Implemented Categories\\{40FC6ED4-2438-11cf-A3DB-080036F12502}", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, NULL);

    /* Safe for Scripting */
    wsprintfA(key, "CLSID\\%s\\Implemented Categories\\{7DD95801-9882-11CF-9FA9-00AA006C42C4}", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, NULL);

    /* Safe for Initializing */
    wsprintfA(key, "CLSID\\%s\\Implemented Categories\\{7DD95802-9882-11CF-9FA9-00AA006C42C4}", CLSID_STR);
    SetRegKey(HKEY_CLASSES_ROOT, key, NULL, NULL);

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
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    DeleteRegTree(HKEY_CLASSES_ROOT, PROGID);

    /* Remove CLSID tree (delete leaves first, then parents) */
    wsprintfA(key, "CLSID\\%s\\Implemented Categories\\{7DD95802-9882-11CF-9FA9-00AA006C42C4}", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\Implemented Categories\\{7DD95801-9882-11CF-9FA9-00AA006C42C4}", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\Implemented Categories\\{40FC6ED4-2438-11cf-A3DB-080036F12502}", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\Implemented Categories", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\MiscStatus\\1", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\MiscStatus", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\Control", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\ProgID", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s\\InprocServer32", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);
    wsprintfA(key, "CLSID\\%s", CLSID_STR);
    DeleteRegTree(HKEY_CLASSES_ROOT, key);

    return S_OK;
}
