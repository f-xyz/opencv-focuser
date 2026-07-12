In this codebase, replacing the classes in [src/](file:///home/andr/projects/opencv-focuser/src) with header-only files (using `.hxx`, `.hpp`, or `.h` extensions) is **very reasonable and common for a utility-focused project of this size**. 

Here is a breakdown of the pros, cons, and recommendations for this change:

---

### **Pros of Header-Only Implementation**
1. **Simplified Integration:** 
   Header-only libraries do not require separate compilation units. In [CMakeLists.txt](file:///home/andr/projects/opencv-focuser/CMakeLists.txt), you would only track [main.cpp](file:///home/andr/projects/opencv-focuser/src/main.cpp). This avoids issues common with `file(GLOB_RECURSE ...)` (which is generally discouraged in modern CMake because it does not automatically detect newly added files without re-running CMake).
2. **Stateless Classes & Utility Functions:** 
   * [SharpnessEstimator](file:///home/andr/projects/opencv-focuser/src/SharpnessEstimator/SharpnessEstimator.h) holds zero state. It could easily be a namespace containing `inline` functions.
   * [utils.h](file:///home/andr/projects/opencv-focuser/src/utils/utils.h) contains free functions that are natural candidates for header-only `inline` declarations.
3. **Inlining and Compiler Optimization:** 
   Having the function implementations visible to the compiler at every call site allows for aggressive inlining and optimization without needing to enable Link-Time Optimization (LTO/LTCG).

---

### **Cons / Potential Issues to Keep in Mind**
1. **Dependency Leakage (Pollution):** 
   If [FitsReader.h](file:///home/andr/projects/opencv-focuser/src/FitsReader/FitsReader.h) becomes header-only, any file that `#include`s it will also transitively include `<fitsio.h>` and OpenCV's headers. 
   * C-libraries like CFITSIO define global constants and macros (e.g., `READONLY`, `IMAGE_HDU`) that can leak into and pollute the namespaces of files including your header.
2. **Compilation Times:** 
   If the implementation code of [FitsReader.cpp](file:///home/andr/projects/opencv-focuser/src/FitsReader/FitsReader.cpp) is placed entirely in a header, changes to any of the FITS parsing logic will trigger a recompilation of every source file that includes it. For a small project, this is trivial, but it scales poorly as the project grows.

---

### **Recommendation**
* **Yes for [SharpnessEstimator](file:///home/andr/projects/opencv-focuser/src/SharpnessEstimator/SharpnessEstimator.h) and [utils](file:///home/andr/projects/opencv-focuser/src/utils/utils.h):** Since these are small, helper-oriented, and rely on standard library/OpenCV types already used everywhere, making them header-only is a clean improvement. You can group them into namespace scopes with `inline` functions.
* **Caution with [FitsReader](file:///home/andr/projects/opencv-focuser/src/FitsReader/FitsReader.h):** Because of its direct dependency on the C-style `fitsio.h` header, keeping the implementation details inside a `.cpp` file (or using a `struct FitsReader::Impl` Pimpl idiom) is usually better practice to prevent FITS macros from polluting your wider application. However, if this is a small utility tool where [main.cpp](file:///home/andr/projects/opencv-focuser/src/main.cpp) is the only consumer, a header-only version using `inline` functions is perfectly acceptable.
