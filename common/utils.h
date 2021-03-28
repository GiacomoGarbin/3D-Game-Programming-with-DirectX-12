#ifndef ThrowIfFailed
#define ThrowIfFailed(cmd) \
{ \
    HRESULT hr = (cmd); \
}
#endif