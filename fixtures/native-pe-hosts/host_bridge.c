/* Independent Win64 DLL fixture, with an actual import of host_b. */
__declspec(dllimport) int Ping(void);
__declspec(dllexport) int BridgeCall(void) { return Ping() + 7; }
