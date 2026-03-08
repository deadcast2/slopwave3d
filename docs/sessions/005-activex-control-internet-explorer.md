# Session 005: ActiveX Control for Internet Explorer

**Date:** March 7, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Build an ActiveX control that wraps the slop3d C engine so it can run in Internet Explorer on Windows XP via a `<object>` tag, exposing the same scripting API as the WASM/browser version. The user has Windows XP running on a Mac via UTM with Visual C++ 6.0 Enterprise installed.

---

## Conversation Summary

### 1. Initial Feasibility Discussion

Caleb proposed building an ActiveX control to run slop3d in Internet Explorer on Windows XP, noting he had XP running via UTM. We assessed feasibility and determined it was a strong fit because:

- The C engine (`src/slop3d.c`) is a pure software rasterizer with no GPU or platform dependencies
- The platform-specific code (display, input, asset loading) lives entirely in the JS layer
- An ActiveX control would be a different "shell" around the same C core
- This authentically mirrors the Shockwave 3D era

We mapped the architecture between the two platforms:

| Component | WASM/Browser | ActiveX/IE |
|-----------|-------------|------------|
| Engine core | `slop3d.c` (same) | `slop3d.c` (same) |
| Display | Canvas `putImageData` | GDI `BitBlt` from DIB section |
| Scripting bridge | Emscripten exports → JS | `IDispatch` → JScript |
| Asset loading | `fetch()` + `Image()` | `URLDownloadToFile` or embedded |
| Packaging | `.wasm` + `.js` | `.ocx` (COM DLL) |
| Embed tag | `<canvas>` | `<object classid="...">` |

### 2. Research Phase

Two parallel research agents investigated:

**Agent 1: C Engine API Surface** — Explored `src/slop3d.h`, `src/slop3d.c`, `js/slop3d.js`, `web/demo.js`, and `web/index.html` to document the complete 10-function public API, the JS wrapper class methods, the frame loop pattern, and the WASM memory model.

**Agent 2: ActiveX/COM Development** — Searched the web for ActiveX control development in C/C++ with Visual C++ 6.0, covering:
- Required COM interfaces (IOleObject, IOleInPlaceObject, IViewObject2, IDispatch, etc.)
- DLL exports (DllGetClassObject, DllCanUnloadNow, DllRegisterServer, DllUnregisterServer)
- IDispatch implementation for scriptable controls
- GDI rendering with CreateDIBSection and BitBlt
- IObjectSafety for IE security zones
- Registry structure for ActiveX controls
- Plain C/C++ COM without ATL/MFC

### 3. Design Phase

A Plan agent designed the implementation based on research findings. Key design decisions:

1. **C++ for COM wrapper, C engine compiled as C++** — COM vtables map naturally to C++ multiple inheritance
2. **Raw COM, no ATL/MFC** — Keeps code self-documenting and readable, matching slop3d's philosophy
3. **VC6-compatible engine port** — Separate `slop3d_vc6.cpp` with mechanical C99-to-C89 fixes
4. **WM_TIMER at ~30fps** — Era-authentic frame rate, single-threaded
5. **Connection points for events** — IE's `<script for="obj" event="...">` syntax
6. **Batch file build** — Transparent, version-controllable build script

### 4. Implementation

All files were created in an `activex/` subdirectory:

- `slop3d_vc6.h` / `slop3d_vc6.cpp` — VC6-compatible engine port
- `slop3d_guids.h` — COM GUIDs and DISPIDs
- `slop3d.idl` — COM interface definitions (not compiled, documentation only)
- `slop3d_activex.h` / `slop3d_activex.cpp` — Full COM control implementation
- `slop3d_dll.cpp` / `slop3d_dll.def` — DLL entry points, class factory, registry
- `build.bat` / `register.bat` / `unregister.bat` — Build and registration scripts
- `test.html` — IE test page with cycling color demo

The VC6 engine port required these mechanical changes from the original C99 code:
- `#include <stdint.h>` → manual typedefs (`unsigned char` for `uint8_t`, etc.)
- `static inline` → `static __forceinline`
- 17 compound literals `(S3D_Vec3){x,y,z}` → helper functions `s3d_vec3(x,y,z)`
- For-loop variable declarations hoisted to block scope
- `sqrtf`/`cosf`/`sinf`/`tanf` → `(float)sqrt()` casts

### 5. Getting Files to XP VM

The user needed to transfer files from macOS to the Windows XP VM running in UTM. We explored several options:

1. **UTM shared folder** — SPICE WebDAV sharing was enabled but not visible in XP's file system
2. **Network drive mapping** — Attempted via Map Network Drive dialog
3. **WebDAV URL** — `http://localhost:9843` worked in IE, allowing file browsing
4. **My Network Places** — Successfully added as a network place for persistent access

The user copied files from the shared folder to `C:\slop3d\` on XP for building.

### 6. First Build — VC6 Compiler Errors

The initial build produced errors:

```
slop3d_activex.h(64) : error C2504: 'IObjectSafety' : base class undefined
slop3d_activex.h(136) : error C2061: syntax error : identifier 'ULONG_PTR'
```

**Root cause:** VC6's headers don't include `IObjectSafety` (added in later Platform SDK) or `ULONG_PTR` (added for Windows XP SDK).

**Fix:** Added a VC6 compatibility section to `slop3d_activex.h`:
- `typedef unsigned long ULONG_PTR;` with `#ifndef _W64` guard
- Manual definition of `IObjectSafety` interface with its IID and safety flag constants

After the fix, the build succeeded on the first try.

### 7. Registration and First Test

The control built, registered via `regsvr32`, and loaded in Internet Explorer. The `test.html` page displayed the control at 640x480 (2x upscaled from native 320x240). The color buttons (Red, Green, Blue) worked correctly, changing the display color. However, Start/Stop had no visible effect and the cycling color animation wasn't running.

### 8. Event System Debugging

The initial implementation used COM connection points with `<script for="slop3d" event="OnFrame()">` syntax for IE event binding. This didn't work because IE needs a type library to map event names to DISPIDs — without one (we skipped midl), IE couldn't connect the `OnFrame` script handler to the connection point.

**First attempt:** Added `IProvideClassInfo2` interface with `GetGUID` returning the event interface IID. This still didn't work — IE also needs type info to map the "OnFrame" string to DISPID 1.

**Working solution:** Added a property-based callback mechanism instead of connection points. Added DISPID 12 (`OnUpdate`) as a read/write property that stores a JavaScript function as an `IDispatch*` pointer. The WM_TIMER handler calls it directly via `Invoke(DISPID_VALUE, ...)`.

Updated `test.html` to use the property approach:
```javascript
// Before (didn't work — needed type library):
<script for="slop3d" event="OnFrame()">...</script>

// After (works — direct property assignment):
engine.OnUpdate = function() { ... };
```

This mirrors the WASM API more closely (`engine.onUpdate(fn)` vs `engine.OnUpdate = fn`).

### 9. Dead Code Cleanup

With the property-based callback working, the entire connection point infrastructure became dead code. Removed ~150 lines:

- `CConnectionPoint` class — declaration (~35 lines) and implementation (~95 lines)
- `IConnectionPointContainer` interface — inheritance, QI entry, 2 method implementations
- `IProvideClassInfo2` interface — inheritance, QI entry, 2 method implementations
- `m_connectionPoint` member and initialization
- `FireOnFrame()` call in WM_TIMER
- 3 unused CATID GUIDs, event interface GUID, library GUID
- `DISPID_S3D_ONFRAME` constant, `LIBID_STR` unused string
- `slop3d.idl` file (user deleted manually — midl was never used)

### 10. Code Duplication Discussion

Caleb noted the C engine code duplication between `src/slop3d.c` and `activex/slop3d_vc6.cpp`. We identified that the only real incompatibility is C99 compound literals — if the original `src/slop3d.c` used the `s3d_vec3()`/`s3d_vec4()` helper functions instead, both builds could share the same source file. This was filed as [GitHub Issue #10](https://github.com/deadcast2/slopwave3d/issues/10).

---

## Research Findings

### ActiveX Control Architecture

An ActiveX control is a COM object packaged as a DLL that implements specific OLE interfaces for embedding, rendering, and scripting. For Internet Explorer:

**Required interfaces:**
- `IUnknown` — reference counting and interface discovery (mandatory for all COM)
- `IDispatch` — scriptability from JavaScript/JScript via `GetIDsOfNames` and `Invoke`
- `IOleObject` — embedding lifecycle, client site management, verb handling
- `IOleInPlaceObject` — windowed in-place activation, window management
- `IOleInPlaceActiveObject` — keyboard and UI activation
- `IViewObject2` — drawing for print and metafile rendering
- `IPersistStreamInit` — persistence (IE requires even if no state to persist)
- `IObjectSafety` — declares control safe for scripting and initialization

**DLL exports:** `DllGetClassObject`, `DllCanUnloadNow`, `DllRegisterServer`, `DllUnregisterServer`

### GDI Rendering for Software Rasterizers

`CreateDIBSection` creates a device-independent bitmap with direct pixel access — ideal for software rasterizers:

```c
bmi.bmiHeader.biHeight = -S3D_HEIGHT;  // negative = top-down (matches engine layout)
bmi.bmiHeader.biBitCount = 32;          // BGRA format
```

The engine framebuffer is RGBA (R at byte 0), but GDI DIBs are BGRA (B at byte 0). A per-pixel R/B channel swizzle is required during the frame copy.

`StretchBlt` with `SetStretchBltMode(hdc, COLORONCOLOR)` provides nearest-neighbor upscaling, matching the `image-rendering: pixelated` CSS in the browser version.

### IDispatch Argument Order

COM's `DISPPARAMS::rgvarg` array stores arguments in **reverse order** — `rgvarg[cArgs-1]` is the first argument, `rgvarg[0]` is the last. This is a common source of bugs in manual IDispatch implementations. Helper functions (`GetFloatArg`, `GetIntArg`) that account for this indexing make the code less error-prone.

### IE Event Binding Limitations

IE's `<script for="object" event="EventName()">` syntax requires:
1. `IProvideClassInfo2::GetGUID` to discover the default source interface
2. A type library to map event names to DISPIDs
3. `IConnectionPointContainer::FindConnectionPoint` to establish the connection

Without a type library (compiled via midl), IE cannot map event name strings to DISPIDs. Property-based callbacks (storing an `IDispatch*` function pointer) are a simpler alternative that doesn't require type library infrastructure.

### Visual C++ 6.0 Compatibility

VC6 (1998) targets C89/C++98. Key gaps from C99/modern C:
- No `<stdint.h>` — need manual typedefs
- No `ULONG_PTR` — added in Windows XP Platform SDK
- No `IObjectSafety` — added in later Platform SDK
- No C99 compound literals — the `(Type){...}` syntax is neither C89 nor C++
- `inline` keyword works in C++ mode but not C mode
- For-loop variable declarations leak scope (VC6 bug) but compile successfully in C++ mode

---

## Research Sources

### Microsoft Documentation
- [ActiveX Controls - Win32 apps](https://learn.microsoft.com/en-us/windows/win32/com/activex-controls) — ActiveX control architecture overview
- [CreateDIBSection function (wingdi.h)](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection) — DIB section creation for direct pixel access
- [IObjectSafety interface](https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa768224(v=vs.85)) — Safe for scripting/initialization
- [IClassFactory::CreateInstance](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iclassfactory-createinstance) — COM class factory pattern
- [IUnknown](https://learn.microsoft.com/en-us/windows/win32/api/unknwn/nn-unknwn-iunknown) — COM reference counting
- [ActiveX Controls Registry Information](https://learn.microsoft.com/en-us/windows/win32/com/activex-controls-registry-information) — Registry structure for controls
- [Safe Initialization and Scripting for ActiveX Controls](https://techshelps.github.io/MSDN/ACTIVEX/inet401/Help/compdev/controls/safety.htm) — IE security zone integration
- [Building ActiveX Controls for Internet Explorer 4.0](https://techshelps.github.io/MSDN/ACTIVEX/inet401/Help/compdev/controls/buildax.htm) — Windowed vs windowless controls
- [Windowless ActiveX Control Accessibility](https://learn.microsoft.com/en-us/windows/win32/winauto/windowless-activex-control-accessibility) — IOleInPlaceObjectWindowless details
- [Redistributing Visual C++ ActiveX Controls](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-activex-controls?view=msvc-170) — DLL export requirements
- [Regsvr32 tool and troubleshooting](https://support.microsoft.com/en-us/topic/how-to-use-the-regsvr32-tool-and-troubleshoot-regsvr32-error-messages-a98d960a-7392-e6fe-d90a-3f4e0cb543e5) — Control registration
- [Class Factories and Factory Templates](https://learn.microsoft.com/en-us/windows/win32/directshow/class-factories-and-factory-templates) — DllGetClassObject pattern
- [Creating the ActiveX Control Project](https://learn.microsoft.com/en-us/previous-versions/visualstudio/visual-studio-2008/4dc6ds49(v=vs.90)) — VC project setup
- [Registering Classes](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/automat/registering-classes) — ProgID naming conventions

### Community Resources
- [Wrapping a C library in COM/ActiveX](https://nachtimwald.com/2012/04/08/wrapping-a-c-library-in-comactivex/) — Pure C COM implementation patterns
- [OLE2/ActiveX without MFC or ATL](https://cplusplus.com/forum/windows/223533/) — Raw COM approach
- [Extending JScript - Dr Dobb's](http://www.drdobbs.com/jvm/extending-jscript/184410820.html) — IDispatch and JScript interaction
- [ActiveX events in JavaScript](http://rageshkrishna.com/2008/05/14/ImplementingActiveXEventsThatCanBeUsedInJavaScript.aspx) — Connection point event model
- [ActiveX Control Basics](https://sinis.ro/static/ch21b.htm) — Minimal interface requirements
- [The layout of a COM object - The Old New Thing](https://devblogs.microsoft.com/oldnewthing/20040205-00/?p=40733) — COM vtable structure
- [Double Buffering with Windows GDI](https://tuttlem.github.io/2024/10/17/double-buffering-with-windows-gdi.html) — DIB section double buffering
- [Visual C++ 6 Unleashed - Chapter 31](https://www.oreilly.com/library/view/visual-c-r-6/0672312417/ch31.html) — ATL ActiveX control creation
- [CreateDIBSection and GDI performance](https://www.experts-exchange.com/questions/10055388/CreateDIBSection-VirtualAlloc-and-GDI-performance.html) — BitBlt performance characteristics
- [How to Register/Unregister a DLL](https://www.c-sharpcorner.com/UploadFile/8911c4/how-to-register-and-unregister-a-dll-or-activex-controls-usi/) — regsvr32 usage

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | COM wrapper language | C++ (not pure C) | COM vtables map naturally to C++ multiple inheritance; engine still compiled as C++ |
| 2 | ATL/MFC usage | Raw COM, no ATL/MFC | Keeps code self-documenting, readable, fits in context window — matches slop3d philosophy |
| 3 | Engine code strategy | Separate VC6-compatible port | C99 compound literals can't be fixed with macros alone; mechanical port is cleanest |
| 4 | Animation loop | WM_TIMER at 33ms (~30fps) | Era-authentic for Shockwave 3D; single-threaded, no sync complexity |
| 5 | Build system | Batch file (`build.bat`) | Transparent, version-controllable, "one command to build" philosophy; VC6 .dsp files are opaque |
| 6 | Event mechanism | Property-based callback (`OnUpdate`) | Connection points + `<script for=...>` require type library; property approach works without midl |
| 7 | Type library | None (skip midl) | Manual IDispatch is sufficient; avoids midl dependency and type library registration complexity |
| 8 | Display scaling | StretchBlt + COLORONCOLOR | Nearest-neighbor upscaling matches `image-rendering: pixelated` in browser version |
| 9 | Framebuffer copy | Per-pixel RGBA→BGRA swizzle | Engine uses RGBA byte order; GDI DIBs use BGRA — channel swap required |
| 10 | Dead code cleanup | Remove connection points, IProvideClassInfo2 | Property-based callback made entire event infrastructure unnecessary (~150 lines removed) |

---

## Files Changed

### Created
| File | Purpose | Lines |
|------|---------|-------|
| `activex/slop3d_vc6.h` | VC6-compatible header (stdint typedefs, vec3/vec4 helpers) | 57 |
| `activex/slop3d_vc6.cpp` | VC6-compatible port of `src/slop3d.c` | 275 |
| `activex/slop3d_guids.h` | COM GUIDs (CLSID, IID) and DISPID constants | 32 |
| `activex/slop3d_activex.h` | CSlop3DControl class declaration (8 COM interfaces) | 195 |
| `activex/slop3d_activex.cpp` | Full COM implementation — IDispatch, IOleObject, GDI rendering | 570 |
| `activex/slop3d_dll.cpp` | DllMain, class factory, regsvr32 registration | 200 |
| `activex/slop3d_dll.def` | DLL export definitions (4 exports) | 6 |
| `activex/build.bat` | VC6 build script (cl + link) | 35 |
| `activex/register.bat` | regsvr32 registration wrapper | 3 |
| `activex/unregister.bat` | regsvr32 unregistration wrapper | 3 |
| `activex/test.html` | IE test page with `<object>` tag and cycling color demo | 65 |

### Modified
| File | Change |
|------|--------|
| `.gitignore` | Added ActiveX build output patterns (*.dll, *.obj, *.tlb, etc.) |

### Total
12 files, 1720 lines added.

---

## Next Steps

1. **Share C engine code** ([Issue #10](https://github.com/deadcast2/slopwave3d/issues/10)) — Refactor `src/slop3d.c` to use `s3d_vec3()`/`s3d_vec4()` helpers instead of compound literals, allowing both builds to share one source file and eliminating the duplicated `slop3d_vc6.cpp`
2. **Continue engine implementation** — As new features are added to the C engine (meshes, textures, lighting via Issues #3-#8), add corresponding DISPIDs to the ActiveX IDispatch handler to keep both platforms in sync
3. **Test with richer content** — Once triangle rasterization and OBJ loading are implemented, verify the ActiveX control renders 3D content correctly in IE
