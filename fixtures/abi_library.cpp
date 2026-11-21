// Windows x64 fixture DLL: intentionally tiny original export surface.
// No proprietary code, Game Core interfaces or SDK files.
extern "C" int ScarlettAdd(int left, int right) noexcept { return left + right; }
extern "C" int ScarlettMultiply(int left, int right) noexcept { return left * right; }
