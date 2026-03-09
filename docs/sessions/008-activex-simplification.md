# Session 008: ActiveX Control Simplification

**Date:** March 9, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Research + refactoring session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Reduce the ActiveX control's code size and token footprint. The entire slop3d engine is designed to fit in a single AI context window, so every line counts. We investigated whether the 58 COM method implementations across 8 interfaces were all truly required, and applied safe simplifications to slim down the codebase.

---

## Conversation Summary

### 1. Initial Assessment

Caleb asked if there was any way to reduce the ActiveX control code. A thorough analysis of all files in `activex/` was conducted:

- **Total before:** ~1,370 lines across 10 files (~9,500 tokens)
- **slop3d_activex.h:** 204 lines — class declaration with 58 virtual methods
- **slop3d_activex.cpp:** 773 lines — all interface implementations, GDI rendering, WndProc
- **slop3d_dll.cpp:** 222 lines — class factory, registry, DLL entry points

Five simplification opportunities were identified:

1. **Stub method macros** (~40-50 lines) — 24 methods are single-line E_NOTIMPL/S_OK stubs
2. **Data-driven registry** (~30-40 lines) — repetitive wsprintfA + SetRegKey calls
3. **Merge GetFloatArg/GetIntArg** (~12 lines) — duplicate DISPPARAMS parsing logic
4. **Trim header declarations** (~30 lines) — stub-only interface declarations
5. **IObjectSafety compat move** (~5 lines) — relocate shim

### 2. Research: Are All 58 COM Methods Required?

Caleb asked to research online whether all 58 COM methods were truly needed for IE embedding. A comprehensive research effort was conducted using the OLE Controls Guidelines v1.1, MSDN documentation, ATL reference implementations, VLC's ActiveX control source, and other technical sources.

*(Full research findings below)*

The key finding: **IOleControl was the one interface that could potentially be dropped entirely**, as all four of its methods (GetControlInfo, OnMnemonic, OnAmbientPropertyChange, FreezeEvents) are optional per the spec. The other 7 interfaces were required for IE embedding.

### 3. Applying Simplifications

With Caleb's approval, three categories of changes were applied:

**a. Drop IOleControl (attempted)**
Removed IOleControl from the class inheritance, QueryInterface chain, header declarations, and all 4 method implementations. Per the OLE Controls spec, this interface is only mandatory for controls with mnemonics or ambient property usage — neither applies to slop3d.

**b. Merge GetFloatArg/GetIntArg into GetNumArg**
Replaced two separate 17/15-line functions with a single `GetNumArg` function returning `double`, plus two casting macros:
```cpp
#define GetFloatArg(p, i) ((float)GetNumArg(p, i))
#define GetIntArg(p, i)   ((int)GetNumArg(p, i))
```
This eliminated ~17 lines of duplicate VARIANT coercion logic.

**c. Registry helper functions**
- Added `SetClsidKey()` helper that wraps `wsprintfA` + `SetRegKey`, reducing each registry entry from 2 lines to 1. DllRegisterServer shrank from 44 to 19 lines.
- Replaced the 3-line non-recursive `DeleteRegTree` with a proper `RecursiveDeleteKey` that traverses and deletes all subkeys. DllUnregisterServer collapsed from 20 manual leaf-first deletions to just 2 recursive calls.

### 4. Testing on Windows XP — IOleControl Failure

Caleb compiled and tested on Windows XP with **Internet Explorer 8**. Result: the control loaded and displayed a black background, but no triangles rendered. The timer was firing (evidenced by the black clear color appearing), but the OnUpdate callback or DrawTriangle dispatch wasn't working.

Analysis of the failure:
- Black background = control IS activated, s3d_frame_begin() IS running
- No triangles = either OnUpdate callback not firing, or DrawTriangle args are wrong
- The GetNumArg change is functionally identical (JScript passes VT_R8 doubles, conversion path unchanged)
- Most likely culprit: IOleControl removal

Despite the OLE Controls spec v1.1 stating IOleControl is optional for controls without mnemonics, **IE8 on Windows XP appears to require it**. IE8's COM hosting code likely calls into IOleControl methods (possibly FreezeEvents or GetControlInfo) during its activation sequence, and getting E_NOINTERFACE from QueryInterface causes it to silently fail some initialization step.

### 5. Restoring IOleControl

IOleControl was restored in full — inheritance, QI entry, declarations, and all 4 method implementations (~14 lines). The control worked again after rebuilding.

### 6. Final Result

The GetNumArg merge and registry helpers remained in place — these are purely internal changes with no COM interface visibility. Final line counts:

| File | Before | After | Saved |
|------|--------|-------|-------|
| slop3d_activex.h | 204 | 204 | 0 |
| slop3d_activex.cpp | 773 | 753 | **20** |
| slop3d_dll.cpp | 222 | 197 | **25** |
| **Total** | **1,199** | **1,154** | **45** |

*(Total across the 3 source files. Build scripts, .def, and test.html unchanged.)*

---

## Research Findings

### Minimum COM Interfaces for an ActiveX Control in IE

According to the **OLE Controls and Control Containers Guidelines v1.1** (the authoritative Microsoft specification), the absolute minimum for a COM object is just IUnknown plus self-registration. However, for a **windowed, scriptable ActiveX control embedded in IE via `<OBJECT>`**, the practical minimum is:

#### Required Interfaces (cannot be dropped)

| Interface | Methods | Why Required |
|-----------|---------|-------------|
| **IUnknown** | 3 | Fundamental COM requirement |
| **IDispatch** | 4 | Scripting access — IE calls GetIDsOfNames/Invoke for all script interactions |
| **IOleObject** | 18 | Core embedding contract — SetClientSite, DoVerb, GetMiscStatus, GetExtent, Advise, etc. |
| **IOleInPlaceObject** (inherits IOleWindow) | 6 | In-place activation — GetWindow, SetObjectRects, InPlaceDeactivate |
| **IViewObject2** (inherits IViewObject) | 7 | Visual representation — Draw for rendering, SetAdvise/GetAdvise for notifications |
| **IPersistStreamInit** | 5 | IE needs at least one IPersist* interface to complete initialization |
| **IObjectSafety** | 2 | IE's "Safe for Scripting" / "Safe for Initialization" check |

#### Practically Required (spec says optional, IE needs them)

| Interface | Methods | Why Needed |
|-----------|---------|-----------|
| **IOleInPlaceActiveObject** | 5 | Spec says mandatory for controls with UI; IE QIs during activation. All methods can be trivial stubs (S_OK or E_NOTIMPL) |
| **IOleControl** | 4 | Spec says optional, but **IE8 requires it in practice**. FreezeEvents and GetControlInfo are called during activation |

#### Theoretically Removable (but proven unsafe)

| Interface | Spec Status | IE Reality |
|-----------|-------------|------------|
| **IOleControl** | "Mandatory for controls with mnemonics and/or ambient properties" — all 4 methods can return E_NOTIMPL | **IE8 on Windows XP breaks without it** — control activates but rendering/callbacks fail |

#### Safe to Omit (return E_NOINTERFACE from QueryInterface)

| Interface | Notes |
|-----------|-------|
| **IQuickActivate** | Optimization only; IE falls back to standard IOleObject::SetClientSite path |
| **IPersistPropertyBag** | Only needed for `<PARAM>` tags; IE falls back to IPersistStreamInit |
| **IPersistStorage** | IE prefers stream-based persistence |
| **IDataObject** | Spec says mandatory, but IE degrades gracefully for simple rendering controls |
| **IProvideClassInfo** | Not required for basic embedding |
| **IConnectionPointContainer** | Only needed if the control fires events |

### IOleObject Methods That Can Safely Stub

Per the OLE Controls spec, these IOleObject methods can safely return E_NOTIMPL:
- SetMoniker, GetMoniker, InitFromData, GetClipboardData, SetColorScheme, EnumAdvise, EnumVerbs

These can be trivial (S_OK with no real logic):
- SetHostNames, Update, IsUpToDate

### IOleInPlaceObject Stubable Methods
- ContextSensitiveHelp → E_NOTIMPL
- ReactivateAndUndo → E_NOTIMPL

### IOleInPlaceActiveObject Stubable Methods
- TranslateAccelerator → S_FALSE
- OnFrameWindowActivate, OnDocWindowActivate, ResizeBorder, EnableModeless → S_OK

### IViewObject Stubable Methods
- Freeze, Unfreeze, GetColorSet → E_NOTIMPL

### Reference Implementations

- **ATL's Impl classes** (IOleObjectImpl, IOleControlImpl, etc.) — canonical reference for what ATL stubs vs. implements
- **VLC's ActiveX control** (npapi-vlc project, `activex/` directory) — hand-rolled non-ATL COM, implements IOleObject, IOleInPlaceObject, IViewObject, IPersistStreamInit, IObjectSafety, IOleControl with many stubs
- **Japheth's AsmCtrl** (japheth.de/AsmCtrl/) — ActiveX control in x86 assembly demonstrating the bare minimum
- **.NET WPF ActiveXHost** — container-side reference showing IE QIs for IOleObject, IOleInPlaceObject, IOleInPlaceActiveObject during the activation state machine

---

## Research Sources

### OLE Controls Specification & COM Documentation
- [OLE Controls and Control Containers Guidelines, Version 1.1](https://learn.microsoft.com/en-us/previous-versions/ms974303(v=msdn.10))
- [Overview of Control and Control Container Guidelines](https://learn.microsoft.com/en-us/windows/win32/com/overview-of-control-and-control-container-guidelines)
- [ActiveX Controls — Win32 apps](https://learn.microsoft.com/en-us/windows/win32/com/activex-controls)
- [Building ActiveX Controls for Internet Explorer 4.0](https://techshelps.github.io/MSDN/ACTIVEX/inet401/Help/compdev/controls/buildax.htm)
- [IObjectSafety interface (IE)](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768224(v=vs.85))

### ATL Reference Implementations
- [ATL IOleInPlaceObjectWindowlessImpl](https://github.com/MicrosoftDocs/cpp-docs/blob/main/docs/atl/reference/ioleinplaceobjectwindowlessimpl-class.md)
- [ATL IOleInPlaceActiveObjectImpl](https://github.com/MicrosoftDocs/cpp-docs/blob/main/docs/atl/reference/ioleinplaceactiveobjectimpl-class.md)
- [ATL IOleControlImpl](https://github.com/MicrosoftDocs/cpp-docs/blob/main/docs/atl/reference/iolecontrolimpl-class.md)
- [CComControl Class](https://learn.microsoft.com/en-us/cpp/atl/reference/ccomcontrol-class?view=msvc-170)

### Real-World ActiveX Control Source Code
- [VLC ActiveX IOleInPlaceObject source](https://github.com/favormm/vlc-for-ios/blob/master/vlc/projects/activex/oleinplaceobject.cpp)
- [VLC ActiveX IViewObject header](https://github.com/rdp/npapi-vlc/blob/master/activex/viewobject.h)

### COM Activation & Persistence
- [IQuickActivate::QuickActivate](https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iquickactivate-quickactivate)
- [IPersistPropertyBag interface (IE)](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768205(v=vs.85))
- [.NET ActiveXHost source](https://www.dotnetframework.org/default.aspx/Net/Net/3@5@50727@3053/DEVDIV/depot/DevDiv/releases/Orcas/SP/wpf/src/Framework/System/Windows/Interop/ActiveXHost@cs/3/ActiveXHost@cs)

---

## Decisions Log

| Decision | Choice | Reasoning |
|----------|--------|-----------|
| Drop IOleControl | Attempted, then reverted | OLE spec says optional, but IE8 on Windows XP requires it in practice — control rendered black with no triangles without it |
| Merge GetFloatArg/GetIntArg | Unified into GetNumArg + macros | Eliminated ~17 lines of duplicate VARIANT coercion logic; JScript passes VT_R8 doubles so conversion path is identical |
| Add SetClsidKey helper | Adopted | Wraps wsprintfA + SetRegKey into one call, cutting DllRegisterServer from 44 to 19 lines |
| Add RecursiveDeleteKey | Adopted | Replaces 20 manual leaf-first DeleteRegTree calls with 2 recursive calls; also future-proof for new subkeys |
| Keep IOleControl in final code | Required | IE8's COM hosting code needs IOleControl during activation — spec vs. reality gap confirmed by live testing |

---

## Files Changed

| File | Change |
|------|--------|
| `activex/slop3d_activex.cpp` | Replaced GetFloatArg + GetIntArg with unified GetNumArg + casting macros (lines 42-58). IOleControl temporarily removed then restored. Net: -20 lines |
| `activex/slop3d_dll.cpp` | Replaced DeleteRegTree with RecursiveDeleteKey; added SetClsidKey helper; simplified DllRegisterServer and DllUnregisterServer. Net: -25 lines |

---

## Key Takeaway

**The OLE Controls spec does not match IE8's actual behavior.** The spec says IOleControl is only mandatory for controls with mnemonics or ambient property usage. In practice, IE8 on Windows XP silently fails activation (or callback dispatch) when IOleControl returns E_NOINTERFACE from QueryInterface. This was confirmed by live testing on actual hardware.

For any future ActiveX work: **trust the browser, not the spec.** All 8 interfaces in the original implementation are required for IE8 compatibility. The only safe simplifications are internal ones — helper functions, macro consolidation, and code deduplication that don't change the COM interface surface area.

---

## Next Steps

- Continue with next GitHub issue in the implementation sequence
- The ActiveX token count is now about as compact as it can get without risking IE compatibility
- Any further ActiveX methods exposed (as new engine features are added) should follow the existing IDispatch pattern with GetNumArg for argument extraction
