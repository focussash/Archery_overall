# Troubleshooting

## Native Tests: Silent Compilation Failure

**Symptom**: `pio test -e native_test` fails with `*** [file.o] Error 1` and zero compiler output (no actual error messages). Even a hello-world `g++ -o test test.cpp` fails silently with exit code 1.

**Root cause**: The scoop `mingw` package (MinGW-Builds) has a broken `cc1plus.exe` that silently crashes.

**Fix**:
1. `scoop uninstall mingw`
2. `scoop install mingw-winlibs` (WinLibs build — different distro, same GCC version, working binaries)
3. If the shell session doesn't pick up the new PATH, prepend: `export PATH="/c/Users/Zijie Jin/scoop/apps/mingw-winlibs/current/bin:$PATH"`
4. Verify with: `g++ --version` (should say "built by Brecht Sanders")

## Unity Test Files: Missing setUp/tearDown

**Symptom**: Linker error `undefined reference to 'setUp'` / `undefined reference to 'tearDown'`.

**Fix**: Every Unity test file must have these stubs (even if unused):
```cpp
void setUp(void) {}
void tearDown(void) {}
```
