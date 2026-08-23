# Coding Style

- C++: C++20, RAII, four-space indentation, LLVM-derived `.clang-format`.
- Public headers live under `include/remotemic/`; implementations live under `src/`.
- Ownership is explicit. Do not expose owning raw pointers.
- Use bounded containers in data paths and avoid blocking operations in future audio callbacks.
- Treat warnings as defects; `/W4` on MSVC and `-Wall -Wextra -Wpedantic` elsewhere.
- Python is reserved for offline tools and follows Black/Ruff when introduced.
- Comments explain constraints and reasons, not obvious syntax.

