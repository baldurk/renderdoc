typedef unsigned long DWORD;
typedef void *HANDLE;
typedef void *LPVOID;

class QObject;
class QString;

class QQmlData {
public: __declspec(dllexport) static class QQmlData *__cdecl get(const QObject*,bool) {return 0;}
};

namespace QV4 {
struct Value;
struct ExecutionEngine {
  __declspec(dllexport) unsigned __int64 __thiscall throwError(const QString&) {return 0;}
  __declspec(dllexport) unsigned __int64 __thiscall throwSyntaxError(const QString&) {return 0;}
  __declspec(dllexport) unsigned __int64 __thiscall throwTypeError(const QString&) {return 0;}
};
struct PersistentValueStorage {
  __declspec(dllexport) static ExecutionEngine *__cdecl getEngine(Value*) {return 0;}
};
};

extern "C" long __stdcall _DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {return 1;}