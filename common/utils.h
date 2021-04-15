#include <comdef.h>

inline std::wstring AnsiToWString(const std::string& str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                                 \
{                                                                                        \
    HRESULT hr__ = (x);                                                                  \
    std::wstring wfn = AnsiToWString(__FILE__);                                          \
    if (FAILED(hr__)) { throw ApplicationFrameworkException(hr__, L#x, wfn, __LINE__); } \
}
#endif

struct ApplicationFrameworkException
{
    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring filename;
    int LineNumber = -1;

    ApplicationFrameworkException() = default;
    ApplicationFrameworkException(HRESULT hr, const std::wstring& funcname, const std::wstring& filename, int line) :
        ErrorCode(hr),
        FunctionName(funcname),
        filename(filename),
        LineNumber(line)
    {}

    std::wstring ToString() const
    {
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
    }
};