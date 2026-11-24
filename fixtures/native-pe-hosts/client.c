/* Independent Win64 PE executable fixture. Cross-linked, never executed. */
__declspec(dllimport) int NativeCall(void);
__declspec(dllimport) int ForwardNamed(void);
__declspec(dllimport) int ForwardOrdinal(void);
#ifndef FORWARD_ONLY
__declspec(dllimport) int OrdinalOnly(void);
__declspec(dllimport) int BridgeCall(void);
#endif
int entry(void) {
#ifdef FORWARD_ONLY
    return ForwardNamed() + ForwardOrdinal();
#else
    return NativeCall() + ForwardNamed() + ForwardOrdinal() + OrdinalOnly() + BridgeCall();
#endif
}
