Here's a thorough analysis of the `INDIClient` class and concrete refactoring suggestions:

---

## Refactoring Suggestions for `INDIClient`

### ✅ 1. Separate `DeviceManager` into its own files *(Done)*

`DeviceManager` now lives in `DeviceManager.h` and `DeviceManager.cpp`, cleanly separated from `INDIClient`.

---

### ⚠️ 2. Encapsulate public data members *(Partial)*

`DeviceManager` now correctly hides `cameras` and `focusers` behind `private` with accessors (`getCameras()`, `getFocusers()`, `addCamera()`, `addFocuser()`).

However, `INDIClient` still exposes several members as `public`:

```cpp
// Still public in INDIClient:
std::string host = "localhost";
unsigned int port = 7624;
DeviceManager deviceManager;
std::optional<std::promise<bool>> connectPromise;
std::optional<std::promise<cv::Mat>> imagePromise;
std::optional<std::promise<bool>> focusPromise;
```

The promises and `deviceManager` should be `private`; `host`/`port` could be constructor parameters.

---

### ✅ 3. Replace raw `promise` pointers with `std::optional` *(Done)*

All three promises now correctly use `std::optional<std::promise<T>>`:

```cpp
std::optional<std::promise<bool>> connectPromise;
std::optional<std::promise<cv::Mat>> imagePromise;
std::optional<std::promise<bool>> focusPromise;
```

---

### ✅ 4. Split `newProperty` and `updateProperty` into handlers *(Done)*

The dispatch is now delegated to dedicated private handlers:

- `onConnection()`
- `onCameraInfo()`
- `onCameraImage()`
- `onFocuserMotion()`

---

### 🔁 5. Replace magic string literals with constants *(Not done)*

Device name matching still uses hardcoded strings scattered across `INDIClient.cpp` and `DeviceManager.h`:

```cpp
// Fragile:
if (deviceName.contains("CCD")) { ... }
if (deviceName.contains("Focuser")) { ... }

// Better: centralize in config or static members
static constexpr std::string_view kCameraDeviceTag   = "CCD";
static constexpr std::string_view kFocuserDeviceTag  = "Focuser";
static constexpr std::string_view kFocuserMotionProp = "REL_FOCUS_POSITION";
```

---

### 🔄 6. `move()` re-entrancy bug *(Not done)*

`move()` unconditionally calls `focusPromise.emplace()`, which silently overwrites an active promise if called while already moving. The previous future becomes permanently unresolvable. A guard is needed:

```cpp
std::future<bool> INDIClient::move(bool isOutwards, int steps) {
  if (focusPromise.has_value()) {
    throw std::logic_error("move() called while already moving");
  }
  // ...
}
```

---

### 📢 7. Replace scattered `std::println` with a logging abstraction *(Not done)*

Both `INDIClient.cpp` and `DeviceManager.h` log directly via `std::println`. A simple callback-based logger would allow suppression or redirection in tests and production builds.

---

### ✅ 8. Move connection logic out of the constructor *(Done)*

`INDIClient()` is now an empty default constructor. The explicit `connect(host, port)` method performs the server connection and returns a `std::future<bool>`.

---

### ⚠️ 9. Clean up dead code *(Partial)*

- ✅ Commented-out `cameras`/`focusers` members in `INDIClient.h` are removed.
- ✅ `newMessage` body is now active (logs device name and message).
- ❌ Dead code still present in `main.cpp` after the first `return 0` (lines 55–73): unreachable file-reading loop.

---

### Summary table

| # | Issue | Status | Severity |
|---|-------|--------|----------|
| 1 | `DeviceManager` mixed into `INDIClient.h` | ✅ Done | Medium |
| 2 | Public data members | ⚠️ Partial (`DeviceManager` fixed; `INDIClient` still exposes promises & fields) | Medium |
| 3 | `unique_ptr<promise>` instead of `optional<promise>` | ✅ Done | Medium |
| 4 | Monolithic `newProperty`/`updateProperty` | ✅ Done | Medium |
| 5 | Magic strings | ❌ Not done | Low |
| 6 | `move()` re-entrancy bug | ❌ Not done | High |
| 7 | Inline logging | ❌ Not done | Low |
| 8 | Connection in constructor | ✅ Done | Medium |
| 9 | Dead/commented code | ⚠️ Partial (dead code remains in `main.cpp`) | Low |