#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <climits>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include "background_ui_logic.h"
#include "fixed_slot_sell_logic.h"
#include "internal_ui_click_logic.h"
#include "protocol.h"
#include "unity_geometry_logic.h"

using namespace cleanroute;
using background_ui_logic::Labels;
using background_ui_logic::Role;
using internal_ui_click_logic::DispatchPlan;
using unity_geometry_logic::GeometryClass;
using unity_geometry_logic::ImageSlot;

namespace {

using Il2CppDomain = void;
using Il2CppAssembly = void;
using Il2CppImage = void;
using Il2CppClass = void;
using MethodInfo = void;
using FieldInfo = void;
using Il2CppType = void;
using Il2CppObject = void;
using Il2CppString = void;

HANDLE g_mapping = nullptr;
SharedBlock* g_shared = nullptr;
std::vector<Il2CppObject*> g_travelBaselineActiveControls{};

template <typename T>
bool Resolve(HMODULE module, const char* name, T& out) {
    out = nullptr;
    FARPROC p = GetProcAddress(module, name);
    if (!p) return false;
    static_assert(sizeof(p) == sizeof(out), "pointer-size mismatch");
    const unsigned char* s = reinterpret_cast<const unsigned char*>(&p);
    unsigned char* d = reinterpret_cast<unsigned char*>(&out);
    for (std::size_t i = 0; i < sizeof(out); ++i) d[i] = s[i];
    return out != nullptr;
}

bool Eq(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) { if (*a++ != *b++) return false; }
    return *a == *b;
}

void SetText(wchar_t* out, std::size_t cap, const wchar_t* text) {
    if (!out || cap == 0) return;
    std::size_t i = 0;
    if (text) while (i + 1 < cap && text[i]) { out[i] = text[i]; ++i; }
    out[i] = 0;
}

void Append(wchar_t* out, std::size_t cap, const wchar_t* text) {
    if (!out || !text || cap == 0) return;
    std::size_t n = 0; while (n + 1 < cap && out[n]) ++n;
    std::size_t i = 0; while (n + 1 < cap && text[i]) out[n++] = text[i++];
    out[n] = 0;
}

void AppendInt(wchar_t* out, std::size_t cap, int value) {
    wchar_t tmp[32]{}; wsprintfW(tmp, L"%d", value); Append(out, cap, tmp);
}

void AppendInt64(wchar_t* out, std::size_t cap, std::int64_t value) {
    wchar_t tmp[48]{}; _snwprintf_s(tmp, _countof(tmp), _TRUNCATE, L"%lld", static_cast<long long>(value));
    Append(out, cap, tmp);
}

struct Api {
    HMODULE module = nullptr;
    Il2CppDomain* (__cdecl* domain_get)() = nullptr;
    const Il2CppAssembly* (__cdecl* domain_assembly_open)(Il2CppDomain*, const char*) = nullptr;
    const Il2CppImage* (__cdecl* assembly_get_image)(const Il2CppAssembly*) = nullptr;
    Il2CppClass* (__cdecl* class_from_name)(const Il2CppImage*, const char*, const char*) = nullptr;
    const MethodInfo* (__cdecl* class_get_method_from_name)(Il2CppClass*, const char*, int) = nullptr;
    Il2CppClass* (__cdecl* class_get_parent)(Il2CppClass*) = nullptr;
    std::uint32_t (__cdecl* method_get_flags)(const MethodInfo*, std::uint32_t*) = nullptr;
    std::uint32_t (__cdecl* method_get_param_count)(const MethodInfo*) = nullptr;
    const Il2CppType* (__cdecl* method_get_param)(const MethodInfo*, std::uint32_t) = nullptr;
    const Il2CppType* (__cdecl* method_get_return_type)(const MethodInfo*) = nullptr;
    char* (__cdecl* type_get_name)(const Il2CppType*) = nullptr;
    void (__cdecl* free_fn)(void*) = nullptr;
    Il2CppObject* (__cdecl* runtime_invoke)(const MethodInfo*, void*, void**, void**) = nullptr;
    void* (__cdecl* object_unbox)(Il2CppObject*) = nullptr;
    Il2CppClass* (__cdecl* object_get_class)(Il2CppObject*) = nullptr;
    FieldInfo* (__cdecl* class_get_field_from_name)(Il2CppClass*, const char*) = nullptr;
    const Il2CppType* (__cdecl* field_get_type)(FieldInfo*) = nullptr;
    void (__cdecl* field_get_value)(Il2CppObject*, FieldInfo*, void*) = nullptr;
    Il2CppClass* (__cdecl* class_from_type)(const Il2CppType*) = nullptr;
    bool (__cdecl* class_is_valuetype)(const Il2CppClass*) = nullptr;
    std::int32_t (__cdecl* string_length)(Il2CppString*) = nullptr;
    const wchar_t* (__cdecl* string_chars)(Il2CppString*) = nullptr;
    bool (__cdecl* class_is_assignable_from)(Il2CppClass*, Il2CppClass*) = nullptr;
    void (__cdecl* field_static_get_value)(FieldInfo*, void*) = nullptr;
    const Il2CppImage* (__cdecl* get_corlib)() = nullptr;
    Il2CppObject* (__cdecl* array_new)(Il2CppClass*, std::uintptr_t) = nullptr;
    Il2CppString* (__cdecl* string_new)(const char*) = nullptr;
    std::size_t (__cdecl* image_get_class_count)(const Il2CppImage*) = nullptr;
    Il2CppClass* (__cdecl* image_get_class)(const Il2CppImage*, std::size_t) = nullptr;
    const char* (__cdecl* class_get_name)(Il2CppClass*) = nullptr;
    const MethodInfo* (__cdecl* class_get_methods)(Il2CppClass*, void**) = nullptr;
    const char* (__cdecl* method_get_name)(const MethodInfo*) = nullptr;
    bool uiDiscoveryExportsLoaded = false;
    bool uiLuaExportsLoaded = false;

    bool Load(wchar_t* detail, std::size_t cap) {
        if (module) return true;
        module = GetModuleHandleW(L"GameAssembly.dll");
        if (!module) { SetText(detail, cap, L"GameAssembly.dll chưa sẵn sàng"); return false; }
#define NEED(symbol) do { if (!Resolve(module, "il2cpp_" #symbol, symbol)) { SetText(detail, cap, L"Thiếu IL2CPP export bắt buộc"); return false; } } while (0)
        NEED(domain_get); NEED(domain_assembly_open); NEED(assembly_get_image); NEED(class_from_name);
        NEED(class_get_method_from_name); NEED(class_get_parent); NEED(method_get_flags);
        NEED(method_get_param_count); NEED(method_get_param); NEED(method_get_return_type);
        NEED(type_get_name); NEED(runtime_invoke); NEED(object_unbox); NEED(object_get_class);
        NEED(class_get_field_from_name); NEED(field_get_type); NEED(field_get_value);
        NEED(class_from_type); NEED(class_is_valuetype); NEED(string_length); NEED(string_chars);
#undef NEED
        if (!Resolve(module, "il2cpp_free", free_fn)) { SetText(detail, cap, L"Thiếu il2cpp_free"); return false; }
        return true;
    }

    bool LoadUiDiscovery(wchar_t* detail, std::size_t cap) {
        if (!Load(detail, cap)) return false;
        if (uiDiscoveryExportsLoaded) return true;
        if (!Resolve(module, "il2cpp_class_is_assignable_from", class_is_assignable_from)) {
            SetText(detail, cap, L"UI discovery thiếu export class_is_assignable_from");
            return false;
        }
        if (!Resolve(module, "il2cpp_field_static_get_value", field_static_get_value)) {
            SetText(detail, cap, L"UI discovery thiếu export field_static_get_value");
            return false;
        }
        // These exports are optional: known namespaces remain the fast path, while
        // metadata enumeration lets the same verified class surface survive a namespace move.
        (void)Resolve(module, "il2cpp_image_get_class_count", image_get_class_count);
        (void)Resolve(module, "il2cpp_image_get_class", image_get_class);
        (void)Resolve(module, "il2cpp_class_get_name", class_get_name);
        (void)Resolve(module, "il2cpp_class_get_methods", class_get_methods);
        (void)Resolve(module, "il2cpp_method_get_name", method_get_name);
        uiDiscoveryExportsLoaded = true;
        return true;
    }

    bool LoadUiLua(wchar_t* detail, std::size_t cap) {
        if (!LoadUiDiscovery(detail, cap)) return false;
        if (uiLuaExportsLoaded) return true;
        if (!Resolve(module, "il2cpp_get_corlib", get_corlib)) {
            SetText(detail, cap, L"Lua callback thiếu export get_corlib");
            return false;
        }
        if (!Resolve(module, "il2cpp_array_new", array_new)) {
            SetText(detail, cap, L"Lua callback thiếu export array_new");
            return false;
        }
        if (!Resolve(module, "il2cpp_string_new", string_new)) {
            SetText(detail, cap, L"Lua callback thiếu export string_new");
            return false;
        }
        uiLuaExportsLoaded = true;
        return true;
    }
};

Api g_api;

const Il2CppImage* Image() {
    Il2CppDomain* domain = g_api.domain_get ? g_api.domain_get() : nullptr;
    if (!domain) return nullptr;
    const Il2CppAssembly* assembly = g_api.domain_assembly_open(domain, "Assembly-CSharp");
    if (!assembly) assembly = g_api.domain_assembly_open(domain, "Assembly-CSharp.dll");
    return assembly ? g_api.assembly_get_image(assembly) : nullptr;
}

const Il2CppImage* ImageForAssembly(const char* name, const char* dllName) {
    Il2CppDomain* domain = g_api.domain_get ? g_api.domain_get() : nullptr;
    if (!domain) return nullptr;
    const Il2CppAssembly* assembly = g_api.domain_assembly_open(domain, name);
    if (!assembly && dllName) assembly = g_api.domain_assembly_open(domain, dllName);
    return assembly ? g_api.assembly_get_image(assembly) : nullptr;
}

bool StaticMethod(const MethodInfo* method) {
    if (!method) return false;
    constexpr std::uint32_t StaticFlag = 0x0010;
    std::uint32_t iflags = 0;
    return (g_api.method_get_flags(method, &iflags) & StaticFlag) != 0;
}

const MethodInfo* FindMethod(Il2CppClass* klass, const char* name, int argc) {
    for (Il2CppClass* c = klass; c; c = g_api.class_get_parent(c)) {
        if (const MethodInfo* m = g_api.class_get_method_from_name(c, name, argc)) return m;
    }
    return nullptr;
}

bool ParamType(const MethodInfo* m, std::uint32_t index, const char* expected) {
    if (!m || index >= g_api.method_get_param_count(m)) return false;
    const Il2CppType* t = g_api.method_get_param(m, index);
    char* n = t ? g_api.type_get_name(t) : nullptr;
    if (!n) return false;
    bool ok = Eq(n, expected);
    g_api.free_fn(n);
    return ok;
}

bool ReturnType(const MethodInfo* method, const char* expected) {
    if (!method || !expected) return false;
    const Il2CppType* type = g_api.method_get_return_type(method);
    char* name = type ? g_api.type_get_name(type) : nullptr;
    if (!name) return false;
    const bool matches = Eq(name, expected);
    g_api.free_fn(name);
    return matches;
}

bool FieldType(FieldInfo* field, const char* expected) {
    if (!field || !expected) return false;
    const Il2CppType* type = g_api.field_get_type(field);
    char* name = type ? g_api.type_get_name(type) : nullptr;
    if (!name) return false;
    const bool matches = Eq(name, expected);
    g_api.free_fn(name);
    return matches;
}

const MethodInfo* ExactMethod(Il2CppClass* klass, const char* name, int argc, bool isStatic,
                              const char* p0 = nullptr, const char* p1 = nullptr, const char* p2 = nullptr) {
    const MethodInfo* m = FindMethod(klass, name, argc);
    if (!m || StaticMethod(m) != isStatic) return nullptr;
    if (argc > 0 && p0 && !ParamType(m, 0, p0)) return nullptr;
    if (argc > 1 && p1 && !ParamType(m, 1, p1)) return nullptr;
    if (argc > 2 && p2 && !ParamType(m, 2, p2)) return nullptr;
    return m;
}

bool InvokeObjectArgs(const MethodInfo* method, void* instance, void** args,
                      Il2CppObject*& out, wchar_t* detail, std::size_t cap) {
    out = nullptr;
    if (!method) { SetText(detail, cap, L"Method object chưa resolve"); return false; }
    void* exc = nullptr;
    out = g_api.runtime_invoke(method, instance, args, &exc);
    if (exc) { SetText(detail, cap, L"Managed exception ở object getter"); return false; }
    return true;
}

bool InvokeObject(const MethodInfo* method, void* instance, Il2CppObject*& out, wchar_t* detail, std::size_t cap) {
    return InvokeObjectArgs(method, instance, nullptr, out, detail, cap);
}

// IL2CPP runtime_invoke expects the unboxed payload when an instance is a
// boxed value type. The v0.1.8 probe used this normalization for every
// property/member read and for HandleClickEvent callbacks.
void* ManagedThis(Il2CppObject* object) {
    if (!object) return nullptr;
    Il2CppClass* klass = g_api.object_get_class(object);
    if (klass && g_api.class_is_valuetype(klass)) {
        void* unboxed = g_api.object_unbox(object);
        if (unboxed) return unboxed;
    }
    return object;
}

bool InvokeScalarArgs(const MethodInfo* method, void* instance, void** args,
                      std::int64_t& out, wchar_t* detail, std::size_t cap) {
    out = 0;
    if (!method) { SetText(detail, cap, L"Scalar method chưa resolve"); return false; }
    const Il2CppType* rt = g_api.method_get_return_type(method);
    char* tn = rt ? g_api.type_get_name(rt) : nullptr;
    if (!tn) { SetText(detail, cap, L"Không đọc được return type"); return false; }
    void* exc = nullptr;
    Il2CppObject* boxed = g_api.runtime_invoke(method, instance, args, &exc);
    if (exc || !boxed) { g_api.free_fn(tn); SetText(detail, cap, L"Scalar getter lỗi/null"); return false; }
    void* raw = g_api.object_unbox(boxed);
    if (!raw) { g_api.free_fn(tn); SetText(detail, cap, L"Không unbox scalar"); return false; }
    bool ok = true;
    if (Eq(tn, "System.Boolean")) out = *reinterpret_cast<const std::uint8_t*>(raw) ? 1 : 0;
    else if (Eq(tn, "System.Int32")) out = *reinterpret_cast<const std::int32_t*>(raw);
    else if (Eq(tn, "System.UInt32")) out = *reinterpret_cast<const std::uint32_t*>(raw);
    else if (Eq(tn, "System.Int64")) out = *reinterpret_cast<const std::int64_t*>(raw);
    else if (Eq(tn, "System.UInt64")) {
        const std::uint64_t value = *reinterpret_cast<const std::uint64_t*>(raw);
        if (value > static_cast<std::uint64_t>(INT64_MAX)) ok = false;
        else out = static_cast<std::int64_t>(value);
    }
    else ok = false;
    g_api.free_fn(tn);
    if (!ok) SetText(detail, cap, L"Return type scalar chưa hỗ trợ");
    return ok;
}

bool InvokeScalar(const MethodInfo* method, void* instance, std::int64_t& out,
                  wchar_t* detail, std::size_t cap) {
    return InvokeScalarArgs(method, instance, nullptr, out, detail, cap);
}

bool ScalarGetter(Il2CppClass* klass, const char* name, void* instance, std::int32_t& out,
                  wchar_t* detail, std::size_t cap) {
    std::int64_t value = 0;
    if (!InvokeScalar(FindMethod(klass, name, 0), instance, value, detail, cap)) return false;
    if (value < INT32_MIN || value > INT32_MAX) { SetText(detail, cap, L"Scalar vượt Int32"); return false; }
    out = static_cast<std::int32_t>(value);
    return true;
}

bool ScalarGetter64(Il2CppClass* klass, const char* name, void* instance, std::int64_t& out,
                    wchar_t* detail, std::size_t cap) {
    return InvokeScalar(FindMethod(klass, name, 0), instance, out, detail, cap);
}

bool StaticScalar(Il2CppClass* klass, const char* name, std::int32_t& out,
                  wchar_t* detail, std::size_t cap) {
    const MethodInfo* m = FindMethod(klass, name, 0);
    if (!m || !StaticMethod(m)) { SetText(detail, cap, L"Static getter chưa resolve"); return false; }
    return ScalarGetter(klass, name, nullptr, out, detail, cap);
}

bool InvokeVoid(const MethodInfo* method, void* instance, void** args,
                wchar_t* detail, std::size_t cap) {
    if (!method) { SetText(detail, cap, L"Action method chưa resolve"); return false; }
    void* exc = nullptr;
    (void)g_api.runtime_invoke(method, instance, args, &exc);
    if (exc) { SetText(detail, cap, L"Action ném managed exception"); return false; }
    return true;
}

bool CopyString(Il2CppString* value, wchar_t* out, std::size_t cap) {
    if (!value || !out || cap == 0) return false;
    const int len = g_api.string_length(value);
    const wchar_t* chars = g_api.string_chars(value);
    if (len < 0 || len > 4096 || !chars) return false;
    std::size_t n = static_cast<std::size_t>(len);
    if (n + 1 > cap) n = cap - 1;
    for (std::size_t i = 0; i < n; ++i) out[i] = chars[i];
    out[n] = 0;
    return true;
}

struct Classes {
    Il2CppClass* gameApi = nullptr;
    Il2CppClass* networkApi = nullptr;
    Il2CppClass* guiApi = nullptr; // optional observer surface; route core must remain usable if unavailable
    Il2CppClass* session = nullptr;
    Il2CppClass* shared = nullptr;
    Il2CppClass* autoPath = nullptr;
};

bool ResolveClasses(Classes& c, wchar_t* detail, std::size_t cap) {
    if (!g_api.Load(detail, cap)) return false;
    const Il2CppImage* image = Image();
    if (!image) { SetText(detail, cap, L"Không mở được Assembly-CSharp"); return false; }
    c.gameApi = g_api.class_from_name(image, "FGStudio.LuaSystem.API", "LuaSystemAPI_Game");
    c.networkApi = g_api.class_from_name(image, "FGStudio.LuaSystem.API", "LuaSystemAPI_Network");
    c.guiApi = g_api.class_from_name(image, "FGStudio.LuaSystem.API", "LuaSystemAPI_GUI");
    c.session = g_api.class_from_name(image, "FGStudio.Game.Logic", "SessionData");
    c.shared = g_api.class_from_name(image, "FGStudio.LuaSystem", "LuaSystemSharedData");
    c.autoPath = g_api.class_from_name(image, "FGStudio.Engine.Logic", "AutoPathManager");
    if (!c.gameApi || !c.session || !c.shared || !c.autoPath) {
        SetText(detail, cap, L"Thiếu class route bắt buộc trên client này");
        return false;
    }
    return true;
}

bool Transition(const Classes& c, int& mapReady, int& waiting, wchar_t* detail, std::size_t cap) {
    if (!StaticScalar(c.gameApi, "IsMapReady", mapReady, detail, cap)) return false;
    if (!StaticScalar(c.session, "get_WaitingChangeMap", waiting, detail, cap)) return false;
    return true;
}

bool GetLeader(const Classes& c, Il2CppObject*& leader, Il2CppClass*& leaderClass,
               wchar_t* detail, std::size_t cap) {
    const MethodInfo* getLeader = ExactMethod(c.shared, "get_LeaderRoleData", 0, true);
    if (!getLeader || !InvokeObject(getLeader, nullptr, leader, detail, cap) || !leader) {
        SetText(detail, cap, L"LeaderRoleData chưa sẵn sàng"); return false;
    }
    leaderClass = g_api.object_get_class(leader);
    if (!leaderClass) { SetText(detail, cap, L"Không lấy được class LeaderRoleData"); return false; }
    return true;
}

bool ReadPosition(Il2CppObject* leader, Il2CppClass* leaderClass, int& x, int& y,
                  wchar_t* detail, std::size_t cap) {
    if (ScalarGetter(leaderClass, "get_PosX", leader, x, detail, cap) &&
        ScalarGetter(leaderClass, "get_PosY", leader, y, detail, cap)) return true;
    FieldInfo* field = nullptr;
    for (Il2CppClass* c = leaderClass; c; c = g_api.class_get_parent(c)) {
        field = g_api.class_get_field_from_name(c, "roleData");
        if (field) break;
    }
    if (!field) { SetText(detail, cap, L"Không resolve được PosX/PosY"); return false; }
    const Il2CppType* ft = g_api.field_get_type(field);
    Il2CppClass* fc = ft ? g_api.class_from_type(ft) : nullptr;
    if (!fc || g_api.class_is_valuetype(fc)) { SetText(detail, cap, L"roleData backing không hợp lệ"); return false; }
    Il2CppObject* backing = nullptr;
    g_api.field_get_value(leader, field, &backing);
    if (!backing) { SetText(detail, cap, L"roleData backing=null"); return false; }
    Il2CppClass* bc = g_api.object_get_class(backing);
    return bc && ScalarGetter(bc, "get_PosX", backing, x, detail, cap) &&
                 ScalarGetter(bc, "get_PosY", backing, y, detail, cap);
}

bool RoleDataBacking(Il2CppObject* leader, Il2CppClass* leaderClass, Il2CppObject*& backing, Il2CppClass*& backingClass) {
    backing = nullptr;
    backingClass = nullptr;
    FieldInfo* field = nullptr;
    for (Il2CppClass* c = leaderClass; c; c = g_api.class_get_parent(c)) {
        field = g_api.class_get_field_from_name(c, "roleData");
        if (field) break;
    }
    if (!field) return false;
    const Il2CppType* ft = g_api.field_get_type(field);
    Il2CppClass* fc = ft ? g_api.class_from_type(ft) : nullptr;
    if (!fc || g_api.class_is_valuetype(fc)) return false;
    g_api.field_get_value(leader, field, &backing);
    if (!backing) return false;
    backingClass = g_api.object_get_class(backing);
    return backingClass != nullptr;
}

bool TryRoleCurrency(Il2CppObject* leader, Il2CppClass* leaderClass, const char* getter, std::int64_t& out) {
    wchar_t ignored[160]{};
    if (ScalarGetter64(leaderClass, getter, leader, out, ignored, _countof(ignored))) return true;
    Il2CppObject* backing = nullptr; Il2CppClass* backingClass = nullptr;
    if (!RoleDataBacking(leader, leaderClass, backing, backingClass)) return false;
    ignored[0] = 0;
    return ScalarGetter64(backingClass, getter, backing, out, ignored, _countof(ignored));
}

bool ReadCurrency(std::int64_t& money, std::int64_t& boundMoney, std::int32_t& validMask,
                  wchar_t* detail, std::size_t cap) {
    money = 0; boundMoney = 0; validMask = 0;
    Classes c{};
    if (!ResolveClasses(c, detail, cap)) return false;
    int ready = 0, waiting = 0;
    if (!Transition(c, ready, waiting, detail, cap)) return false;
    if (!ready || waiting) { SetText(detail, cap, L"Currency tạm hoãn: đang chuyển map"); return false; }
    Il2CppObject* leader = nullptr; Il2CppClass* lc = nullptr;
    if (!GetLeader(c, leader, lc, detail, cap)) return false;
    if (TryRoleCurrency(leader, lc, "get_Money", money)) validMask |= 1;
    if (TryRoleCurrency(leader, lc, "get_BoundMoney", boundMoney)) validMask |= 2;
    if (!validMask) { SetText(detail, cap, L"Không resolve được Money/BoundMoney trên Leader/roleData"); return false; }
    SetText(detail, cap, L"CURRENCY money="); AppendInt64(detail, cap, money);
    Append(detail, cap, L" bound="); AppendInt64(detail, cap, boundMoney);
    return true;
}

bool AutoPathInstance(const Classes& c, Il2CppObject*& instance, Il2CppClass*& actual,
                      wchar_t* detail, std::size_t cap) {
    const MethodInfo* getInstance = ExactMethod(c.autoPath, "get_Instance", 0, true);
    if (!getInstance || !InvokeObject(getInstance, nullptr, instance, detail, cap) || !instance) {
        SetText(detail, cap, L"AutoPathManager.Instance chưa sẵn sàng"); return false;
    }
    actual = g_api.object_get_class(instance);
    if (!actual) { SetText(detail, cap, L"Không lấy được class AutoPathManager"); return false; }
    return true;
}

bool ReadState(Snapshot& s, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap)) return false;
    s = {};
    int ready = 0, waiting = 0;
    if (!Transition(c, ready, waiting, detail, cap)) return false;
    s.mapReady = ready ? 1 : 0;
    s.waitingChangeMap = waiting ? 1 : 0;
    s.validMask |= ValidMapTransition;
    if (!ready || waiting) {
        SetText(detail, cap, L"Đang chuyển map; không đọc object sâu và không gửi action");
        return true;
    }

    Il2CppObject* leader = nullptr; Il2CppClass* lc = nullptr;
    if (!GetLeader(c, leader, lc, detail, cap)) return false;
    int role = 0, map = 0, x = 0, y = 0, riding = 0;
    if (!ScalarGetter(lc, "get_RoleID", leader, role, detail, cap) || role <= 0) return false;
    if (!ScalarGetter(lc, "get_MapID", leader, map, detail, cap) || map <= 0) return false;
    if (!ReadPosition(leader, lc, x, y, detail, cap)) return false;
    if (!ScalarGetter(lc, "get_IsRiding", leader, riding, detail, cap)) return false;
    s.roleID = role; s.mapID = map; s.x = x; s.y = y; s.riding = riding ? 1 : 0;
    s.validMask |= ValidIdentity | ValidMap | ValidPosition | ValidRiding;

    int dead = 0;
    wchar_t optionalDetail[160]{};
    if (ScalarGetter(lc, "get_IsDeath", leader, dead, optionalDetail, _countof(optionalDetail))) {
        s.dead = dead ? 1 : 0; s.validMask |= ValidLifeState;
    }


    int enableAutoF1 = 0;
    optionalDetail[0] = 0;
    if (StaticScalar(c.gameApi, "get_EnableAutoF1", enableAutoF1, optionalDetail, _countof(optionalDetail))) {
        // Existing client semantic verified by the read-only NewCore donor: EnableAutoF1=false means auto-fight is ON.
        s.autoFight = enableAutoF1 ? 0 : 1; s.validMask |= ValidAutoFight;
    }

    int freeBagSpace = -1;
    optionalDetail[0] = 0;
    if (StaticScalar(c.gameApi, "GetFreeBagSpace", freeBagSpace, optionalDetail, _countof(optionalDetail)) && freeBagSpace >= 0) {
        s.freeBagSpace = freeBagSpace; s.validMask |= ValidBagSpace;
    }


    Il2CppObject* ap = nullptr; Il2CppClass* ac = nullptr;
    if (!AutoPathInstance(c, ap, ac, detail, cap)) return false;
    int pathing = 0;
    if (!ScalarGetter(ac, "get_IsAutoPathing", ap, pathing, detail, cap)) return false;
    s.autoPathing = pathing ? 1 : 0; s.validMask |= ValidAutoPath;

    const MethodInfo* getName = FindMethod(lc, "get_Name", 0);
    if (getName) {
        Il2CppObject* no = nullptr;
        wchar_t ignored[128]{};
        if (InvokeObject(getName, leader, no, ignored, _countof(ignored)) && no)
            (void)CopyString(reinterpret_cast<Il2CppString*>(no), s.characterName, _countof(s.characterName));
    }

    SetText(detail, cap, L"STATE map="); AppendInt(detail, cap, s.mapID);
    Append(detail, cap, L" pos="); AppendInt(detail, cap, s.x); Append(detail, cap, L","); AppendInt(detail, cap, s.y);
    Append(detail, cap, L" riding="); AppendInt(detail, cap, s.riding);
    Append(detail, cap, L" autoPath="); AppendInt(detail, cap, s.autoPathing);
    if (s.validMask & ValidLifeState) { Append(detail, cap, L" dead="); AppendInt(detail, cap, s.dead); }
    if (s.validMask & ValidAutoFight) { Append(detail, cap, L" autoFight="); AppendInt(detail, cap, s.autoFight); }
    if (s.validMask & ValidBagSpace) { Append(detail, cap, L" freeBag="); AppendInt(detail, cap, s.freeBagSpace); }
    return true;
}

bool SafeForAction(const Classes& c, wchar_t* detail, std::size_t cap) {
    int ready = 0, waiting = 0;
    if (!Transition(c, ready, waiting, detail, cap)) return false;
    if (!ready || waiting) { SetText(detail, cap, L"Action bị chặn: đang chuyển map"); return false; }
    return true;
}

bool ToggleRide(bool desiredRiding, wchar_t* detail, std::size_t cap) {
    Classes c{}; if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    Il2CppObject* leader = nullptr; Il2CppClass* lc = nullptr;
    if (!GetLeader(c, leader, lc, detail, cap)) return false;
    int riding = 0;
    if (!ScalarGetter(lc, "get_IsRiding", leader, riding, detail, cap)) return false;
    if ((riding != 0) == desiredRiding) { SetText(detail, cap, L"Ride state đã đúng; không toggle lại"); return true; }

    const MethodInfo* getSlot = ExactMethod(c.gameApi, "get_CurrentMountSlot", 0, true);
    const MethodInfo* toggle = ExactMethod(c.gameApi, "SendToggleRideState", 1, true, "System.Int32");
    if (!getSlot || !toggle) { SetText(detail, cap, L"Không resolve được API lên/xuống ngựa"); return false; }
    std::int64_t slot64 = 0;
    if (!InvokeScalar(getSlot, nullptr, slot64, detail, cap) || slot64 < 0 || slot64 > INT32_MAX) return false;
    std::int32_t slot = static_cast<std::int32_t>(slot64);
    void* args[] = { &slot };
    if (!InvokeVoid(toggle, nullptr, args, detail, cap)) return false;
    SetText(detail, cap, desiredRiding ? L"Đã gửi lệnh lên ngựa" : L"Đã gửi lệnh xuống ngựa");
    return true;
}

bool StartPath(int mapID, int x, int y, wchar_t* detail, std::size_t cap) {
    if (mapID <= 0) { SetText(detail, cap, L"MapID đích không hợp lệ"); return false; }
    Classes c{}; if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    Il2CppObject* ap = nullptr; Il2CppClass* ac = nullptr;
    if (!AutoPathInstance(c, ap, ac, detail, cap)) return false;
    const MethodInfo* start = ExactMethod(ac, "StartAutoPath", 3, false,
                                          "System.Int32", "System.Int32", "System.Int32");
    if (!start) { SetText(detail, cap, L"Không resolve đúng StartAutoPath(Int32,Int32,Int32)"); return false; }
    std::int32_t m = mapID, px = x, py = y;
    void* args[] = { &m, &px, &py };
    if (!InvokeVoid(start, ap, args, detail, cap)) return false;
    SetText(detail, cap, L"Đã gửi AutoPath tới map="); AppendInt(detail, cap, mapID);
    Append(detail, cap, L" x="); AppendInt(detail, cap, x); Append(detail, cap, L" y="); AppendInt(detail, cap, y);
    return true;
}

bool StopPath(wchar_t* detail, std::size_t cap) {
    Classes c{}; if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    const MethodInfo* stop = ExactMethod(c.gameApi, "StopAutoPath", 0, true);
    if (!stop) { SetText(detail, cap, L"Không resolve được LuaSystemAPI_Game.StopAutoPath()"); return false; }
    if (!InvokeVoid(stop, nullptr, nullptr, detail, cap)) return false;
    SetText(detail, cap, L"Đã gửi StopAutoPath");
    return true;
}


bool ClickNpc(int npcID, wchar_t* detail, std::size_t cap) {
    if (npcID <= 0) { SetText(detail, cap, L"NPC ID không hợp lệ"); return false; }
    Classes c{}; if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    const MethodInfo* click = ExactMethod(c.gameApi, "ClickNPC", 1, true);
    if (!click) { SetText(detail, cap, L"Không resolve đúng static LuaSystemAPI_Game.ClickNPC(1 arg)"); return false; }
    std::int32_t id = npcID;
    void* args[] = { &id };
    if (!InvokeVoid(click, nullptr, args, detail, cap)) return false;
    SetText(detail, cap, L"Đã gửi ClickNPC id="); AppendInt(detail, cap, npcID);
    return true;
}

enum class UiKind { Button, Toggle, Rect };

struct UiRuntime {
    bool discoveryReady = false;
    bool luaReady = false;
    const Il2CppImage* image = nullptr;
    Il2CppClass* uiObject = nullptr;
    Il2CppClass* button = nullptr;
    Il2CppClass* toggle = nullptr;
    Il2CppClass* rect = nullptr;
    Il2CppClass* executor = nullptr;
    Il2CppClass* guiApi = nullptr;
    Il2CppClass* systemObject = nullptr;
    const Il2CppImage* coreImage = nullptr;
    const Il2CppImage* uiModuleImage = nullptr;
    const Il2CppImage* legacyUnityImage = nullptr;
    Il2CppClass* unityRectTransform = nullptr;
    Il2CppClass* unityTransform = nullptr;
    Il2CppClass* unityGameObject = nullptr;
    Il2CppClass* rectTransformUtility = nullptr;
    Il2CppClass* unityScreen = nullptr;
    bool geometryReady = false;
    Il2CppClass* inputSyncManager = nullptr;
    const MethodInfo* inputSyncGetInstance = nullptr;
    const MethodInfo* inputSyncPress = nullptr;
    const MethodInfo* inputSyncDrag = nullptr;
    const MethodInfo* inputSyncRelease = nullptr;
    const MethodInfo* inputSyncCancel = nullptr;
    FieldInfo* inputSyncDragging = nullptr;
    bool internalPointClickReady = false;
    FieldInfo* instances = nullptr;
    std::vector<std::pair<Il2CppClass*, UiKind>> kindCache{};
};

struct UiControl {
    Il2CppObject* object = nullptr;
    Il2CppClass* klass = nullptr;
    UiKind kind = UiKind::Button;
    Labels labels{};
    // v0.1.8 uses Tag/selectionID as part of the destination identity. Keep it
    // outside Labels because the background UI role scorer does not need it.
    std::wstring tag;
};

UiRuntime g_ui;

template <typename T>
bool ReadLocal(const void* base, std::size_t offset, T& value) {
    if (!base) return false;
    SIZE_T done = 0;
    const auto* address = reinterpret_cast<const unsigned char*>(base) + offset;
    return ReadProcessMemory(GetCurrentProcess(), address, &value, sizeof(value), &done) != FALSE &&
           done == sizeof(value);
}

template <typename T>
bool WriteLocal(void* base, std::size_t offset, const T& value) {
    if (!base) return false;
    SIZE_T done = 0;
    auto* address = reinterpret_cast<unsigned char*>(base) + offset;
    return WriteProcessMemory(GetCurrentProcess(), address, &value, sizeof(value), &done) != FALSE &&
           done == sizeof(value);
}

FieldInfo* FindField(Il2CppClass* klass, const char* name) {
    for (Il2CppClass* current = klass; current; current = g_api.class_get_parent(current)) {
        if (FieldInfo* field = g_api.class_get_field_from_name(current, name)) return field;
    }
    return nullptr;
}

bool IsExecutorClass(Il2CppClass* klass) {
    return klass && ExactMethod(klass, "get_Instance", 0, true) &&
           ExactMethod(klass, "ExecuteScriptFunction", 3, false);
}

bool IsUiObjectClass(Il2CppClass* klass) {
    return klass && FindField(klass, "instances");
}

bool IsButtonClass(Il2CppClass* klass) {
    return klass && ExactMethod(klass, "HandleClickEvent", 0, false);
}

bool IsToggleClass(Il2CppClass* klass) {
    return klass &&
           (ExactMethod(klass, "set_Selected", 1, false, "System.Boolean") ||
            ExactMethod(klass, "HandleSelectEvent", 1, false, "System.Boolean"));
}

bool IsRectClass(Il2CppClass* klass) {
    return klass && ExactMethod(klass, "get_PointerClickHandler", 0, false);
}

void FindUiClassesByMetadata(const Il2CppImage* image) {
    if (!image || !g_api.image_get_class_count || !g_api.image_get_class || !g_api.class_get_name)
        return;
    const std::size_t count = g_api.image_get_class_count(image);
    if (count == 0 || count > 65536) return;
    for (std::size_t i = 0; i < count; ++i) {
        Il2CppClass* klass = g_api.image_get_class(image, i);
        const char* name = klass ? g_api.class_get_name(klass) : nullptr;
        if (!name) continue;
        if (!g_ui.uiObject && Eq(name, "UIObject") && IsUiObjectClass(klass)) g_ui.uiObject = klass;
        else if (!g_ui.button && Eq(name, "UIButton") && IsButtonClass(klass)) g_ui.button = klass;
        else if (!g_ui.toggle && Eq(name, "UIToggle") && IsToggleClass(klass)) g_ui.toggle = klass;
        else if (!g_ui.rect && Eq(name, "UIRectTransform") && IsRectClass(klass)) g_ui.rect = klass;
        else if (!g_ui.executor && Eq(name, "MonoBehaviourExecutor") && IsExecutorClass(klass))
            g_ui.executor = klass;
        if (g_ui.uiObject && g_ui.button && g_ui.toggle && g_ui.rect && g_ui.executor) return;
    }
}

Il2CppClass* FindExecutorClass(const Il2CppImage* image) {
    if (!image) return nullptr;
    const char* namespaces[] = {
        "FGStudio.LuaSystem", "FGStudio.LuaSystem.Base", "FGStudio.LuaSystem.GUI",
        "FGStudio.Engine.Utilities", ""
    };
    for (const char* nameSpace : namespaces) {
        Il2CppClass* klass = g_api.class_from_name(image, nameSpace, "MonoBehaviourExecutor");
        if (IsExecutorClass(klass)) return klass;
    }
    if (!g_api.image_get_class_count || !g_api.image_get_class || !g_api.class_get_name)
        return nullptr;
    const std::size_t count = g_api.image_get_class_count(image);
    if (count == 0 || count > 65536) return nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        Il2CppClass* klass = g_api.image_get_class(image, i);
        if (klass && Eq(g_api.class_get_name(klass), "MonoBehaviourExecutor") &&
            IsExecutorClass(klass)) return klass;
    }
    return nullptr;
}

void AppendMissing(wchar_t* detail, std::size_t cap, const wchar_t* component) {
    if (!detail[0]) SetText(detail, cap, L"Thiếu component:");
    Append(detail, cap, L" ");
    Append(detail, cap, component);
}

bool EnsureUiDiscovery(wchar_t* detail, std::size_t cap) {
    if (g_ui.discoveryReady) return true;
    if (!g_api.LoadUiDiscovery(detail, cap)) return false;
    g_ui.image = Image();
    if (!g_ui.image) { SetText(detail, cap, L"UI discovery: không mở được Assembly-CSharp"); return false; }
    g_ui.uiObject = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.Base", "UIObject");
    g_ui.button = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.GUI", "UIButton");
    g_ui.toggle = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.GUI", "UIToggle");
    g_ui.rect = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.GUI", "UIRectTransform");
    // A matching name is not enough. Drop only the invalid capability, then let the
    // metadata fallback find a same-named class with the required surface elsewhere.
    if (g_ui.uiObject && !IsUiObjectClass(g_ui.uiObject)) g_ui.uiObject = nullptr;
    if (g_ui.button && !IsButtonClass(g_ui.button)) g_ui.button = nullptr;
    if (g_ui.toggle && !IsToggleClass(g_ui.toggle)) g_ui.toggle = nullptr;
    if (g_ui.rect && !IsRectClass(g_ui.rect)) g_ui.rect = nullptr;
    FindUiClassesByMetadata(g_ui.image);
    g_ui.instances = g_ui.uiObject ? FindField(g_ui.uiObject, "instances") : nullptr;
    const bool anyControlClass = g_ui.button || g_ui.toggle || g_ui.rect;
    if (!g_ui.uiObject || !g_ui.instances || !anyControlClass) {
        detail[0] = 0;
        if (!g_ui.uiObject) AppendMissing(detail, cap, L"UIObject(validated)");
        if (!g_ui.instances) AppendMissing(detail, cap, L"UIObject.instances");
        if (!anyControlClass) AppendMissing(detail, cap, L"mọi control class Button/Toggle/Rect(validated)");
        return false;
    }
    g_ui.discoveryReady = true;
    return true;
}

bool EnsureUiLua(bool requireGuiApi, wchar_t* detail, std::size_t cap) {
    if (g_ui.luaReady && (!requireGuiApi || g_ui.guiApi)) return true;
    if (!EnsureUiDiscovery(detail, cap) || !g_api.LoadUiLua(detail, cap)) return false;
    if (!g_ui.executor) g_ui.executor = FindExecutorClass(g_ui.image);
    if (!g_ui.guiApi)
        g_ui.guiApi = g_api.class_from_name(g_ui.image, "FGStudio.LuaSystem.API", "LuaSystemAPI_GUI");
    if (!g_ui.systemObject) {
        const Il2CppImage* corlib = g_api.get_corlib();
        g_ui.systemObject = corlib ? g_api.class_from_name(corlib, "System", "Object") : nullptr;
    }
    if (!g_ui.executor || !g_ui.systemObject || (requireGuiApi && !g_ui.guiApi)) {
        detail[0] = 0;
        if (!g_ui.executor) AppendMissing(detail, cap, L"MonoBehaviourExecutor(any namespace)");
        if (!g_ui.systemObject) AppendMissing(detail, cap, L"System.Object");
        if (requireGuiApi && !g_ui.guiApi) AppendMissing(detail, cap, L"LuaSystemAPI_GUI");
        return false;
    }
    g_ui.luaReady = true;
    return true;
}

bool ReadManagedPointerArray(Il2CppObject* array, std::vector<Il2CppObject*>& values,
                             std::size_t hardLimit) {
    values.clear();
    std::uintptr_t length = 0;
    if (!array || !ReadLocal(array, 0x18, length) || length > hardLimit) return false;
    values.reserve(static_cast<std::size_t>(length));
    for (std::uintptr_t i = 0; i < length; ++i) {
        Il2CppObject* value = nullptr;
        if (!ReadLocal(array, 0x20 + static_cast<std::size_t>(i) * sizeof(void*), value)) return false;
        if (value) values.push_back(value);
    }
    return true;
}


bool ReadScalarField64(Il2CppObject* object, Il2CppClass* klass, const char* name, std::int64_t& out) {
    out = 0;
    FieldInfo* field = FindField(klass, name);
    if (!field) return false;
    const Il2CppType* type = g_api.field_get_type(field);
    char* typeName = type ? g_api.type_get_name(type) : nullptr;
    if (!typeName) return false;
    bool ok = true;
    if (Eq(typeName, "System.Boolean")) { std::uint8_t v = 0; g_api.field_get_value(object, field, &v); out = v ? 1 : 0; }
    else if (Eq(typeName, "System.Int32")) { std::int32_t v = 0; g_api.field_get_value(object, field, &v); out = v; }
    else if (Eq(typeName, "System.UInt32")) { std::uint32_t v = 0; g_api.field_get_value(object, field, &v); out = v; }
    else if (Eq(typeName, "System.Int64")) { std::int64_t v = 0; g_api.field_get_value(object, field, &v); out = v; }
    else if (Eq(typeName, "System.UInt64")) {
        std::uint64_t v = 0; g_api.field_get_value(object, field, &v);
        if (v > static_cast<std::uint64_t>(INT64_MAX)) ok = false; else out = static_cast<std::int64_t>(v);
    } else ok = false;
    g_api.free_fn(typeName);
    return ok;
}

bool ReadScalarMember64(Il2CppObject* object, Il2CppClass* klass, const char* name, std::int64_t& out) {
    wchar_t ignored[96]{};
    std::string getter = std::string("get_") + name;
    if (ScalarGetter64(klass, getter.c_str(), object, out, ignored, _countof(ignored))) return true;
    return ReadScalarField64(object, klass, name, out);
}

bool InvokeEnum32NameArgs(const MethodInfo* method, void* instance, void** args,
                          std::int32_t& value, wchar_t* name, std::size_t nameCap,
                          wchar_t* detail, std::size_t cap) {
    value = 0;
    if (name && nameCap) name[0] = 0;
    if (!method) { SetText(detail, cap, L"Enum method chưa resolve"); return false; }
    void* exc = nullptr;
    Il2CppObject* boxed = g_api.runtime_invoke(method, instance, args, &exc);
    if (exc || !boxed) { SetText(detail, cap, L"Enum getter lỗi/null"); return false; }
    void* raw = g_api.object_unbox(boxed);
    if (!raw) { SetText(detail, cap, L"Không unbox enum"); return false; }
    value = *reinterpret_cast<const std::int32_t*>(raw);
    if (name && nameCap) {
        Il2CppClass* klass = g_api.object_get_class(boxed);
        const MethodInfo* toString = klass ? FindMethod(klass, "ToString", 0) : nullptr;
        Il2CppObject* str = nullptr;
        wchar_t ignored[96]{};
        if (toString && InvokeObject(toString, boxed, str, ignored, _countof(ignored)) && str)
            (void)CopyString(reinterpret_cast<Il2CppString*>(str), name, nameCap);
    }
    return true;
}

const MethodInfo* BagListMethod(const Classes& c) {
    const MethodInfo* method = FindMethod(c.gameApi, "GetItemsAtSite", 1);
    if (method && StaticMethod(method)) return method;
    method = FindMethod(c.shared, "GetItemsAtSite", 1);
    return method && StaticMethod(method) ? method : nullptr;
}

const MethodInfo* BagItemAtSiteMethod(const Classes& c) {
    const MethodInfo* method = FindMethod(c.gameApi, "GetItemAtSite", 2);
    if (method && StaticMethod(method)) return method;
    method = FindMethod(c.shared, "GetItemAtSite", 2);
    return method && StaticMethod(method) ? method : nullptr;
}

bool TryReadBagIndexed(Il2CppObject* collection, Il2CppClass* cc, std::vector<Il2CppObject*>& items) {
    items.clear();
    std::int32_t count = 0;
    wchar_t ignored[128]{};
    if (!ScalarGetter(cc, "get_Count", collection, count, ignored, _countof(ignored)) || count < 0 || count > 1000)
        return false;

    // v1.3 incorrectly fell back to any one-argument get_Item(). A Dictionary<int,T> also has
    // get_Item(Int32), but its argument is a KEY, not an ordinal index. Sparse bag positions therefore
    // throw KeyNotFoundException when looping 0..Count-1. Only accept the exact Int32 indexer and
    // treat any managed exception as a signal to use a safer semantic enumeration path below.
    const MethodInfo* getItem = ExactMethod(cc, "get_Item", 1, false, "System.Int32");
    if (!getItem) return false;

    std::vector<Il2CppObject*> candidate;
    candidate.reserve(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i) {
        std::int32_t index = i;
        void* itemArgs[] = {&index};
        Il2CppObject* item = nullptr;
        wchar_t itemError[96]{};
        if (!InvokeObjectArgs(getItem, collection, itemArgs, item, itemError, _countof(itemError)))
            return false;
        if (item) candidate.push_back(item);
    }
    items.swap(candidate);
    return true;
}

bool LooksLikeBagItemObject(Il2CppObject* object) {
    if (!object) return false;
    Il2CppClass* klass = g_api.object_get_class(object);
    if (!klass) return false;
    const bool hasId = FindField(klass, "ID") || FindMethod(klass, "get_ID", 0);
    const bool hasItemId = FindField(klass, "ItemID") || FindMethod(klass, "get_ItemID", 0);
    return hasId && hasItemId;
}

bool TryUnwrapBagEnumeratorCurrent(Il2CppObject* current, Il2CppObject*& item) {
    item = nullptr;
    if (!current) return true;
    if (LooksLikeBagItemObject(current)) { item = current; return true; }

    // Dictionary enumerators expose KeyValuePair<TKey,TValue>. Unwrap Value without assuming
    // the concrete generic dictionary type. If it is some other enumerable shape, fail softly.
    Il2CppClass* klass = g_api.object_get_class(current);
    const MethodInfo* getValue = klass ? FindMethod(klass, "get_Value", 0) : nullptr;
    if (!getValue) return false;
    Il2CppObject* value = nullptr;
    wchar_t ignored[96]{};
    if (!InvokeObject(getValue, current, value, ignored, _countof(ignored))) return false;
    if (!value || !LooksLikeBagItemObject(value)) return false;
    item = value;
    return true;
}

bool TryReadBagEnumerator(Il2CppObject* collection, Il2CppClass* cc, std::vector<Il2CppObject*>& items) {
    items.clear();
    const MethodInfo* getEnumerator = FindMethod(cc, "GetEnumerator", 0);
    if (!getEnumerator) return false;
    Il2CppObject* enumerator = nullptr;
    wchar_t ignored[128]{};
    if (!InvokeObject(getEnumerator, collection, enumerator, ignored, _countof(ignored)) || !enumerator) return false;
    Il2CppClass* ec = g_api.object_get_class(enumerator);
    if (!ec) return false;
    const MethodInfo* moveNext = FindMethod(ec, "MoveNext", 0);
    const MethodInfo* getCurrent = FindMethod(ec, "get_Current", 0);
    if (!moveNext || !getCurrent) return false;

    std::vector<Il2CppObject*> candidate;
    candidate.reserve(100);
    for (int guard = 0; guard < 1000; ++guard) {
        std::int64_t moved = 0;
        if (!InvokeScalar(moveNext, enumerator, moved, ignored, _countof(ignored))) return false;
        if (!moved) { items.swap(candidate); return true; }
        Il2CppObject* current = nullptr;
        if (!InvokeObject(getCurrent, enumerator, current, ignored, _countof(ignored))) return false;
        Il2CppObject* item = nullptr;
        if (!TryUnwrapBagEnumeratorCurrent(current, item)) return false;
        if (item) candidate.push_back(item);
    }
    return false;
}

bool TryReadBagByPosition(const Classes& c, std::vector<Il2CppObject*>& items) {
    items.clear();
    const MethodInfo* getItem = BagItemAtSiteMethod(c);
    if (!getItem) return false;

    std::vector<Il2CppObject*> candidate;
    candidate.reserve(100);
    int successfulCalls = 0;
    std::int32_t site = 10;
    // Cover both possible 0-based and 1-based 100-slot position conventions.
    for (std::int32_t position = 0; position <= 100; ++position) {
        void* args[] = {&site, &position};
        Il2CppObject* item = nullptr;
        wchar_t ignored[96]{};
        if (!InvokeObjectArgs(getItem, nullptr, args, item, ignored, _countof(ignored))) continue;
        ++successfulCalls;
        if (!item) continue;
        bool duplicate = false;
        for (Il2CppObject* existing : candidate) if (existing == item) { duplicate = true; break; }
        if (!duplicate) candidate.push_back(item);
    }
    if (successfulCalls == 0) return false;
    items.swap(candidate);
    return true;
}

bool ReadBagObjects(const Classes& c, std::vector<Il2CppObject*>& items, int& freeSpace,
                    wchar_t* detail, std::size_t cap) {
    items.clear(); freeSpace = -1;
    const MethodInfo* getItems = BagListMethod(c);
    if (!getItems) { SetText(detail, cap, L"Không resolve GetItemsAtSite"); return false; }
    std::int32_t site = 10;
    void* args[] = {&site};
    Il2CppObject* collection = nullptr;
    if (!InvokeObjectArgs(getItems, nullptr, args, collection, detail, cap) || !collection) {
        SetText(detail, cap, L"GetItemsAtSite(Bag) trả null"); return false;
    }
    Il2CppClass* cc = g_api.object_get_class(collection);
    if (!cc) { SetText(detail, cap, L"Bag collection thiếu class"); return false; }

    // Prefer the cheap list path, but never let one managed indexer exception kill the full scan.
    // Sparse/keyed collections fall through to enumerator, then to the semantic per-position API.
    bool readOk = TryReadBagIndexed(collection, cc, items);
    if (!readOk) readOk = TryReadBagEnumerator(collection, cc, items);
    if (!readOk) readOk = TryReadBagByPosition(c, items);
    if (!readOk) {
        SetText(detail, cap, L"Không enumerate được tay nải (indexed/enumerator/position đều fail)");
        return false;
    }

    wchar_t ignored[128]{};
    if (!StaticScalar(c.gameApi, "GetFreeBagSpace", freeSpace, ignored, _countof(ignored)) || freeSpace < 0) freeSpace = -1;
    return true;
}

bool StaticBoolByItemID(Il2CppClass* gameApi, const char* methodName, std::int32_t itemID, bool& out) {
    out = false;
    const MethodInfo* method = FindMethod(gameApi, methodName, 1);
    if (!method || !StaticMethod(method)) return false;
    void* args[] = {&itemID}; std::int64_t value = 0; wchar_t ignored[96]{};
    if (!InvokeScalarArgs(method, nullptr, args, value, ignored, _countof(ignored))) return false;
    out = value != 0; return true;
}

bool FillBagItemSnapshot(const Classes& c, Il2CppObject* object, BagItemSnapshot& out,
                         wchar_t* detail, std::size_t cap) {
    out = {};
    if (!object) return false;
    Il2CppClass* klass = g_api.object_get_class(object);
    if (!klass) return false;
    std::int64_t v = 0;
    if (!ReadScalarMember64(object, klass, "ID", v) || v <= 0) { SetText(detail, cap, L"Bag item thiếu ID"); return false; }
    out.instanceID = v;
    if (!ReadScalarMember64(object, klass, "ItemID", v) || v <= 0 || v > INT32_MAX) { SetText(detail, cap, L"Bag item thiếu ItemID"); return false; }
    out.itemID = static_cast<std::int32_t>(v);
    if (ReadScalarMember64(object, klass, "Site", v) && v >= INT32_MIN && v <= INT32_MAX) out.site = static_cast<std::int32_t>(v);
    if (ReadScalarMember64(object, klass, "Position", v) && v >= INT32_MIN && v <= INT32_MAX) out.position = static_cast<std::int32_t>(v);
    if (ReadScalarMember64(object, klass, "Quantity", v) && v >= 0 && v <= INT32_MAX) out.quantity = static_cast<std::int32_t>(v);
    if (ReadScalarMember64(object, klass, "Bound", v)) out.bound = v ? 1 : 0;

    const MethodInfo* getName = FindMethod(c.gameApi, "GetItemName", 1);
    if (getName && StaticMethod(getName)) {
        std::int32_t id = out.itemID; void* args[] = {&id}; Il2CppObject* str = nullptr; wchar_t ignored[96]{};
        if (InvokeObjectArgs(getName, nullptr, args, str, ignored, _countof(ignored)) && str)
            (void)CopyString(reinterpret_cast<Il2CppString*>(str), out.name, _countof(out.name));
    }

    const MethodInfo* getType = FindMethod(c.gameApi, "GetItemType", 1);
    if (getType && StaticMethod(getType)) {
        std::int32_t id = out.itemID; void* args[] = {&id};
        (void)InvokeEnum32NameArgs(getType, nullptr, args, out.itemTypeCode, out.itemType, _countof(out.itemType), detail, cap);
    }
    out.isEquip = (_wcsicmp(out.itemType, L"Equip") == 0) ? 1 : 0;
    if (out.isEquip) {
        const MethodInfo* getEquip = FindMethod(c.gameApi, "GetEquipType", 1);
        if (getEquip && StaticMethod(getEquip)) {
            std::int32_t id = out.itemID; void* args[] = {&id};
            (void)InvokeEnum32NameArgs(getEquip, nullptr, args, out.equipTypeCode, out.equipType, _countof(out.equipType), detail, cap);
        }
        out.isWeapon = (_wcsicmp(out.equipType, L"Weapon") == 0) ? 1 : 0;
    }
    bool flag = false;
    if (StaticBoolByItemID(c.gameApi, "IsItemThrowable", out.itemID, flag)) out.throwable = flag ? 1 : 0;
    if (StaticBoolByItemID(c.gameApi, "IsItemSellable", out.itemID, flag)) out.sellable = flag ? 1 : 0;
    return true;
}

bool ReadBagPage(int start, Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap)) return false;
    std::vector<Il2CppObject*> items; int freeSpace = -1;
    if (!ReadBagObjects(c, items, freeSpace, detail, cap)) return false;
    start = std::clamp(start, 0, static_cast<int>(items.size()));
    response.bagPage.totalCount = static_cast<std::int32_t>(items.size());
    response.bagPage.pageStart = start;
    response.bagPage.freeBagSpace = freeSpace;
    const int count = std::min<int>(static_cast<int>(kBagPageCapacity), static_cast<int>(items.size()) - start);
    for (int i = 0; i < count; ++i) {
        if (!FillBagItemSnapshot(c, items[static_cast<std::size_t>(start + i)], response.bagPage.items[i], detail, cap)) return false;
    }
    response.bagPage.pageCount = count;
    response.value0 = response.bagPage.totalCount;
    response.value1 = freeSpace;
    SetText(detail, cap, L"Bag semantic page "); AppendInt(detail, cap, start); Append(detail, cap, L"+"); AppendInt(detail, cap, count);
    return true;
}

std::int64_t RequestInstanceID(std::int32_t low, std::int32_t high) {
    const std::uint64_t lo = static_cast<std::uint32_t>(low);
    const std::uint64_t hi = static_cast<std::uint32_t>(high);
    return static_cast<std::int64_t>((hi << 32) | lo);
}

bool FindFreshBagItem(const Classes& c, std::int64_t instanceID, std::int32_t expectedItemID,
                      Il2CppObject*& object, BagItemSnapshot& item, wchar_t* detail, std::size_t cap) {
    object = nullptr; item = {};
    std::vector<Il2CppObject*> items; int freeSpace = -1;
    if (!ReadBagObjects(c, items, freeSpace, detail, cap)) return false;
    for (Il2CppObject* candidate : items) {
        BagItemSnapshot current{}; wchar_t ignored[160]{};
        if (!FillBagItemSnapshot(c, candidate, current, ignored, _countof(ignored))) continue;
        if (current.instanceID == instanceID && current.itemID == expectedItemID && current.site == 10) {
            object = candidate; item = current; return true;
        }
    }
    SetText(detail, cap, L"Item instance đã đổi/mất; yêu cầu re-scan");
    return false;
}

bool SendNetworkPacket(const Classes& c, std::int32_t packetID, const std::string& payload,
                       wchar_t* detail, std::size_t cap) {
    if (!c.networkApi) { SetText(detail, cap, L"Thiếu LuaSystemAPI_Network"); return false; }
    if (!g_api.string_new && !Resolve(g_api.module, "il2cpp_string_new", g_api.string_new)) {
        SetText(detail, cap, L"Thiếu il2cpp_string_new"); return false;
    }
    const MethodInfo* send = ExactMethod(c.networkApi, "SendPacket", 2, true, "System.Int32", "System.String");
    if (!send) { SetText(detail, cap, L"Không resolve Network.SendPacket(Int32,String)"); return false; }
    Il2CppString* data = g_api.string_new(payload.c_str());
    if (!data) { SetText(detail, cap, L"Không tạo được packet payload"); return false; }
    void* args[] = {&packetID, &data};
    return InvokeVoid(send, nullptr, args, detail, cap);
}

bool DropBagItem(std::int64_t instanceID, std::int32_t expectedItemID, Response& response,
                 wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    Il2CppObject* object = nullptr; BagItemSnapshot item{};
    if (!FindFreshBagItem(c, instanceID, expectedItemID, object, item, detail, cap)) return false;
    if (!item.throwable) { SetText(detail, cap, L"Item hiện tại IsItemThrowable=false; chặn vứt"); return false; }
    if (item.itemID >= 40000000 && item.itemID < 50000000) { SetText(detail, cap, L"Item quest-family; chặn vứt fail-closed"); return false; }
    const std::string payload = std::string("4:") + std::to_string(static_cast<long long>(item.instanceID));
    if (!SendNetworkPacket(c, 100005, payload, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value64_0 = item.instanceID; response.value0 = item.itemID;
    SetText(detail, cap, L"Đã gửi Abandon semantic instance="); AppendInt64(detail, cap, item.instanceID);
    return true;
}

bool StringGetter(Il2CppObject* object, Il2CppClass* klass, const char* name,
                  std::wstring& output) {
    output.clear();
    wchar_t ignored[128]{};
    Il2CppObject* boxed = nullptr;
    const MethodInfo* getter = FindMethod(klass, name, 0);
    if (!getter || !InvokeObject(getter, ManagedThis(object), boxed, ignored, _countof(ignored)) || !boxed) return false;
    wchar_t value[1024]{};
    if (!CopyString(reinterpret_cast<Il2CppString*>(boxed), value, _countof(value))) return false;
    output = value;
    return true;
}

bool EnsureManagedStringFactory() {
    return g_api.string_new || (g_api.module && Resolve(g_api.module, "il2cpp_string_new", g_api.string_new));
}

bool IsPrimitiveField(FieldInfo* field) {
    return field && (FieldType(field, "System.Boolean") || FieldType(field, "System.Byte") ||
                     FieldType(field, "System.SByte") || FieldType(field, "System.Int16") ||
                     FieldType(field, "System.UInt16") || FieldType(field, "System.Int32") ||
                     FieldType(field, "System.UInt32") || FieldType(field, "System.Int64") ||
                     FieldType(field, "System.UInt64") || FieldType(field, "System.Single") ||
                     FieldType(field, "System.Double"));
}

// This is the v0.1.8 ObjMember() lookup order. The production route used to
// stop after a property getter/backing string field, while the probe also
// handled UI wrappers that expose CoreChildren, Text or Tag through an
// indexer-like getter (get_Item/GetValue/Get/RawGet).
bool ReadUiMemberObject(Il2CppObject* object, Il2CppClass* klass, const char* member,
                        Il2CppObject*& output) {
    output = nullptr;
    if (!object || !klass || !member) return false;

    const std::string getterName = std::string("get_") + member;
    if (const MethodInfo* getter = FindMethod(klass, getterName.c_str(), 0)) {
        wchar_t ignored[128]{};
        Il2CppObject* value = nullptr;
        if (InvokeObject(getter, ManagedThis(object), value, ignored, _countof(ignored)) && value) {
            output = value;
            return true;
        }
    }

    // Only read reference/string fields through the object pointer path. A
    // primitive field must be handled by ReadUiMemberText below instead of
    // being misinterpreted as a pointer.
    if ((!g_api.class_is_valuetype || !g_api.class_is_valuetype(klass))) {
        if (FieldInfo* field = FindField(klass, member); field && !IsPrimitiveField(field)) {
            Il2CppObject* value = nullptr;
            g_api.field_get_value(object, field, &value);
            if (value) {
                output = value;
                return true;
            }
        }
    }

    if (!EnsureManagedStringFactory()) return false;
    Il2CppString* key = g_api.string_new(member);
    if (!key) return false;
    for (const char* methodName : {"get_Item", "GetValue", "Get", "RawGet"}) {
        if (const MethodInfo* method = FindMethod(klass, methodName, 1)) {
            void* args[] = {&key};
            wchar_t ignored[128]{};
            Il2CppObject* value = nullptr;
            if (InvokeObject(method, ManagedThis(object), value, ignored, _countof(ignored)) && value) {
                output = value;
                return true;
            }
        }
    }
    return false;
}

bool ManagedObjectText(Il2CppObject* object, std::wstring& output) {
    output.clear();
    if (!object) return false;
    Il2CppClass* klass = g_api.object_get_class(object);
    const char* className = klass && g_api.class_get_name ? g_api.class_get_name(klass) : nullptr;
    if (className && (Eq(className, "String") || Eq(className, "System.String"))) {
        wchar_t value[1024]{};
        if (!CopyString(reinterpret_cast<Il2CppString*>(object), value, _countof(value))) return false;
        output = value;
        return !output.empty();
    }

    void* raw = g_api.object_unbox(object);
    if (!className || !raw) return false;
    if (Eq(className, "Boolean") || Eq(className, "System.Boolean")) {
        output = *reinterpret_cast<const std::uint8_t*>(raw) ? L"1" : L"0"; return true;
    }
    if (Eq(className, "Int32") || Eq(className, "System.Int32")) {
        output = std::to_wstring(*reinterpret_cast<const std::int32_t*>(raw)); return true;
    }
    if (Eq(className, "UInt32") || Eq(className, "System.UInt32")) {
        output = std::to_wstring(*reinterpret_cast<const std::uint32_t*>(raw)); return true;
    }
    if (Eq(className, "Int64") || Eq(className, "System.Int64")) {
        output = std::to_wstring(*reinterpret_cast<const std::int64_t*>(raw)); return true;
    }
    if (Eq(className, "UInt64") || Eq(className, "System.UInt64")) {
        output = std::to_wstring(*reinterpret_cast<const std::uint64_t*>(raw)); return true;
    }
    if (Eq(className, "Int16") || Eq(className, "System.Int16")) {
        output = std::to_wstring(*reinterpret_cast<const std::int16_t*>(raw)); return true;
    }
    if (Eq(className, "UInt16") || Eq(className, "System.UInt16")) {
        output = std::to_wstring(*reinterpret_cast<const std::uint16_t*>(raw)); return true;
    }
    return false;
}

bool ReadUiMemberText(Il2CppObject* object, Il2CppClass* klass, const char* member,
                      std::wstring& output) {
    output.clear();
    if (!object || !klass || !member) return false;
    Il2CppObject* value = nullptr;
    if (ReadUiMemberObject(object, klass, member, value) && ManagedObjectText(value, output)) return true;

    FieldInfo* field = FindField(klass, member);
    if (!field) return false;
    if (FieldType(field, "System.String")) {
        Il2CppString* stringValue = nullptr;
        g_api.field_get_value(object, field, &stringValue);
        if (!stringValue) return false;
        wchar_t buffer[1024]{};
        if (!CopyString(stringValue, buffer, _countof(buffer))) return false;
        output = buffer;
        return !output.empty();
    }
    if (FieldType(field, "System.Boolean")) { std::uint8_t value{}; g_api.field_get_value(object, field, &value); output = value ? L"1" : L"0"; return true; }
    if (FieldType(field, "System.Int32")) { std::int32_t value{}; g_api.field_get_value(object, field, &value); output = std::to_wstring(value); return true; }
    if (FieldType(field, "System.UInt32")) { std::uint32_t value{}; g_api.field_get_value(object, field, &value); output = std::to_wstring(value); return true; }
    if (FieldType(field, "System.Int64")) { std::int64_t value{}; g_api.field_get_value(object, field, &value); output = std::to_wstring(value); return true; }
    if (FieldType(field, "System.UInt64")) { std::uint64_t value{}; g_api.field_get_value(object, field, &value); output = std::to_wstring(value); return true; }
    return false;
}

// v0.1.8's TextMember() accepts a property, backing field, or dynamic member.
// Keep the same order for visible text, Tag/selectionID, and child labels.
bool ReadUiString(Il2CppObject* object, Il2CppClass* klass, const char* property,
                  std::wstring& output) {
    return ReadUiMemberText(object, klass, property, output);
}

bool ObjectGetter(Il2CppObject* object, Il2CppClass* klass, const char* name,
                  Il2CppObject*& output) {
    // Existing production callers historically passed "get_Parent" while
    // v0.1.8's ObjMember() accepts the property name "Parent". Normalize both
    // forms so the proven dynamic-member fallback is used by every caller.
    const char* member = name;
    if (name && name[0] == 'g' && name[1] == 'e' && name[2] == 't' && name[3] == '_')
        member = name + 4;
    return ReadUiMemberObject(object, klass, member, output);
}

const Il2CppImage* GeometryImage(ImageSlot slot) {
    switch (slot) {
        case ImageSlot::CoreModule: return g_ui.coreImage;
        case ImageSlot::UiModule: return g_ui.uiModuleImage;
        case ImageSlot::LegacyUnity: return g_ui.legacyUnityImage;
    }
    return nullptr;
}

Il2CppClass* ResolveGeometryClass(GeometryClass role) {
    const unity_geometry_logic::SearchPlan plan = unity_geometry_logic::PlanFor(role);
    for (ImageSlot slot : plan.images) {
        const Il2CppImage* image = GeometryImage(slot);
        if (!image) continue;
        Il2CppClass* klass = g_api.class_from_name(image, "UnityEngine", plan.className);
        if (klass) return klass;
    }
    return nullptr;
}

void OpenUnityImages() {
    if (!g_ui.coreImage)
        g_ui.coreImage = ImageForAssembly("UnityEngine.CoreModule", "UnityEngine.CoreModule.dll");
    if (!g_ui.uiModuleImage)
        g_ui.uiModuleImage = ImageForAssembly("UnityEngine.UIModule", "UnityEngine.UIModule.dll");
    if (!g_ui.legacyUnityImage)
        g_ui.legacyUnityImage = ImageForAssembly("UnityEngine", "UnityEngine.dll");
}

void AppendGeometryAvailability(wchar_t* detail, std::size_t cap) {
    Append(detail, cap, L" • assembly Core=");
    Append(detail, cap, g_ui.coreImage ? L"OK" : L"NO");
    Append(detail, cap, L" UI=");
    Append(detail, cap, g_ui.uiModuleImage ? L"OK" : L"NO");
    Append(detail, cap, L" Legacy=");
    Append(detail, cap, g_ui.legacyUnityImage ? L"OK" : L"NO");
}

bool EnsureUiGeometry(wchar_t* detail, std::size_t cap) {
    if (g_ui.geometryReady) return true;
    if (!EnsureUiDiscovery(detail, cap)) return false;
    OpenUnityImages();
    if (!g_ui.coreImage && !g_ui.uiModuleImage && !g_ui.legacyUnityImage) {
        SetText(detail, cap, L"Không mở được CoreModule/UIModule/UnityEngine.dll để hit-test tọa độ");
        return false;
    }
    g_ui.unityRectTransform = ResolveGeometryClass(GeometryClass::RectTransform);
    g_ui.unityTransform = ResolveGeometryClass(GeometryClass::Transform);
    g_ui.unityGameObject = ResolveGeometryClass(GeometryClass::GameObject);
    g_ui.rectTransformUtility = ResolveGeometryClass(GeometryClass::RectTransformUtility);
    g_ui.unityScreen = ResolveGeometryClass(GeometryClass::Screen);
    const std::pair<GeometryClass, Il2CppClass*> required[] = {
        {GeometryClass::RectTransform, g_ui.unityRectTransform},
        {GeometryClass::Transform, g_ui.unityTransform},
        {GeometryClass::GameObject, g_ui.unityGameObject},
        {GeometryClass::RectTransformUtility, g_ui.rectTransformUtility},
        {GeometryClass::Screen, g_ui.unityScreen},
    };
    bool missing = false;
    for (const auto& item : required) {
        if (item.second) continue;
        if (!missing) SetText(detail, cap, L"Geometry thiếu class: ");
        else Append(detail, cap, L", ");
        Append(detail, cap, unity_geometry_logic::ClassLabel(item.first));
        missing = true;
    }
    if (missing) {
        AppendGeometryAvailability(detail, cap);
        return false;
    }
    g_ui.geometryReady = true;
    return true;
}

bool StartsWith(const char* value, const char* prefix) {
    if (!value || !prefix) return false;
    while (*prefix) {
        if (*value++ != *prefix++) return false;
    }
    return true;
}

bool AssignableObject(Il2CppClass* base, Il2CppObject* object) {
    if (!base || !object) return false;
    Il2CppClass* actual = g_api.object_get_class(object);
    return actual && g_api.class_is_assignable_from(base, actual);
}

bool NormalizeRectTransformObject(Il2CppObject* candidate, Il2CppObject*& rectTransform) {
    rectTransform = nullptr;
    if (!candidate) return false;
    if (AssignableObject(g_ui.unityRectTransform, candidate)) {
        rectTransform = candidate;
        return true;
    }
    if (!AssignableObject(g_ui.unityGameObject, candidate)) return false;
    Il2CppClass* actual = g_api.object_get_class(candidate);
    Il2CppObject* transform = nullptr;
    wchar_t ignored[128]{};
    const MethodInfo* getter = actual ? FindMethod(actual, "get_transform", 0) : nullptr;
    if (!getter || !InvokeObject(getter, candidate, transform, ignored, _countof(ignored)) ||
        !AssignableObject(g_ui.unityRectTransform, transform)) return false;
    rectTransform = transform;
    return true;
}

bool GetterMayExposeRectTransform(const MethodInfo* method) {
    if (!method || StaticMethod(method) || g_api.method_get_param_count(method) != 0) return false;
    const Il2CppType* returnType = g_api.method_get_return_type(method);
    Il2CppClass* returnClass = returnType ? g_api.class_from_type(returnType) : nullptr;
    return returnClass &&
        (g_api.class_is_assignable_from(g_ui.unityRectTransform, returnClass) ||
         g_api.class_is_assignable_from(g_ui.unityTransform, returnClass) ||
         g_api.class_is_assignable_from(g_ui.unityGameObject, returnClass));
}

bool ResolveRectTransform(Il2CppObject* object, Il2CppClass* klass,
                          Il2CppObject*& rectTransform) {
    rectTransform = nullptr;
    if (!object || !klass) return false;
    if (NormalizeRectTransformObject(object, rectTransform)) return true;

    if (g_api.class_get_methods && g_api.method_get_name) {
        for (Il2CppClass* current = klass; current; current = g_api.class_get_parent(current)) {
            void* iterator = nullptr;
            int inspected = 0;
            while (const MethodInfo* method = g_api.class_get_methods(current, &iterator)) {
                if (++inspected > 512) break;
                const char* name = g_api.method_get_name(method);
                if (!StartsWith(name, "get_") || !GetterMayExposeRectTransform(method)) continue;
                Il2CppObject* candidate = nullptr;
                wchar_t ignored[128]{};
                if (InvokeObject(method, object, candidate, ignored, _countof(ignored)) &&
                    NormalizeRectTransformObject(candidate, rectTransform)) return true;
            }
        }
    }

    // Named fallbacks keep the feature usable if method enumeration exports are
    // stripped while still invoking only getters with a validated Unity return type.
    const char* getters[] = {
        "get_RectTransform", "get_CoreRectTransform", "get_Transform",
        "get_CoreTransform", "get_GameObject", "get_CoreGameObject"
    };
    for (const char* name : getters) {
        const MethodInfo* method = FindMethod(klass, name, 0);
        if (!GetterMayExposeRectTransform(method)) continue;
        Il2CppObject* candidate = nullptr;
        wchar_t ignored[128]{};
        if (InvokeObject(method, object, candidate, ignored, _countof(ignored)) &&
            NormalizeRectTransformObject(candidate, rectTransform)) return true;
    }
    return false;
}

struct UnityVector2 { float x = 0.0f; float y = 0.0f; };
struct UnityRectValue { float x = 0.0f; float y = 0.0f; float width = 0.0f; float height = 0.0f; };

bool ReadInputSyncDragging(Il2CppObject* manager, bool& dragging,
                           wchar_t* detail, std::size_t cap) {
    dragging = false;
    if (!manager || !g_ui.inputSyncDragging) {
        SetText(detail, cap, L"InputSyncManager._uiDragging chưa resolve");
        return false;
    }
    std::uint8_t value = 0;
    g_api.field_get_value(manager, g_ui.inputSyncDragging, &value);
    dragging = value != 0;
    return true;
}

bool EnsureInternalPointClick(wchar_t* detail, std::size_t cap) {
    if (g_ui.internalPointClickReady) return true;
    if (!g_api.Load(detail, cap)) return false;
    if (!g_ui.image) g_ui.image = Image();
    if (!g_ui.image) {
        SetText(detail, cap, L"Click nội bộ: không mở được Assembly-CSharp");
        return false;
    }

    g_ui.inputSyncManager = g_api.class_from_name(g_ui.image, "", "InputSyncManager");
    OpenUnityImages();
    if (!g_ui.unityScreen) g_ui.unityScreen = ResolveGeometryClass(GeometryClass::Screen);

    constexpr auto plan = DispatchPlan();
    if (g_ui.inputSyncManager) {
        g_ui.inputSyncGetInstance = ExactMethod(g_ui.inputSyncManager, "get_Instance", 0, true);
        g_ui.inputSyncPress = ExactMethod(
            g_ui.inputSyncManager, plan[0].methodName, plan[0].parameterCount, false,
            "System.Int32", "UnityEngine.Vector2");
        g_ui.inputSyncDrag = ExactMethod(
            g_ui.inputSyncManager, "UpdateUIDrag", 1, false, "UnityEngine.Vector2");
        g_ui.inputSyncRelease = ExactMethod(
            g_ui.inputSyncManager, plan[1].methodName, plan[1].parameterCount, false,
            "UnityEngine.Vector2");
        g_ui.inputSyncCancel = ExactMethod(g_ui.inputSyncManager, "CancelUIDragState", 0, false);
        g_ui.inputSyncDragging = FindField(g_ui.inputSyncManager, "_uiDragging");
    }

    const bool methodsValid = g_ui.inputSyncGetInstance && g_ui.inputSyncPress &&
        g_ui.inputSyncDrag && g_ui.inputSyncRelease && g_ui.inputSyncCancel &&
        ReturnType(g_ui.inputSyncPress, "System.Void") &&
        ReturnType(g_ui.inputSyncDrag, "System.Void") &&
        ReturnType(g_ui.inputSyncRelease, "System.Void") &&
        ReturnType(g_ui.inputSyncCancel, "System.Void");
    if (!g_ui.inputSyncManager || !g_ui.unityScreen || !methodsValid ||
        !FieldType(g_ui.inputSyncDragging, "System.Boolean")) {
        SetText(detail, cap, L"Không resolve đúng InputSyncManager press/release/drag-state hoặc Unity Screen");
        return false;
    }
    g_ui.internalPointClickReady = true;
    return true;
}

bool BuildInputSyncScreenPoint(int normalizedX, int normalizedY, UnityVector2& point,
                               wchar_t* detail, std::size_t cap) {
    if (!EnsureInternalPointClick(detail, cap)) return false;
    if (!internal_ui_click_logic::IsNormalizedCoordinate(
            normalizedX, fixed_slot_sell_logic::kCoordinateScale) ||
        !internal_ui_click_logic::IsNormalizedCoordinate(
            normalizedY, fixed_slot_sell_logic::kCoordinateScale)) {
        SetText(detail, cap, L"Tọa độ UI chuẩn hóa nằm ngoài client");
        return false;
    }
    std::int32_t width = 0;
    std::int32_t height = 0;
    if (!StaticScalar(g_ui.unityScreen, "get_width", width, detail, cap) || width <= 0 ||
        !StaticScalar(g_ui.unityScreen, "get_height", height, detail, cap) || height <= 0) {
        SetText(detail, cap, L"Không đọc được Unity Screen.width/height cho click nội bộ");
        return false;
    }
    point.x = static_cast<float>(static_cast<double>(normalizedX) * width /
                                 fixed_slot_sell_logic::kCoordinateScale);
    const double topY = static_cast<double>(normalizedY) * height /
                        fixed_slot_sell_logic::kCoordinateScale;
    point.y = static_cast<float>(height - 1.0 - topY);
    return true;
}

void CancelOwnedInputSyncDrag(Il2CppObject* manager) {
    if (!manager || !g_ui.inputSyncCancel) return;
    wchar_t ignored[128]{};
    (void)InvokeVoid(g_ui.inputSyncCancel, manager, nullptr, ignored, _countof(ignored));
}

bool InvokeInternalPointClick(int normalizedX, int normalizedY,
                          wchar_t* detail, std::size_t cap,
                          bool rawMacro = false) {
    UnityVector2 point{};
    if (!BuildInputSyncScreenPoint(normalizedX, normalizedY, point, detail, cap)) return false;

    Il2CppObject* manager = nullptr;
    if (!InvokeObject(g_ui.inputSyncGetInstance, nullptr, manager, detail, cap) || !manager) {
        SetText(detail, cap, L"InputSyncManager.Instance chưa sẵn sàng");
        return false;
    }

    bool dragging = false;
    if (!rawMacro) {
        if (!ReadInputSyncDragging(manager, dragging, detail, cap)) return false;
        if (dragging) {
            SetText(detail, cap, L"InputSyncManager đang giữ UI drag; không chồng click nội bộ");
            return false;
        }
    } else {
        // Raw trade macro: stale drag evidence is cleanup-only, not a UI gate.
        wchar_t ignored[128]{};
        if (ReadInputSyncDragging(manager, dragging, ignored, _countof(ignored)) && dragging)
            CancelOwnedInputSyncDrag(manager);
    }

    std::int32_t button = internal_ui_click_logic::kLeftButton;
    void* pressArgs[] = {&button, &point};
    if (!InvokeVoid(g_ui.inputSyncPress, manager, pressArgs, detail, cap)) {
        bool ownsDrag = false;
        if (ReadInputSyncDragging(manager, ownsDrag, detail, cap) && ownsDrag)
            CancelOwnedInputSyncDrag(manager);
        SetText(detail, cap, L"InputSyncManager.TryClickUI ném exception");
        return false;
    }

    if (!rawMacro) {
        if (!ReadInputSyncDragging(manager, dragging, detail, cap)) return false;
        if (!dragging) {
            SetText(detail, cap, L"InputSyncManager raycast không bắt được UI tại tọa độ đã gán");
            return false;
        }
    }

    void* releaseArgs[] = {&point};
    if (!InvokeVoid(g_ui.inputSyncRelease, manager, releaseArgs, detail, cap)) {
        CancelOwnedInputSyncDrag(manager);
        SetText(detail, cap, L"InputSyncManager.EndUIDrag ném exception; đã hủy drag nội bộ");
        return false;
    }

    if (!rawMacro) {
        if (!ReadInputSyncDragging(manager, dragging, detail, cap)) return false;
        if (dragging) {
            CancelOwnedInputSyncDrag(manager);
            SetText(detail, cap, L"InputSyncManager chưa nhả UI drag; đã hủy và dừng fail-closed");
            return false;
        }
    } else {
        // Best-effort cleanup only. Raycast/drag-state evidence never decides
        // whether the user's saved trade macro advances to Delay/Repeat/next row.
        wchar_t ignored[128]{};
        if (ReadInputSyncDragging(manager, dragging, ignored, _countof(ignored)) && dragging)
            CancelOwnedInputSyncDrag(manager);
    }
    return true;
}

bool ClickInternalPoint(int normalizedX, int normalizedY, bool rawMacro, Response& response,
                        wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    if (!InvokeInternalPointClick(normalizedX, normalizedY, detail, cap, rawMacro)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, rawMacro
        ? L"RAW MACRO InputSync: TryClickUI → EndUIDrag; không gate theo raycast/_uiDragging"
        : L"InputSync click nội bộ hoàn chỉnh: TryClickUI → EndUIDrag");
    return true;
}

bool ClickInternalPointRawTest(int normalizedX, int normalizedY, Response& response,
                               wchar_t* detail, std::size_t cap) {
    if (!InvokeInternalPointClick(normalizedX, normalizedY, detail, cap, true)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, L"RAW SCAN InputSync PASS: TryClickUI → EndUIDrag • không SafeForAction");
    return true;
}


bool InvokeInternalPointDrag(int startNormalizedX, int startNormalizedY,
                             int endNormalizedX, int endNormalizedY,
                             wchar_t* detail, std::size_t cap) {
    UnityVector2 start{}, end{};
    if (!BuildInputSyncScreenPoint(startNormalizedX, startNormalizedY, start, detail, cap) ||
        !BuildInputSyncScreenPoint(endNormalizedX, endNormalizedY, end, detail, cap)) return false;
    Il2CppObject* manager = nullptr;
    if (!InvokeObject(g_ui.inputSyncGetInstance, nullptr, manager, detail, cap) || !manager) {
        SetText(detail, cap, L"InputSyncManager.Instance chưa sẵn sàng cho drag"); return false;
    }
    bool dragging = false;
    if (!ReadInputSyncDragging(manager, dragging, detail, cap)) return false;
    if (dragging) { CancelOwnedInputSyncDrag(manager); SetText(detail, cap, L"InputSyncManager có drag cũ; đã cancel"); return false; }
    std::int32_t button = internal_ui_click_logic::kLeftButton;
    void* pressArgs[] = {&button, &start};
    if (!InvokeVoid(g_ui.inputSyncPress, manager, pressArgs, detail, cap)) {
        CancelOwnedInputSyncDrag(manager); SetText(detail, cap, L"Hidden drag: TryClickUI thất bại"); return false;
    }
    if (!ReadInputSyncDragging(manager, dragging, detail, cap) || !dragging) {
        CancelOwnedInputSyncDrag(manager); SetText(detail, cap, L"Hidden drag: START không tạo được UI drag target"); return false;
    }
    constexpr int kSteps = 12;
    for (int i=1;i<=kSteps;++i) {
        const float q=static_cast<float>(i)/static_cast<float>(kSteps);
        UnityVector2 point{}; point.x=start.x+(end.x-start.x)*q; point.y=start.y+(end.y-start.y)*q;
        void* args[] = {&point};
        if (!InvokeVoid(g_ui.inputSyncDrag, manager, args, detail, cap)) {
            CancelOwnedInputSyncDrag(manager); SetText(detail, cap, L"Hidden drag: UpdateUIDrag thất bại; đã cancel"); return false;
        }
    }
    void* releaseArgs[] = {&end};
    if (!InvokeVoid(g_ui.inputSyncRelease, manager, releaseArgs, detail, cap)) {
        CancelOwnedInputSyncDrag(manager); SetText(detail, cap, L"Hidden drag: EndUIDrag thất bại; đã cancel"); return false;
    }
    if (!ReadInputSyncDragging(manager, dragging, detail, cap)) return false;
    if (dragging) { CancelOwnedInputSyncDrag(manager); SetText(detail, cap, L"Hidden drag chưa nhả state; đã cancel"); return false; }
    return true;
}

bool DragInternalPoint(int startNormalizedX, int startNormalizedY, int packedEnd,
                       Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    const int qx=(static_cast<unsigned int>(packedEnd)>>16)&0xffff;
    const int qy=static_cast<unsigned int>(packedEnd)&0xffff;
    const int endNormalizedX=static_cast<int>((static_cast<long long>(qx)*fixed_slot_sell_logic::kCoordinateScale+32767)/65535);
    const int endNormalizedY=static_cast<int>((static_cast<long long>(qy)*fixed_slot_sell_logic::kCoordinateScale+32767)/65535);
    if (!InvokeInternalPointDrag(startNormalizedX,startNormalizedY,endNormalizedX,endNormalizedY,detail,cap)) return false;
    response.resultCode=static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, L"InputSync hidden drag PASS: TryClickUI → UpdateUIDrag x12 → EndUIDrag");
    return true;
}

bool ReadRectArea(Il2CppObject* rectTransform, float& area) {
    area = 0.0f;
    Il2CppClass* klass = rectTransform ? g_api.object_get_class(rectTransform) : nullptr;
    const MethodInfo* getter = klass ? FindMethod(klass, "get_rect", 0) : nullptr;
    if (!getter) return false;
    Il2CppObject* boxed = nullptr;
    wchar_t ignored[128]{};
    if (!InvokeObject(getter, rectTransform, boxed, ignored, _countof(ignored)) || !boxed) return false;
    const void* raw = g_api.object_unbox(boxed);
    if (!raw) return false;
    const UnityRectValue value = *reinterpret_cast<const UnityRectValue*>(raw);
    area = std::fabs(value.width * value.height);
    return std::isfinite(area) && area > 0.0f;
}

bool BuildUnityScreenPoint(int normalizedX, int normalizedY, UnityVector2& point,
                           const MethodInfo*& contains, wchar_t* detail, std::size_t cap) {
    contains = nullptr;
    if (!EnsureUiGeometry(detail, cap)) return false;
    if (normalizedX < 0 || normalizedX >= fixed_slot_sell_logic::kCoordinateScale ||
        normalizedY < 0 || normalizedY >= fixed_slot_sell_logic::kCoordinateScale) {
        SetText(detail, cap, L"Tọa độ chuẩn hóa ô trang bị nằm ngoài client");
        return false;
    }
    std::int32_t width = 0, height = 0;
    if (!StaticScalar(g_ui.unityScreen, "get_width", width, detail, cap) || width <= 0 ||
        !StaticScalar(g_ui.unityScreen, "get_height", height, detail, cap) || height <= 0) {
        SetText(detail, cap, L"Không đọc được Unity Screen.width/height");
        return false;
    }
    contains = ExactMethod(
        g_ui.rectTransformUtility, "RectangleContainsScreenPoint", 3, true,
        "UnityEngine.RectTransform", "UnityEngine.Vector2", "UnityEngine.Camera");
    if (!contains) {
        SetText(detail, cap, L"Không resolve đúng RectTransformUtility.RectangleContainsScreenPoint(3)");
        return false;
    }
    point.x = static_cast<float>(static_cast<double>(normalizedX) * width /
                                 fixed_slot_sell_logic::kCoordinateScale);
    const double topY = static_cast<double>(normalizedY) * height /
                        fixed_slot_sell_logic::kCoordinateScale;
    point.y = static_cast<float>(height - 1.0 - topY);
    return true;
}

bool RectContainsScreenPoint(Il2CppObject* rectTransform, const UnityVector2& screenPoint,
                             const MethodInfo* contains) {
    UnityVector2 point = screenPoint;
    Il2CppObject* camera = nullptr;
    void* args[] = {&rectTransform, &point, &camera};
    std::int64_t result = 0;
    wchar_t ignored[192]{};
    return InvokeScalarArgs(contains, nullptr, args, result, ignored, _countof(ignored)) && result != 0;
}

int UiDepth(Il2CppObject* object) {
    int depth = 0;
    std::vector<Il2CppObject*> seen;
    while (object && depth < 64) {
        if (std::find(seen.begin(), seen.end(), object) != seen.end()) break;
        seen.push_back(object);
        Il2CppClass* klass = g_api.object_get_class(object);
        Il2CppObject* parent = nullptr;
        if (!klass || !ObjectGetter(object, klass, "get_Parent", parent) || !parent) break;
        object = parent;
        ++depth;
    }
    return depth;
}

bool EnumerateControls(std::vector<UiControl>& controls, wchar_t* detail, std::size_t cap);

[[maybe_unused]] bool FindControlAtNormalizedPoint(int normalizedX, int normalizedY, UiControl& selected,
                                  wchar_t* detail, std::size_t cap) {
    if (!EnsureUiGeometry(detail, cap)) return false;
    UnityVector2 screenPoint{};
    const MethodInfo* contains = nullptr;
    if (!BuildUnityScreenPoint(normalizedX, normalizedY, screenPoint, contains, detail, cap)) return false;
    std::vector<UiControl> controls;
    if (!EnumerateControls(controls, detail, cap)) return false;
    struct Hit {
        UiControl control{};
        float area = 0.0f;
        int depth = 0;
    };
    std::vector<Hit> hits;
    int geometryCount = 0;
    for (UiControl& control : controls) {
        if (control.kind == UiKind::Toggle) continue;
        if (control.kind == UiKind::Rect && control.labels.handler.empty()) continue;
        Il2CppObject* rectTransform = nullptr;
        if (!ResolveRectTransform(control.object, control.klass, rectTransform)) continue;
        ++geometryCount;
        if (!RectContainsScreenPoint(rectTransform, screenPoint, contains)) continue;
        float area = 3.4e38f;
        (void)ReadRectArea(rectTransform, area);
        const int depth = UiDepth(control.object);
        hits.push_back({std::move(control), area, depth});
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) {
        if (std::fabs(a.area - b.area) > 0.5f) return a.area < b.area;
        if (a.depth != b.depth) return a.depth > b.depth;
        return reinterpret_cast<std::uintptr_t>(a.control.object) <
               reinterpret_cast<std::uintptr_t>(b.control.object);
    });
    if (hits.empty()) {
        SetText(detail, cap, L"Không có UIButton/UIRect callback tại tọa độ đã gán • geometry=");
        AppendInt(detail, cap, geometryCount);
        return false;
    }
    if (hits.size() > 1 && std::fabs(hits[0].area - hits[1].area) <= 0.5f &&
        hits[0].depth == hits[1].depth && hits[0].control.object != hits[1].control.object) {
        SetText(detail, cap, L"Hai callback UI đồng hạng tại tọa độ item; fail-closed");
        return false;
    }
    selected = std::move(hits.front().control);
    return true;
}

bool ClassifyControl(Il2CppClass* klass, UiKind& kind) {
    for (const auto& cached : g_ui.kindCache) {
        if (cached.first == klass) { kind = cached.second; return true; }
    }
    bool matched = false;
    if (g_ui.button && g_api.class_is_assignable_from(g_ui.button, klass)) {
        kind = UiKind::Button; matched = true;
    } else if (g_ui.toggle && g_api.class_is_assignable_from(g_ui.toggle, klass)) {
        kind = UiKind::Toggle; matched = true;
    } else if (g_ui.rect && g_api.class_is_assignable_from(g_ui.rect, klass)) {
        kind = UiKind::Rect; matched = true;
    }
    if (matched) g_ui.kindCache.push_back({klass, kind});
    return matched;
}

bool ReadBasicControl(Il2CppObject* object, Il2CppClass* klass, UiKind kind,
                      UiControl& output) {
    wchar_t ignored[128]{};
    std::int32_t active = 0;
    if (!ScalarGetter(klass, "get_ActiveInHierarchy", object, active, ignored, _countof(ignored)) || !active)
        return false;
    if (kind != UiKind::Rect) {
        std::int32_t interactable = 0;
        if (!ScalarGetter(klass, "get_Interactable", object, interactable, ignored, _countof(ignored)) ||
            !interactable) return false;
    }
    output = {};
    output.object = object;
    output.klass = klass;
    output.kind = kind;
    (void)ReadUiString(object, klass, "Name", output.labels.name);
    if (kind != UiKind::Rect) (void)ReadUiString(object, klass, "Text", output.labels.text);
    if (kind == UiKind::Rect)
        (void)ReadUiString(object, klass, "PointerClickHandler", output.labels.handler);
    return true;
}

bool EnumerateControls(std::vector<UiControl>& controls, wchar_t* detail, std::size_t cap) {
    controls.clear();
    if (!EnsureUiDiscovery(detail, cap)) return false;
    Il2CppObject* dictionary = nullptr;
    g_api.field_static_get_value(g_ui.instances, &dictionary);
    Il2CppObject* entries = nullptr;
    std::int32_t count = 0;
    std::uintptr_t capacity = 0;
    if (!dictionary || !ReadLocal(dictionary, 0x18, entries) || !entries ||
        !ReadLocal(dictionary, 0x20, count) || count < 0 || count > 32768 ||
        !ReadLocal(entries, 0x18, capacity) || capacity > 32768) {
        SetText(detail, cap, L"UIObject.instances dictionary không hợp lệ");
        return false;
    }

    // Scan the complete entries-array capacity, not merely [0,count). A Dictionary can
    // contain deleted buckets; limiting the scan to Count silently misses live controls.
    for (std::uintptr_t i = 0; i < capacity; ++i) {
        Il2CppObject* object = nullptr;
        const std::size_t entry = 0x20 + static_cast<std::size_t>(i) * 0x18;
        if (!ReadLocal(entries, entry + 0x10, object) || !object) continue;
        std::uint8_t disposed = 1;
        Il2CppClass* klass = nullptr;
        if (!ReadLocal(object, 0x60, disposed) || disposed != 0 || !ReadLocal(object, 0, klass) || !klass)
            continue;
        UiKind kind{};
        if (!ClassifyControl(klass, kind)) continue;
        UiControl control{};
        if (ReadBasicControl(object, klass, kind, control)) controls.push_back(std::move(control));
    }
    return true;
}


// UI DIRECT uses a more tolerant enumerator than legacy coordinate/role scanning.
// Do not trust the historical object+0x60 "disposed" byte, and do not treat a temporarily
// unreadable ActiveInHierarchy/Interactable getter as proof that the control is dead.
// This is intentionally isolated so unrelated UI automation keeps its existing behavior.
bool EnumerateUiDirectControls(std::vector<UiControl>& controls, wchar_t* detail, std::size_t cap) {
    controls.clear();
    if (!EnsureUiDiscovery(detail, cap)) return false;
    Il2CppObject* dictionary = nullptr;
    g_api.field_static_get_value(g_ui.instances, &dictionary);
    Il2CppObject* entries = nullptr;
    std::int32_t count = 0;
    std::uintptr_t capacity = 0;
    if (!dictionary || !ReadLocal(dictionary, 0x18, entries) || !entries ||
        !ReadLocal(dictionary, 0x20, count) || count < 0 || count > 32768 ||
        !ReadLocal(entries, 0x18, capacity) || capacity > 32768) {
        SetText(detail, cap, L"UIObject.instances dictionary không hợp lệ");
        return false;
    }

    for (std::uintptr_t i = 0; i < capacity; ++i) {
        Il2CppObject* object = nullptr;
        const std::size_t entry = 0x20 + static_cast<std::size_t>(i) * 0x18;
        if (!ReadLocal(entries, entry + 0x10, object) || !object) continue;
        Il2CppClass* klass = nullptr;
        if (!ReadLocal(object, 0, klass) || !klass) continue;
        UiKind kind{};
        if (!ClassifyControl(klass, kind)) continue;

        wchar_t ignored[128]{};
        if (FindMethod(klass, "get_ActiveInHierarchy", 0)) {
            std::int32_t active = 1;
            if (ScalarGetter(klass, "get_ActiveInHierarchy", object, active, ignored, _countof(ignored)) && !active)
                continue;
        }
        if (kind != UiKind::Rect && FindMethod(klass, "get_Interactable", 0)) {
            std::int32_t interactable = 1;
            if (ScalarGetter(klass, "get_Interactable", object, interactable, ignored, _countof(ignored)) && !interactable)
                continue;
        }

        UiControl control{};
        control.object = object;
        control.klass = klass;
        control.kind = kind;
        (void)ReadUiString(object, klass, "Name", control.labels.name);
        if (kind != UiKind::Rect) {
            (void)ReadUiString(object, klass, "Text", control.labels.text);
            // Proven Test-UI-DIRECT path: live close buttons may expose a generic
            // Name/Text while their semantic identity lives in ClickHandler.
            if (!ReadUiString(object, klass, "ClickHandler", control.labels.handler))
                (void)ReadUiString(object, klass, "PointerClickHandler", control.labels.handler);
        } else {
            (void)ReadUiString(object, klass, "PointerClickHandler", control.labels.handler);
        }
        (void)ReadUiString(object, klass, "Tag", control.tag);
        controls.push_back(std::move(control));
    }
    return true;
}

void AppendLabel(std::wstring& target, const std::wstring& value) {
    if (value.empty()) return;
    if (target.find(value) != std::wstring::npos) return;
    if (!target.empty()) target += L"/";
    target += value;
}

void CollectDescendantLabels(UiControl& control) {
    control.labels.descendants.clear();
    std::vector<Il2CppObject*> pending{control.object};
    std::vector<Il2CppObject*> visited;
    while (!pending.empty() && visited.size() < 96) {
        Il2CppObject* current = pending.back();
        pending.pop_back();
        if (!current || std::find(visited.begin(), visited.end(), current) != visited.end()) continue;
        visited.push_back(current);

        Il2CppClass* klass = nullptr;
        if (!ReadLocal(current, 0, klass) || !klass) continue;
        if (current != control.object) {
            std::wstring name;
            if (ReadUiString(current, klass, "Name", name))
                AppendLabel(control.labels.descendants, name);
            // Do not require the child to be UIButton/UIToggle/UIRect. The
            // visible caption in the proven probe is often a plain UIText/
            // label node whose parent owns HandleClickEvent().
            std::wstring childText;
            if (ReadUiString(current, klass, "Text", childText))
                AppendLabel(control.labels.descendants, childText);
        }

        Il2CppObject* childrenArray = nullptr;
        if (!ObjectGetter(current, klass, "get_CoreChildren", childrenArray) || !childrenArray)
            (void)ObjectGetter(current, klass, "get_Children", childrenArray);
        if (!childrenArray) continue;
        std::vector<Il2CppObject*> children;
        if (!ReadManagedPointerArray(childrenArray, children, 128)) continue;
        for (Il2CppObject* child : children) pending.push_back(child);
    }
}

bool ReadAncestors(UiControl& control);

bool EnumerateActiveUiObjects(std::vector<UiControl>& objects, wchar_t* detail, std::size_t cap) {
    objects.clear();
    if (!EnsureUiDiscovery(detail, cap)) return false;
    Il2CppObject* dictionary = nullptr;
    g_api.field_static_get_value(g_ui.instances, &dictionary);
    Il2CppObject* entries = nullptr;
    std::int32_t count = 0;
    std::uintptr_t capacity = 0;
    if (!dictionary || !ReadLocal(dictionary, 0x18, entries) || !entries ||
        !ReadLocal(dictionary, 0x20, count) || count < 0 || count > 32768 ||
        !ReadLocal(entries, 0x18, capacity) || capacity > 32768) {
        SetText(detail, cap, L"UIObject.instances dictionary không hợp lệ");
        return false;
    }
    for (std::uintptr_t i = 0; i < capacity; ++i) {
        Il2CppObject* object = nullptr;
        const std::size_t entry = 0x20 + static_cast<std::size_t>(i) * 0x18;
        if (!ReadLocal(entries, entry + 0x10, object) || !object) continue;
        Il2CppClass* klass = nullptr;
        // Match the proven v0.1.8 probe: do not assume that offset 0x60 is a
        // disposed byte on every client build. ActiveInHierarchy is the runtime
        // validity gate for this semantic-only scan.
        if (!ReadLocal(object, 0, klass) || !klass) continue;
        const MethodInfo* activeGetter = FindMethod(klass, "get_ActiveInHierarchy", 0);
        if (activeGetter) {
            std::int32_t active = 0; wchar_t ignored[128]{};
            // v0.1.8 treats ActiveInHierarchy as an optional validity filter:
            // unreadable is not the same as inactive. The old production code
            // discarded the whole row when this getter was unavailable or
            // temporarily threw while the NPC dialog was being built.
            if (ScalarGetter(klass, "get_ActiveInHierarchy", object, active, ignored, _countof(ignored)) && !active)
                continue;
        }
        UiControl row{}; row.object = object; row.klass = klass; row.kind = UiKind::Button;
        (void)ReadUiString(object, klass, "Name", row.labels.name);
        (void)ReadUiString(object, klass, "Text", row.labels.text);
        (void)ReadUiString(object, klass, "Tag", row.tag);
        (void)ReadAncestors(row);
        // Probe v0.1.8 falls back to descendant text for clickable controls. Keep the
        // same behavior here so a popup whose visible "Xác nhận" lives on a child label
        // is still eligible even when the parent button itself has empty get_Text().
        CollectDescendantLabels(row);
        objects.push_back(std::move(row));
    }
    return true;
}

bool ReadAncestors(UiControl& control) {
    control.labels.ancestors.clear();
    Il2CppObject* parentArray = nullptr;
    if (ObjectGetter(control.object, control.klass, "get_CoreParents", parentArray) && parentArray) {
        std::vector<Il2CppObject*> parents;
        if (ReadManagedPointerArray(parentArray, parents, 64)) {
            for (Il2CppObject* parent : parents) {
                Il2CppClass* klass = nullptr;
                if (!ReadLocal(parent, 0, klass) || !klass) continue;
                std::wstring name;
                if (ReadUiString(parent, klass, "Name", name)) AppendLabel(control.labels.ancestors, name);
            }
            if (!control.labels.ancestors.empty()) return true;
        }
    }

    Il2CppObject* current = control.object;
    std::vector<Il2CppObject*> seen;
    for (int depth = 0; depth < 12 && current; ++depth) {
        if (std::find(seen.begin(), seen.end(), current) != seen.end()) break;
        seen.push_back(current);
        Il2CppClass* klass = nullptr;
        if (!ReadLocal(current, 0, klass) || !klass) break;
        Il2CppObject* parent = nullptr;
        if (!ObjectGetter(current, klass, "get_Parent", parent) || !parent) break;
        Il2CppClass* parentClass = nullptr;
        if (!ReadLocal(parent, 0, parentClass) || !parentClass) break;
        std::wstring name;
        if (ReadUiString(parent, parentClass, "Name", name)) AppendLabel(control.labels.ancestors, name);
        current = parent;
    }
    return !control.labels.ancestors.empty();
}

bool FindRoleControl(Role role, UiControl& selected, wchar_t* detail, std::size_t cap) {
    std::vector<UiControl> controls;
    if (!EnumerateControls(controls, detail, cap)) return false;
    struct Candidate { std::size_t index = 0; int score = 0; };
    std::vector<Candidate> candidates;

    auto scoreControls = [&](bool includeDescendants) {
        candidates.clear();
        for (std::size_t i = 0; i < controls.size(); ++i) {
            UiControl& control = controls[i];
            if (role == Role::ConfirmMap || role == Role::CloseTradeOrBag)
                (void)ReadAncestors(control);
            if (includeDescendants) CollectDescendantLabels(control);
            const int score = background_ui_logic::Score(control.labels, role);
            if (score > 0) candidates.push_back({i, score});
        }
    };

    // Most controls expose Name/Text directly. Only traverse child trees if that cheap,
    // precise pass finds nothing; some Lua layouts put the visible label on a child control.
    scoreControls(false);
    if (candidates.empty()) scoreControls(true);
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) {
        if (a.score != b.score) return a.score > b.score;
        return reinterpret_cast<std::uintptr_t>(controls[a.index].object) <
               reinterpret_cast<std::uintptr_t>(controls[b.index].object);
    });
    if (candidates.empty()) {
        SetText(detail, cap, L"Không tìm thấy control nội bộ đúng vai trò");
        Append(detail, cap, L" • types B/T/R=");
        AppendInt(detail, cap, g_ui.button ? 1 : 0);
        Append(detail, cap, L"/"); AppendInt(detail, cap, g_ui.toggle ? 1 : 0);
        Append(detail, cap, L"/"); AppendInt(detail, cap, g_ui.rect ? 1 : 0);
        return false;
    }
    if (candidates.size() > 1 && candidates[0].score == candidates[1].score &&
        controls[candidates[0].index].object != controls[candidates[1].index].object) {
        SetText(detail, cap, L"Có nhiều control cùng điểm; fail-closed, không gọi mù");
        return false;
    }
    selected = std::move(controls[candidates[0].index]);
    return true;
}

bool ExecutorInstance(Il2CppObject*& instance, const MethodInfo*& execute,
                      wchar_t* detail, std::size_t cap) {
    instance = nullptr;
    execute = nullptr;
    const MethodInfo* getInstance = ExactMethod(g_ui.executor, "get_Instance", 0, true);
    if (!getInstance || !InvokeObject(getInstance, nullptr, instance, detail, cap) || !instance) {
        SetText(detail, cap, L"MonoBehaviourExecutor.Instance chưa sẵn sàng");
        return false;
    }
    execute = ExactMethod(g_ui.executor, "ExecuteScriptFunction", 3, false);
    if (!execute) { SetText(detail, cap, L"Thiếu ExecuteScriptFunction(UIObject,string,object[])"); return false; }
    return true;
}

bool ExecuteLuaOnObject(Il2CppObject* uiObject, Il2CppString* function,
                        Il2CppObject* argsArray, wchar_t* detail, std::size_t cap) {
    Il2CppObject* executor = nullptr;
    const MethodInfo* execute = nullptr;
    if (!uiObject || !function || !argsArray || !ExecutorInstance(executor, execute, detail, cap)) return false;
    void* args[] = {&uiObject, &function, &argsArray};
    return InvokeVoid(execute, executor, args, detail, cap);
}

bool InvokeControl(UiControl& control, wchar_t* detail, std::size_t cap) {
    if (control.kind == UiKind::Button) {
        const MethodInfo* click = ExactMethod(control.klass, "HandleClickEvent", 0, false);
        if (!click) { SetText(detail, cap, L"UIButton thiếu HandleClickEvent()"); return false; }
        return InvokeVoid(click, control.object, nullptr, detail, cap);
    }
    if (control.kind == UiKind::Toggle) {
        std::int32_t selected = 0;
        wchar_t ignored[128]{};
        if (ScalarGetter(control.klass, "get_Selected", control.object, selected, ignored, _countof(ignored)) && selected)
            return true;
        std::uint8_t yes = 1;
        void* args[] = {&yes};
        const MethodInfo* setSelected = ExactMethod(control.klass, "set_Selected", 1, false, "System.Boolean");
        if (setSelected && InvokeVoid(setSelected, control.object, args, detail, cap)) return true;
        const MethodInfo* selectEvent = ExactMethod(control.klass, "HandleSelectEvent", 1, false, "System.Boolean");
        if (!selectEvent) { SetText(detail, cap, L"UIToggle thiếu callback chọn"); return false; }
        return InvokeVoid(selectEvent, control.object, args, detail, cap);
    }

    if (!EnsureUiLua(false, detail, cap)) return false;
    Il2CppObject* handlerObject = nullptr;
    if (!ObjectGetter(control.object, control.klass, "get_PointerClickHandler", handlerObject) || !handlerObject) {
        SetText(detail, cap, L"UIRect không có PointerClickHandler");
        return false;
    }
    Il2CppObject* argsArray = g_api.array_new(g_ui.systemObject, 3);
    if (!argsArray || !WriteLocal(argsArray, 0x20, control.object)) {
        SetText(detail, cap, L"Không tạo được object[3] cho UIRect callback");
        return false;
    }
    return ExecuteLuaOnObject(control.object, reinterpret_cast<Il2CppString*>(handlerObject),
                              argsArray, detail, cap);
}

bool FindUiByName(const char* uiName, Il2CppObject*& ui, wchar_t* detail, std::size_t cap) {
    ui = nullptr;
    Il2CppString* managedName = g_api.string_new(uiName);
    if (!managedName) { SetText(detail, cap, L"Không tạo được tên UI managed"); return false; }
    const char* methods[] = {"FindUI", "MainFindUI"};
    for (const char* methodName : methods) {
        const MethodInfo* method = ExactMethod(g_ui.guiApi, methodName, 1, true, "System.String");
        if (!method) continue;
        void* args[] = {&managedName};
        Il2CppObject* found = nullptr;
        if (InvokeObjectArgs(method, nullptr, args, found, detail, cap) && found) {
            ui = found;
            return true;
        }
    }
    SetText(detail, cap, L"Không tìm thấy Lua UI theo tên");
    return false;
}

bool InvokeLuaAction(const char* uiName, const char* functionName,
                     wchar_t* detail, std::size_t cap) {
    if (!EnsureUiLua(true, detail, cap)) return false;
    Il2CppObject* ui = nullptr;
    if (!FindUiByName(uiName, ui, detail, cap)) return false;
    Il2CppString* function = g_api.string_new(functionName);
    Il2CppObject* emptyArgs = g_api.array_new(g_ui.systemObject, 0);
    if (!function || !emptyArgs) { SetText(detail, cap, L"Không tạo được Lua action arguments"); return false; }
    if (!ExecuteLuaOnObject(ui, function, emptyArgs, detail, cap)) return false;
    SetText(detail, cap, L"Đã gọi Lua action nội bộ ");
    Append(detail, cap, functionName[0] == 'A' ? L"TopIcon" : L"UI");
    return true;
}



// 10.1 AUTO LOOT ----------------------------------------------------------
// Production-only port of the V99 CP19 ItemPack semantic action. No SCAN-RUONG
// test UI is carried over. Every mutation re-resolves the nearest live ItemPack.
enum class LootResolveResult { Error, NoCandidate, Candidate };

static void LootAppendAscii(wchar_t* out, std::size_t cap, const char* text) {
    if (!text) return;
    wchar_t wide[160]{};
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, static_cast<int>(_countof(wide)));
    Append(out, cap, wide);
}

static void LootAppendMethodSignature(const MethodInfo* method, wchar_t* detail, std::size_t cap) {
    if (!method) return;
    LootAppendAscii(detail, cap, g_api.method_get_name ? g_api.method_get_name(method) : "?");
    Append(detail, cap, L"(");
    const auto argc = g_api.method_get_param_count(method);
    for (std::uint32_t i = 0; i < argc; ++i) {
        if (i) Append(detail, cap, L",");
        const Il2CppType* t = g_api.method_get_param(method, i);
        char* n = t ? g_api.type_get_name(t) : nullptr;
        if (n) { LootAppendAscii(detail, cap, n); g_api.free_fn(n); }
        else Append(detail, cap, L"?");
    }
    Append(detail, cap, L")");
}

static bool LootReadObjectId(Il2CppObject* object, std::int64_t& id) {
    id = 0;
    if (!object) return false;
    Il2CppClass* klass = g_api.object_get_class(object);
    if (!klass) return false;
    const char* idNames[] = {"get_ID", "get_ItemPackID", "get_RoleID", "get_ResID"};
    for (const char* name : idNames) {
        std::int64_t value = 0; wchar_t ignored[96]{};
        if (ScalarGetter64(klass, name, ManagedThis(object), value, ignored, _countof(ignored)) && value > 0) {
            id = value;
            return true;
        }
    }
    return false;
}

static LootResolveResult LootGetNearestItemPack(const Classes& c, Il2CppObject*& object,
                                                std::int64_t& scalarId,
                                                wchar_t* detail, std::size_t cap) {
    object = nullptr;
    scalarId = 0;
    const MethodInfo* method = ExactMethod(c.gameApi, "GetNearestItemPack", 0, true);
    if (!method) {
        SetText(detail, cap, L"GetNearestItemPack(0 arg) chưa khớp. Runtime overload: ");
        if (g_api.class_get_methods && g_api.method_get_name) {
            void* it = nullptr; int shown = 0;
            while (const MethodInfo* current = g_api.class_get_methods(c.gameApi, &it)) {
                const char* name = g_api.method_get_name(current);
                if (name && Eq(name, "GetNearestItemPack")) {
                    if (shown++) Append(detail, cap, L" | ");
                    LootAppendMethodSignature(current, detail, cap);
                    if (shown >= 4) break;
                }
            }
        }
        return LootResolveResult::Error;
    }

    const Il2CppType* returnType = g_api.method_get_return_type(method);
    char* returnName = returnType ? g_api.type_get_name(returnType) : nullptr;
    const bool scalar = returnName &&
        (Eq(returnName, "System.Int32") || Eq(returnName, "System.Int64") || Eq(returnName, "System.UInt32"));
    if (returnName) g_api.free_fn(returnName);

    if (scalar) {
        if (!InvokeScalar(method, nullptr, scalarId, detail, cap)) return LootResolveResult::Error;
        return scalarId > 0 ? LootResolveResult::Candidate : LootResolveResult::NoCandidate;
    }
    if (!InvokeObject(method, nullptr, object, detail, cap)) return LootResolveResult::Error;
    return object ? LootResolveResult::Candidate : LootResolveResult::NoCandidate;
}

bool ProbeNearbyLoot(Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap)) return false;
    int ready = 0, waiting = 0;
    if (!Transition(c, ready, waiting, detail, cap) || !ready || waiting) {
        SetText(detail, cap, L"LOOT PROBE: map chưa sẵn sàng");
        return false;
    }

    Il2CppObject* object = nullptr;
    std::int64_t scalarId = 0;
    const LootResolveResult resolved = LootGetNearestItemPack(c, object, scalarId, detail, cap);
    if (resolved == LootResolveResult::Error) return false;
    if (resolved == LootResolveResult::NoCandidate) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::NoCandidate);
        SetText(detail, cap, L"SCAN ITEMPACK: không có ItemPack gần nhất");
        return true;
    }

    std::int64_t id = scalarId;
    int x = 0, y = 0;
    if (object) {
        Il2CppClass* klass = g_api.object_get_class(object);
        if (!klass || !LootReadObjectId(object, id)) {
            SetText(detail, cap, L"SCAN ITEMPACK: object có nhưng không đọc được ID");
            return false;
        }
        wchar_t ignored[96]{};
        (void)ScalarGetter(klass, "get_PosX", ManagedThis(object), x, ignored, _countof(ignored));
        ignored[0] = 0;
        (void)ScalarGetter(klass, "get_PosY", ManagedThis(object), y, ignored, _countof(ignored));
        SetText(detail, cap, L"SCAN ITEMPACK: class=");
        if (g_api.class_get_name) LootAppendAscii(detail, cap, g_api.class_get_name(klass));
        else Append(detail, cap, L"?");
        Append(detail, cap, L" id="); AppendInt64(detail, cap, id);
        Append(detail, cap, L" pos="); AppendInt(detail, cap, x); Append(detail, cap, L","); AppendInt(detail, cap, y);
    } else {
        SetText(detail, cap, L"SCAN ITEMPACK: nearest id="); AppendInt64(detail, cap, id);
        Append(detail, cap, L" • client trả scalar ID");
    }
    response.resultCode = static_cast<std::int32_t>(ActionResult::StageReady);
    response.value64_0 = id;
    response.value0 = x;
    response.value1 = y;
    return true;
}

bool PickNearestLoot(Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;

    Il2CppObject* object = nullptr;
    std::int64_t scalarId = 0;
    const LootResolveResult resolved = LootGetNearestItemPack(c, object, scalarId, detail, cap);
    if (resolved == LootResolveResult::Error) return false;
    if (resolved == LootResolveResult::NoCandidate) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::NoCandidate);
        SetText(detail, cap, L"AUTO NHẶT: không có ItemPack gần nhất");
        return true;
    }

    std::int64_t id = scalarId;
    if (object && !LootReadObjectId(object, id)) {
        SetText(detail, cap, L"AUTO NHẶT: ItemPack object không đọc được ID");
        return false;
    }
    if (id <= 0 || id > INT32_MAX) {
        SetText(detail, cap, L"AUTO NHẶT: ItemPack ID không hợp lệ");
        return false;
    }

    const MethodInfo* pick = FindMethod(c.gameApi, "PickUpItemFromItemPack", 3);
    if (!pick || !StaticMethod(pick) ||
        !ParamType(pick, 0, "System.Int32") || !ParamType(pick, 1, "System.Int32")) {
        SetText(detail, cap, L"AUTO NHẶT: signature PickUpItemFromItemPack chưa khớp Int32,Int32,...");
        return false;
    }

    std::int32_t pack = static_cast<std::int32_t>(id);
    std::int32_t slot = -1;
    std::int32_t usingAuto = 1;
    std::uint8_t usingAutoBool = 1;
    void* args[3] = {&pack, &slot, nullptr};
    if (ParamType(pick, 2, "System.Int32")) args[2] = &usingAuto;
    else if (ParamType(pick, 2, "System.Boolean")) args[2] = &usingAutoBool;
    else {
        SetText(detail, cap, L"AUTO NHẶT: tham số UsingAuto không phải Int32/Boolean");
        return false;
    }

    if (!InvokeVoid(pick, nullptr, args, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value64_0 = id;
    SetText(detail, cap, L"AUTO NHẶT: PickUpItemFromItemPack pick-all id=");
    AppendInt64(detail, cap, id);
    return true;
}
// END 10.1 AUTO LOOT ------------------------------------------------------

bool SelectTargetByRoleID(std::int32_t targetRoleID, bool allowSelect, Response& response,
                          wchar_t* detail, std::size_t cap) {
    if (targetRoleID <= 0) { SetText(detail, cap, L"SelectTarget thiếu RoleID MAIN"); return false; }

    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;

    // v1.5.3 lag guard: verify first. v1.5.2 always invoked Game.SelectTarget on every
    // retry (~each 250ms controller tick), which can repeatedly trigger the client's
    // target-change/UI work. Once MAIN is already selected, return without another mutation.
    const MethodInfo* getSelected = ExactMethod(c.gameApi, "get_SelectedTarget", 0, true);
    if (!getSelected) { SetText(detail, cap, L"Không resolve Game.get_SelectedTarget"); return false; }
    Il2CppObject* target = nullptr;
    wchar_t verifyDetail[160]{};
    if (InvokeObject(getSelected, nullptr, target, verifyDetail, _countof(verifyDetail)) && target) {
        Il2CppClass* targetClass = g_api.object_get_class(target);
        std::int32_t selectedRole = 0;
        wchar_t roleDetail[160]{};
        if (targetClass && ScalarGetter(targetClass, "get_RoleID", target, selectedRole, roleDetail, _countof(roleDetail)) &&
            selectedRole == targetRoleID) {
            response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
            response.value0 = selectedRole;
            SetText(detail, cap, L"TARGET MAIN PASS • đã đúng RoleID, không gọi SelectTarget lặp");
            return true;
        }
    }

    if (!allowSelect) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::StageReady);
        response.value1 = 0;
        SetText(detail, cap, L"TARGET MAIN verify read-only • chưa khớp RoleID");
        return true;
    }

    const MethodInfo* selectTarget = ExactMethod(c.gameApi, "SelectTarget", 1, true, "System.Int32");
    if (!selectTarget) { SetText(detail, cap, L"Không resolve Game.SelectTarget(Int32)"); return false; }
    std::int32_t role = targetRoleID;
    void* selectArgs[] = {&role};
    if (!InvokeVoid(selectTarget, nullptr, selectArgs, detail, cap)) return false;

    // Do not immediately re-read SelectedTarget in the same game-thread callback/frame.
    // Controller backs off before the next verify-first request.
    response.resultCode = static_cast<std::int32_t>(ActionResult::StageReady);
    response.value0 = targetRoleID;
    response.value1 = 1; // mutation really issued; controller starts the 2s re-select backoff here.
    SetText(detail, cap, L"Đã SelectTarget MAIN một lần • chờ verify ở tick sau");
    return true;
}


bool SellBagItem(std::int64_t instanceID, std::int32_t expectedItemID, Response& response,
                 wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    Il2CppObject* itemObject = nullptr; BagItemSnapshot item{};
    if (!FindFreshBagItem(c, instanceID, expectedItemID, itemObject, item, detail, cap)) return false;
    if (!item.sellable) { SetText(detail, cap, L"Item hiện tại IsItemSellable=false; chặn bán"); return false; }
    if (item.itemID >= 40000000 && item.itemID < 50000000) { SetText(detail, cap, L"Item quest-family; chặn bán"); return false; }
    if (!EnsureUiLua(true, detail, cap)) return false;
    Il2CppObject* sellTab = nullptr;
    if (!FindUiByName("NPCShop_SellItemTab", sellTab, detail, cap) || !sellTab) {
        SetText(detail, cap, L"Chưa có NPCShop_SellItemTab hiện hành; không bán mù"); return false;
    }
    Il2CppString* function = g_api.string_new("RequestSellItem");
    Il2CppObject* argsArray = g_api.array_new(g_ui.systemObject, 1);
    if (!function || !argsArray || !WriteLocal(argsArray, 0x20, itemObject)) {
        SetText(detail, cap, L"Không tạo được RequestSellItem(item) args"); return false;
    }
    if (!ExecuteLuaOnObject(sellTab, function, argsArray, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value64_0 = item.instanceID; response.value0 = item.itemID;
    SetText(detail, cap, L"Đã gọi NPCShop_SellItemTab.RequestSellItem instance="); AppendInt64(detail, cap, item.instanceID);
    return true;
}

bool InvokeRole(Role role, Response& response, wchar_t* detail, std::size_t cap) {
    UiControl control{};
    if (!FindRoleControl(role, control, detail, cap) || !InvokeControl(control, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, L"Đã gọi callback UI nội bộ • ");
    Append(detail, cap, control.labels.text.empty() ? control.labels.name.c_str() : control.labels.text.c_str());
    return true;
}

bool ConfirmMap(Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    return InvokeRole(Role::ConfirmMap, response, detail, cap);
}

bool IsExactSemanticToken(const std::wstring& raw, TravelSemantic semantic) {
    const std::wstring key = background_ui_logic::Key(raw);
    switch (semantic) {
        case TravelSemantic::KunLunSon: return key == L"conlonson";
        case TravelSemantic::TinhTucHai: return key == L"tinhtuc" || key == L"tinhtuchai";
        case TravelSemantic::DenCacMonPhai: return key == L"dencacmonphai";
        case TravelSemantic::Trade: return key == L"giaodich";
        case TravelSemantic::InviteParty: return key == L"moivaonhom";
        case TravelSemantic::ThienSon: return key == L"thienson";
        case TravelSemantic::SellPopup: return key == L"ban";
        case TravelSemantic::DiscardPopup: return key == L"vut" || key == L"vutbo" || key == L"discard";
        case TravelSemantic::DiscardConfirm: return key == L"xacnhan";
        case TravelSemantic::PutUpPopup: return key == L"datlen";
        case TravelSemantic::NamHai: return key == L"namhai";
        case TravelSemantic::MieuCuong: return key == L"mieucuong";
        case TravelSemantic::HoangLongPhu: return key == L"hoanglongphu";
        case TravelSemantic::ThachLam: return key == L"thachlam";
        case TravelSemantic::DaiLy: return key == L"daili";
        default: return false;
    }
}

bool SemanticTokenInDescendants(const std::wstring& descendants, TravelSemantic semantic) {
    std::size_t start = 0;
    while (start <= descendants.size()) {
        const std::size_t slash = descendants.find(L'/', start);
        const std::wstring token = descendants.substr(start, slash == std::wstring::npos ? std::wstring::npos : slash - start);
        if (IsExactSemanticToken(token, semantic)) return true;
        if (slash == std::wstring::npos) break;
        start = slash + 1;
    }
    return false;
}

bool ClickTravelSemantic(TravelSemantic semantic, bool probeOnly, Response& response, wchar_t* detail, std::size_t cap) {
    // v3.0 keeps semantic callbacks only for the working enter routes. The
    // Côn Lôn exit (Đại Lý + Xác nhận) is intentionally absent and is handled
    // entirely by three configured TryClickUI points in the controller.
    if (!EnsureUiDiscovery(detail, cap)) return false;

    std::vector<UiControl> objects;
    if (!EnumerateActiveUiObjects(objects, detail, cap)) return false;

    std::vector<std::size_t> candidates;
    std::vector<std::size_t> dialogCandidates;
    std::vector<std::size_t> preview;
    int clickableCount = 0;
    bool hasDialogContext = false;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        UiControl& row = objects[i];
        if (!row.object || !row.klass) continue;
        const MethodInfo* handleClick = ExactMethod(row.klass, "HandleClickEvent", 0, false);
        if (!handleClick) continue;

        const MethodInfo* interactableGetter = FindMethod(row.klass, "get_Interactable", 0);
        if (interactableGetter) {
            std::int32_t interactable = 0; wchar_t ignored[128]{};
            if (ScalarGetter(row.klass, "get_Interactable", row.object, interactable, ignored, _countof(ignored)) && !interactable)
                continue;
        }
        ++clickableCount;
        const std::wstring context = background_ui_logic::Key(row.labels.ancestors + L"/" + row.labels.name);
        const bool inDialog = background_ui_logic::Has(
            context, {L"gamedialog", L"buttonlist", L"dialog", L"npc"});
        if (inDialog) hasDialogContext = true;
        if (preview.size() < 4 && inDialog) preview.push_back(i);

        const bool semanticMatch = IsExactSemanticToken(row.labels.text, semantic) ||
            IsExactSemanticToken(row.labels.name, semantic) ||
            SemanticTokenInDescendants(row.labels.descendants, semantic);
        if (semanticMatch) {
            candidates.push_back(i);
            if (inDialog) dialogCandidates.push_back(i);
        }
    }

    if (hasDialogContext && !dialogCandidates.empty()) candidates.swap(dialogCandidates);
    if (candidates.empty()) {
        SetText(detail, cap, L"SEMANTIC_SCAN NOT_FOUND • active="); AppendInt(detail, cap, static_cast<int>(objects.size()));
        Append(detail, cap, L" clickable="); AppendInt(detail, cap, clickableCount);
        for (std::size_t j = 0; j < preview.size(); ++j) {
            const UiControl& row = objects[preview[j]];
            Append(detail, cap, L" • [N="); Append(detail, cap, row.labels.name.c_str());
            Append(detail, cap, L" T="); Append(detail, cap, row.labels.text.c_str());
            Append(detail, cap, L" P="); Append(detail, cap, row.labels.ancestors.c_str()); Append(detail, cap, L"]");
        }
        return false;
    }
    if (candidates.size() != 1) {
        SetText(detail, cap, L"SEMANTIC_SCAN AMBIGUOUS • exact candidates=");
        AppendInt(detail, cap, static_cast<int>(candidates.size()));
        return false;
    }

    UiControl& selected = objects[candidates.front()];
    const MethodInfo* handleClick = ExactMethod(selected.klass, "HandleClickEvent", 0, false);
    if (!handleClick) {
        SetText(detail, cap, L"SEMANTIC_SCAN target mất HandleClickEvent trước callback");
        return false;
    }
    if (probeOnly) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::StageReady);
        SetText(detail, cap, L"SEMANTIC_SCAN PROBE PASS • N=");
        Append(detail, cap, selected.labels.name.c_str());
        Append(detail, cap, L" T="); Append(detail, cap, selected.labels.text.c_str());
        return true;
    }
    g_travelBaselineActiveControls.clear();
    g_travelBaselineActiveControls.reserve(objects.size());
    for (const UiControl& row : objects) if (row.object) g_travelBaselineActiveControls.push_back(row.object);
    if (!InvokeVoid(handleClick, ManagedThis(selected.object), nullptr, detail, cap)) {
        g_travelBaselineActiveControls.clear();
        return false;
    }
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, L"SEMANTIC_SCAN CALLBACK PASS • N=");
    Append(detail, cap, selected.labels.name.c_str());
    Append(detail, cap, L" T="); Append(detail, cap, selected.labels.text.c_str());
    Append(detail, cap, L" P="); Append(detail, cap, selected.labels.ancestors.c_str());
    return true;
}

int PositiveTravelConfirmScore(const Labels& labels) {
    const std::wstring text = background_ui_logic::Key(labels.text);
    const std::wstring name = background_ui_logic::Key(labels.name);
    const std::wstring descendants = background_ui_logic::Key(labels.descendants);
    const std::wstring all = text + name + descendants;
    if (text == L"xacnhan" || name == L"xacnhan") return 100;
    if (text == L"dongy" || name == L"dongy" || text == L"chapnhan" || name == L"chapnhan") return 95;
    if (text == L"confirm" || name == L"confirm") return 90;
    if (text == L"ok" || name == L"ok") return 85;
    if (text == L"co" || name == L"co" || text == L"yes" || name == L"yes") return 70;
    if (all.find(L"xacnhan") != std::wstring::npos) return 98;
    if (all.find(L"dongy") != std::wstring::npos || all.find(L"chapnhan") != std::wstring::npos) return 93;
    if (all.find(L"confirm") != std::wstring::npos) return 88;
    return 0;
}

bool NegativeTravelConfirm(const Labels& labels) {
    auto exactNegative = [](const std::wstring& raw) {
        const std::wstring key = background_ui_logic::Key(raw);
        return key == L"huy" || key == L"khong" || key == L"cancel" || key == L"dong" ||
               key == L"close" || key == L"boqua" || key == L"quaylai" || key == L"thoat" || key == L"no";
    };
    if (exactNegative(labels.text) || exactNegative(labels.name)) return true;
    std::size_t start = 0;
    while (start <= labels.descendants.size()) {
        const std::size_t slash = labels.descendants.find(L'/', start);
        const std::wstring token = labels.descendants.substr(start, slash == std::wstring::npos ? std::wstring::npos : slash - start);
        if (exactNegative(token)) return true;
        if (slash == std::wstring::npos) break;
        start = slash + 1;
    }
    const std::wstring structure = background_ui_logic::Key(labels.name + L"/" + labels.handler);
    return background_ui_logic::Has(structure, {L"buttonno", L"btnno", L"buttoncancel", L"btncancel", L"buttonclose", L"btnclose"});
}

bool ConfirmTravelSemantic(Response& response, wchar_t* detail, std::size_t cap) {
    // Confirmation is the second direct UI callback from the v0.1.8 probe.
    // Do not gate it on route/map readiness: the popup itself is what changes
    // that state, and SafeForAction would reject the only valid click.
    if (!EnsureUiDiscovery(detail, cap)) return false;

    // Transplanted behavior from successful probe 0.1.8: scan ALL active UIObject.instances.
    // A confirmation control is eligible by capability (HandleClickEvent), not by a pre-known
    // UIButton runtime class. Popup container names only add score; they are never mandatory.
    std::vector<UiControl> objects;
    if (!EnumerateActiveUiObjects(objects, detail, cap)) return false;
    struct Candidate { std::size_t index = 0; int score = 0; bool newlyActive = false; bool dialogContext = false; };
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < objects.size(); ++i) {
        UiControl& row = objects[i];
        if (!row.object || !row.klass) continue;
        const MethodInfo* handleClick = ExactMethod(row.klass, "HandleClickEvent", 0, false);
        if (!handleClick) continue;
        if (NegativeTravelConfirm(row.labels)) continue;

        const MethodInfo* interactableGetter = FindMethod(row.klass, "get_Interactable", 0);
        if (interactableGetter) {
            std::int32_t interactable = 0; wchar_t ignored[128]{};
            if (ScalarGetter(row.klass, "get_Interactable", row.object, interactable, ignored, _countof(ignored)) && !interactable) continue;
        }

        const int semanticScore = PositiveTravelConfirmScore(row.labels);
        if (semanticScore <= 0) continue;
        const bool newlyActive = std::find(g_travelBaselineActiveControls.begin(), g_travelBaselineActiveControls.end(), row.object) == g_travelBaselineActiveControls.end();
        const std::wstring context = background_ui_logic::Key(row.labels.ancestors + L"/" + row.labels.name);
        const bool dialogContext = background_ui_logic::Has(context, {L"messagebox", L"dialog", L"confirm", L"notice", L"prompt", L"tip", L"warning", L"alert", L"ask"});
        int score = semanticScore + (newlyActive ? 40 : 0) + (dialogContext ? 20 : 0);
        candidates.push_back({i, score, newlyActive, dialogContext});
    }
    if (candidates.empty()) {
        SetText(detail, cap, L"CONFIRM_NOT_FOUND • chưa thấy ACTIVE UIObject positive có HandleClickEvent sau callback điểm đến; giữ baseline để retry");
        return false;
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b){ return a.score > b.score; });
    if (candidates.size() > 1 && candidates[0].score == candidates[1].score &&
        objects[candidates[0].index].object != objects[candidates[1].index].object) {
        SetText(detail, cap, L"SAFETY REJECT • nhiều control Xác nhận cùng điểm semantic; không callback mù");
        return false;
    }
    Candidate chosen = candidates.front();
    UiControl& selected = objects[chosen.index];
    const MethodInfo* handleClick = ExactMethod(selected.klass, "HandleClickEvent", 0, false);
    if (!handleClick || !InvokeVoid(handleClick, ManagedThis(selected.object), nullptr, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, L"CONFIRM CALLBACK ĐÃ GỌI • generic ACTIVE UIObject + HandleClickEvent • UI-delta=");
    Append(detail, cap, chosen.newlyActive ? L"NEW" : L"REUSED");
    Append(detail, cap, L" • semantic=");
    Append(detail, cap, selected.labels.text.empty() ? selected.labels.name.c_str() : selected.labels.text.c_str());
    Append(detail, cap, L" • Tag="); Append(detail, cap, selected.tag.c_str());
    g_travelBaselineActiveControls.clear();
    return true;
}

bool Revive(Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    Il2CppObject* leader = nullptr;
    Il2CppClass* leaderClass = nullptr;
    if (!GetLeader(c, leader, leaderClass, detail, cap)) return false;
    std::int32_t dead = 0;
    if (!ScalarGetter(leaderClass, "get_IsDeath", leader, dead, detail, cap) || !dead) {
        SetText(detail, cap, L"Không gọi Đầu thai vì IsDeath=false/không đọc được");
        return false;
    }
    return InvokeRole(Role::Revive, response, detail, cap);
}

bool AutoFightAction(bool start, Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    if (!InvokeLuaAction("TopIcon", start ? "AutoTrainClick" : "AutoStopClick", detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    return true;
}

struct BackgroundSellState {
    bool active = false;
    int npcID = 0;
    int stage = 0;
    int lastFree = -1;
    int sold = 0;
    int callbacks = 0;
    int skipped = 0;
    bool pendingCallback = false;
};

BackgroundSellState g_sell;

bool ReadFreeBagSpace(const Classes& c, int& freeSpace, wchar_t* detail, std::size_t cap) {
    freeSpace = -1;
    if (!StaticScalar(c.gameApi, "GetFreeBagSpace", freeSpace, detail, cap) || freeSpace < 0) {
        SetText(detail, cap, L"Không đọc được GetFreeBagSpace");
        return false;
    }
    return true;
}

void RegisterPendingSellResult(int currentFree) {
    if (!g_sell.pendingCallback) return;
    if (currentFree > g_sell.lastFree) g_sell.sold += currentFree - g_sell.lastFree;
    g_sell.pendingCallback = false;
}

bool BeginBackgroundSell(int npcID, Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    int freeSpace = -1;
    if (!ReadFreeBagSpace(c, freeSpace, detail, cap)) return false;
    g_sell = {};
    if (!ClickNpc(npcID, detail, cap)) return false;
    g_sell.active = true;
    g_sell.npcID = npcID;
    g_sell.lastFree = freeSpace;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value0 = g_sell.stage;
    response.value1 = freeSpace;
    return true;
}

bool TryInvokeSellRole(Role role, int nextStage, Response& response,
                       wchar_t* detail, std::size_t cap) {
    UiControl control{};
    if (!FindRoleControl(role, control, detail, cap) || !InvokeControl(control, detail, cap)) return false;
    g_sell.stage = nextStage;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value0 = g_sell.stage;
    response.value1 = g_sell.sold;
    SetText(detail, cap, L"Bán nền: đã gọi control • ");
    Append(detail, cap, control.labels.text.empty() ? control.labels.name.c_str() : control.labels.text.c_str());
    return true;
}

bool AdvanceBackgroundSell(Response& response, wchar_t* detail, std::size_t cap) {
    if (!g_sell.active) { SetText(detail, cap, L"Chưa có phiên bán nền"); return false; }
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    if (g_sell.stage >= 4) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::StageReady);
        response.value0 = g_sell.stage;
        return true;
    }

    wchar_t firstReason[512]{};
    if (g_sell.stage == 0) {
        // Lock the first NPC-function callback to the actual vendor selected by ResID.
        // 373 Mã Kiêu Minh -> "Mua thú cưỡi".
        // 279 Dược Đại Phu / 328 Ba Nhĩ -> "Mua thuốc".
        // For these three approved vendors, try that exact callback first. If the client
        // happens to have opened NPCShop directly, SellTab remains a safe fallback.
        const bool explicitVendor = g_sell.npcID == 373 || g_sell.npcID == 279 || g_sell.npcID == 328;
        const Role shopEntryRole = g_sell.npcID == 373 ? Role::MountShopEntry
                                   : (g_sell.npcID == 279 || g_sell.npcID == 328) ? Role::MedicineShopEntry
                                   : Role::ShopEntry;
        if (explicitVendor) {
            if (TryInvokeSellRole(shopEntryRole, 1, response, detail, cap)) return true;
            SetText(firstReason, _countof(firstReason), detail);
            if (TryInvokeSellRole(Role::SellTab, 2, response, detail, cap)) return true;
        } else {
            // Preserve the previous direct-shop behavior for the other existing presets.
            if (TryInvokeSellRole(Role::SellTab, 2, response, detail, cap)) return true;
            SetText(firstReason, _countof(firstReason), detail);
            if (TryInvokeSellRole(shopEntryRole, 1, response, detail, cap)) return true;
        }
        if (TryInvokeSellRole(Role::QuickSell, 3, response, detail, cap)) return true;
    } else if (g_sell.stage == 1) {
        if (TryInvokeSellRole(Role::SellTab, 2, response, detail, cap)) return true;
        SetText(firstReason, _countof(firstReason), detail);
        if (TryInvokeSellRole(Role::QuickSell, 3, response, detail, cap)) return true;
    } else if (g_sell.stage == 2) {
        if (TryInvokeSellRole(Role::QuickSell, 3, response, detail, cap)) return true;
        SetText(firstReason, _countof(firstReason), detail);
        if (TryInvokeSellRole(Role::EquipmentTab, 4, response, detail, cap)) return true;
    } else if (g_sell.stage == 3) {
        if (TryInvokeSellRole(Role::EquipmentTab, 4, response, detail, cap)) return true;
    }
    if (firstReason[0]) {
        Append(detail, cap, L" • probe trước: ");
        Append(detail, cap, firstReason);
    }
    return false;
}

bool SellFixedBagSlot(int normalizedX, int normalizedY, Response& response,
                      wchar_t* detail, std::size_t cap) {
    if (!g_sell.active || g_sell.stage < 4) { SetText(detail, cap, L"UI bán nền chưa tới bước Trang bị"); return false; }
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    int currentFree = -1;
    if (!ReadFreeBagSpace(c, currentFree, detail, cap)) return false;
    RegisterPendingSellResult(currentFree);
    response.value0 = currentFree;
    response.value1 = g_sell.sold;
    if (g_sell.callbacks >= 90) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::NoCandidate);
        SetText(detail, cap, L"Bán nền dừng ở chặn an toàn 90 callback ô cố định");
        return true;
    }

    if (!InvokeInternalPointClick(normalizedX, normalizedY, detail, cap)) return false;
    ++g_sell.callbacks;
    g_sell.pendingCallback = true;
    g_sell.lastFree = currentFree;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, L"Bán nền: InputSync click nội bộ ô cố định ");
    AppendInt(detail, cap, g_sell.callbacks);
    Append(detail, cap, L"/90 • FreeBag trước click=");
    AppendInt(detail, cap, currentFree);
    Append(detail, cap, L" • đã bán xác minh=");
    AppendInt(detail, cap, g_sell.sold);
    return true;
}


struct BackgroundTreatmentState {
    bool active = false;
    int stage = 0;
    int closeAttempts = 0;
};

BackgroundTreatmentState g_treatment;

bool BeginBackgroundTreatment(int npcID, Response& response, wchar_t* detail, std::size_t cap) {
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    g_treatment = {};
    if (!ClickNpc(npcID, detail, cap)) return false;
    g_treatment.active = true;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value0 = 0;
    SetText(detail, cap, L"Trị liệu nền: đã mở NPC ResID=");
    AppendInt(detail, cap, npcID);
    return true;
}

bool AdvanceBackgroundTreatment(Response& response, wchar_t* detail, std::size_t cap) {
    if (!g_treatment.active) { SetText(detail, cap, L"Chưa có phiên trị liệu nền"); return false; }
    Classes c{};
    if (!ResolveClasses(c, detail, cap) || !SafeForAction(c, detail, cap)) return false;
    if (g_treatment.stage >= 3) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::StageReady);
        response.value0 = g_treatment.stage;
        SetText(detail, cap, L"Trị liệu nền: đủ Treatment → Xác nhận → Ta biết rồi");
        return true;
    }

    const Role role = g_treatment.stage == 0 ? Role::Treatment
                    : g_treatment.stage == 1 ? Role::TreatmentConfirm
                                             : Role::TreatmentAck;
    UiControl control{};
    if (!FindRoleControl(role, control, detail, cap) || !InvokeControl(control, detail, cap)) return false;
    ++g_treatment.stage;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    response.value0 = g_treatment.stage;
    SetText(detail, cap, L"Trị liệu nền: callback bước ");
    AppendInt(detail, cap, g_treatment.stage);
    Append(detail, cap, L"/3 • ");
    Append(detail, cap, control.labels.text.empty() ? control.labels.name.c_str() : control.labels.text.c_str());
    return true;
}

bool CloseBackgroundTreatment(Response& response, wchar_t* detail, std::size_t cap) {
    if (!g_treatment.active) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::NothingToClose);
        SetText(detail, cap, L"Phiên trị liệu nền đã đóng");
        return true;
    }
    UiControl close{};
    if (!FindRoleControl(Role::CloseTradeOrBag, close, detail, cap)) {
        g_treatment.active = false;
        response.resultCode = static_cast<std::int32_t>(ActionResult::NothingToClose);
        SetText(detail, cap, L"Trị liệu nền: không còn UI shop/tay nải cần đóng");
        return true;
    }
    if (!InvokeControl(close, detail, cap)) return false;
    ++g_treatment.closeAttempts;
    if (g_treatment.closeAttempts >= 3) g_treatment.active = false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::UiClosed);
    response.value0 = g_treatment.closeAttempts;
    SetText(detail, cap, L"Trị liệu nền: đã gọi callback đóng UI");
    return true;
}

bool CloseBackgroundSell(Response& response, wchar_t* detail, std::size_t cap) {
    if (!g_sell.active) {
        response.resultCode = static_cast<std::int32_t>(ActionResult::NothingToClose);
        SetText(detail, cap, L"Phiên bán nền đã đóng");
        return true;
    }
    UiControl close{};
    if (!FindRoleControl(Role::CloseTradeOrBag, close, detail, cap)) {
        g_sell.active = false;
        response.resultCode = static_cast<std::int32_t>(ActionResult::NothingToClose);
        response.value0 = g_sell.sold;
        response.value1 = g_sell.skipped;
        SetText(detail, cap, L"Không còn cửa sổ shop/tay nải cần đóng");
        return true;
    }
    if (!InvokeControl(close, detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::UiClosed);
    response.value0 = g_sell.sold;
    response.value1 = g_sell.skipped;
    SetText(detail, cap, L"Đã gọi callback đóng một cửa sổ shop/tay nải");
    return true;
}


const wchar_t* UiDirectTargetName(UiDirectTarget target) {
    switch (target) {
        case UiDirectTarget::CloseItemPopup: return L"X popup item";
        case UiDirectTarget::CloseBag: return L"X Tay nải";
        case UiDirectTarget::CloseTrade: return L"X Giao dịch";
        case UiDirectTarget::TradeConfirm: return L"Xác nhận giao dịch";
        case UiDirectTarget::TradeTabEquip: return L"Tab Trang bị";
        case UiDirectTarget::TradeLock: return L"Khóa";
        case UiDirectTarget::TradeSubmit: return L"Giao dịch";
        case UiDirectTarget::OpenBag: return L"MỞ TAY NẢI";
        case UiDirectTarget::TradeRequestCancel: return L"Hủy bỏ yêu cầu giao dịch";
        default: return L"UNKNOWN";
    }
}

bool UiDirectKeyEquals(const std::wstring& key, std::initializer_list<const wchar_t*> values) {
    for (const wchar_t* value : values) if (value && key == value) return true;
    return false;
}

bool UiDirectTradeConfirmLabel(UiControl& c) {
    CollectDescendantLabels(c);
    const std::wstring text = background_ui_logic::Key(c.labels.text);
    const std::wstring name = background_ui_logic::Key(c.labels.name);
    const std::wstring descendants = background_ui_logic::Key(c.labels.descendants);
    return UiDirectKeyEquals(text, {L"xacnhan", L"dongy", L"confirm"}) ||
           UiDirectKeyEquals(name, {L"xacnhan", L"confirm"}) ||
           background_ui_logic::Has(descendants, {L"xacnhan", L"dongy"});
}

bool UiDirectSameTradeRequestPopup(UiControl& cancel, std::vector<UiControl>& controls) {
    Il2CppObject* cancelParent = nullptr;
    (void)ObjectGetter(cancel.object, cancel.klass, "get_Parent", cancelParent);
    (void)ReadAncestors(cancel);
    const std::wstring cancelAncestors = background_ui_logic::Key(cancel.labels.ancestors);
    for (UiControl& other : controls) {
        if (other.object == cancel.object || !UiDirectTradeConfirmLabel(other)) continue;
        Il2CppObject* otherParent = nullptr;
        (void)ObjectGetter(other.object, other.klass, "get_Parent", otherParent);
        if (cancelParent && otherParent && cancelParent == otherParent) return true;
        (void)ReadAncestors(other);
        const std::wstring otherAncestors = background_ui_logic::Key(other.labels.ancestors);
        if (!cancelAncestors.empty() && cancelAncestors == otherAncestors &&
            background_ui_logic::Has(cancelAncestors, {L"trade", L"exchange", L"giaodich", L"message", L"notice", L"dialog", L"tips", L"popup"}))
            return true;
    }
    return false;
}

bool UiDirectStructuralCloseContext(UiDirectTarget target, UiControl& control) {
    // Some live close buttons are just BtnClose/X while their immediate parent has a
    // generic generated name. Walk only the nearest panel ancestors and inspect each
    // local subtree for strong fingerprints; stop before broad MainUI/HUD roots so an
    // unrelated global X cannot inherit context from the whole screen.
    Il2CppObject* current = control.object;
    std::vector<Il2CppObject*> seen;
    for (int depth = 0; depth < 8 && current; ++depth) {
        if (std::find(seen.begin(), seen.end(), current) != seen.end()) break;
        seen.push_back(current);
        Il2CppClass* currentClass = nullptr;
        if (!ReadLocal(current, 0, currentClass) || !currentClass) break;
        Il2CppObject* parent = nullptr;
        if (!ObjectGetter(current, currentClass, "get_Parent", parent) || !parent) break;
        Il2CppClass* parentClass = nullptr;
        if (!ReadLocal(parent, 0, parentClass) || !parentClass) break;

        UiControl scope{};
        scope.object = parent;
        scope.klass = parentClass;
        scope.kind = UiKind::Rect;
        (void)ReadUiString(parent, parentClass, "Name", scope.labels.name);
        const std::wstring parentName = background_ui_logic::Key(scope.labels.name);
        if (depth > 0 && background_ui_logic::Has(parentName, {L"mainui", L"hud", L"canvas", L"uiroot", L"rootui"}))
            break;
        CollectDescendantLabels(scope);
        const std::wstring local = parentName + background_ui_logic::Key(scope.labels.descendants);

        if (target == UiDirectTarget::CloseTrade) {
            const bool explicitTrade = background_ui_logic::Has(local, {L"trade", L"exchange", L"giaodich"});
            const bool tradeFingerprint = background_ui_logic::Has(local, {L"trangbi"}) &&
                                          background_ui_logic::Has(local, {L"khoa"}) &&
                                          background_ui_logic::Has(local, {L"giaodich"});
            if (explicitTrade || tradeFingerprint) return true;
        } else if (target == UiDirectTarget::CloseBag) {
            if (background_ui_logic::Has(local, {L"roleinfo", L"bag", L"package", L"inventory", L"tuido", L"itempack"}))
                return true;
        } else if (target == UiDirectTarget::CloseItemPopup) {
            const bool item = background_ui_logic::Has(local, {L"item", L"equipinfo", L"iteminfo"});
            const bool popup = background_ui_logic::Has(local, {L"tips", L"tip", L"detail", L"popup"});
            if (item && popup) return true;
        }
        current = parent;
    }
    return false;
}

int UiDirectScore(UiDirectTarget target, UiControl& c) {
    (void)ReadAncestors(c);
    CollectDescendantLabels(c);
    const std::wstring text = background_ui_logic::Key(c.labels.text);
    const std::wstring name = background_ui_logic::Key(c.labels.name);
    const std::wstring handler = background_ui_logic::Key(c.labels.handler);
    const std::wstring tag = background_ui_logic::Key(c.tag);
    const std::wstring parents = background_ui_logic::Key(c.labels.ancestors);
    const std::wstring descendants = background_ui_logic::Key(c.labels.descendants);
    const std::wstring visible = text + name + descendants;
    const std::wstring all = visible + handler + tag + parents;
    bool tradeCtx = background_ui_logic::Has(parents + name, {L"trade", L"exchange", L"giaodich"});
    bool bagCtx = background_ui_logic::Has(parents + name, {L"roleinfo", L"bag", L"package", L"inventory", L"tuido"});
    bool itemCtx = background_ui_logic::Has(parents + name, {L"item", L"tips", L"tip", L"detail", L"popup", L"equipinfo", L"iteminfo"});
    const bool popupCtx = background_ui_logic::Has(parents + name, {L"tips", L"tip", L"detail", L"popup", L"dialog", L"message", L"notice"});
    const bool closeMarker = UiDirectKeyEquals(text, {L"x", L"close", L"dong"}) ||
        background_ui_logic::Has(name + handler + descendants, {L"close", L"btnclose", L"buttonclose", L"closebutton", L"exit", L"quit"});
    if (closeMarker) {
        if (!tradeCtx && target == UiDirectTarget::CloseTrade)
            tradeCtx = UiDirectStructuralCloseContext(target, c);
        if (!bagCtx && target == UiDirectTarget::CloseBag)
            bagCtx = UiDirectStructuralCloseContext(target, c);
        if (!itemCtx && target == UiDirectTarget::CloseItemPopup)
            itemCtx = UiDirectStructuralCloseContext(target, c);
    }

    switch (target) {
        case UiDirectTarget::CloseItemPopup:
            if (!closeMarker) return 0;
            if (itemCtx && popupCtx) return 1200;
            if (itemCtx && !bagCtx && !tradeCtx) return 950;
            return 0;
        case UiDirectTarget::CloseBag:
            return closeMarker && bagCtx && !tradeCtx ? 1150 : 0;
        case UiDirectTarget::CloseTrade:
            return closeMarker && tradeCtx ? 1200 : 0;
        case UiDirectTarget::TradeConfirm: {
            const bool label = UiDirectTradeConfirmLabel(c);
            if (!label) return 0;
            if (tradeCtx || background_ui_logic::Has(all, {L"trade", L"exchange", L"giaodich", L"request"})) return 1200;
            return popupCtx ? 900 : 700;
        }
        case UiDirectTarget::TradeTabEquip: {
            const bool label = UiDirectKeyEquals(text, {L"trangbi", L"equip"}) || UiDirectKeyEquals(name, {L"trangbi", L"equip"}) ||
                background_ui_logic::Has(descendants, {L"trangbi"});
            return label && tradeCtx ? 1200 : 0;
        }
        case UiDirectTarget::TradeLock: {
            const bool unlock = UiDirectKeyEquals(text, {L"bokhoa", L"unlock"}) || background_ui_logic::Has(descendants, {L"bokhoa"});
            const bool label = UiDirectKeyEquals(text, {L"khoa", L"lock"}) || UiDirectKeyEquals(name, {L"khoa", L"lock"}) ||
                background_ui_logic::Has(descendants, {L"khoa"});
            return label && !unlock && tradeCtx ? 1200 : 0;
        }
        case UiDirectTarget::TradeSubmit: {
            const bool label = UiDirectKeyEquals(text, {L"giaodich", L"trade"}) || UiDirectKeyEquals(name, {L"giaodich", L"trade"}) ||
                background_ui_logic::Has(descendants, {L"giaodich"});
            return label && tradeCtx ? 1200 : 0;
        }
        case UiDirectTarget::OpenBag: {
            // Proven Test-UI DATA-222 fingerprint: BottomIcon_Layout ButBag -> ButBagClick.
            const bool exactName = name == L"butbag";
            const bool exactHandler = handler == L"butbagclick";
            if (exactName && exactHandler) return 1800;
            if (exactHandler) return 1700;
            if (exactName) return 1600;
            const bool caption = UiDirectKeyEquals(text, {L"tuido", L"bag"}) || background_ui_logic::Has(descendants, {L"tuido"});
            const bool hud = background_ui_logic::Has(parents, {L"bottomicon", L"mainui", L"hud"});
            return caption && hud ? 1000 : 0;
        }
        case UiDirectTarget::TradeRequestCancel: {
            // Exact button text confirmed in live screenshot: "Hủy bỏ". This base score is
            // intentionally insufficient by itself; FindUiDirectTarget additionally requires
            // an Xác nhận sibling/same-popup proof so another generic Hủy bỏ cannot be clicked.
            const bool label = UiDirectKeyEquals(text, {L"huybo", L"cancel"}) || UiDirectKeyEquals(name, {L"huybo", L"cancel"}) ||
                background_ui_logic::Has(descendants, {L"huybo"});
            return label ? 1250 : 0;
        }
        default:
            return 0;
    }
}

bool FindUiDirectTarget(UiDirectTarget target, std::vector<UiControl>& controls, std::size_t& selectedIndex,
                        int& candidateCount, int& selectedScore, bool& found,
                        wchar_t* detail, std::size_t cap) {
    found = false; selectedIndex = 0; candidateCount = 0; selectedScore = 0;
    if (target < UiDirectTarget::CloseItemPopup || target > UiDirectTarget::TradeRequestCancel) {
        SetText(detail, cap, L"UI DIRECT target không hợp lệ"); return false;
    }
    if (!EnumerateUiDirectControls(controls, detail, cap)) return false;
    struct Candidate { std::size_t index; int score; };
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < controls.size(); ++i) {
        int score = UiDirectScore(target, controls[i]);
        if (score <= 0) continue;
        if (target == UiDirectTarget::TradeRequestCancel && !UiDirectSameTradeRequestPopup(controls[i], controls)) continue;
        candidates.push_back({i, score});
    }
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b) {
        if (a.score != b.score) return a.score > b.score;
        return reinterpret_cast<std::uintptr_t>(controls[a.index].object) < reinterpret_cast<std::uintptr_t>(controls[b.index].object);
    });
    candidateCount = static_cast<int>(candidates.size());
    if (candidates.empty()) {
        SetText(detail, cap, UiDirectTargetName(target)); Append(detail, cap, L" • NOT FOUND"); return true;
    }
    const bool closeSweepTarget = target == UiDirectTarget::CloseItemPopup ||
                                  target == UiDirectTarget::CloseBag ||
                                  target == UiDirectTarget::CloseTrade;
    if (!closeSweepTarget && candidates.size() > 1 && candidates[0].score == candidates[1].score &&
        controls[candidates[0].index].object != controls[candidates[1].index].object) {
        SetText(detail, cap, UiDirectTargetName(target)); Append(detail, cap, L" • AMBIGUOUS • fail-closed • candidates=");
        AppendInt(detail, cap, candidateCount); return false;
    }
    // For X cleanup only, equal-score candidates are all safe members of the same close
    // class. Close one deterministically now; later abort sweeps will close the remainder.
    found = true; selectedIndex = candidates[0].index; selectedScore = candidates[0].score;
    return true;
}

bool ProbeUiDirect(UiDirectTarget target, Response& response, wchar_t* detail, std::size_t cap) {
    std::vector<UiControl> controls; std::size_t index = 0; int count = 0, score = 0; bool found = false;
    if (!FindUiDirectTarget(target, controls, index, count, score, found, detail, cap)) return false;
    response.value0 = found ? 1 : 0; response.value1 = count;
    if (!found) return true;
    SetText(detail, cap, UiDirectTargetName(target)); Append(detail, cap, L" • FOUND • candidates="); AppendInt(detail, cap, count);
    Append(detail, cap, L" • score="); AppendInt(detail, cap, score);
    return true;
}

bool InvokeUiDirect(UiDirectTarget target, Response& response, wchar_t* detail, std::size_t cap) {
    if (target == UiDirectTarget::OpenBag) {
        response.value0 = 0; response.value1 = 0;
        SetText(detail, cap, L"MỞ TAY NẢI semantic invoke đã khóa • dùng tọa F8 đã gán");
        return false;
    }
    std::vector<UiControl> controls; std::size_t index = 0; int count = 0, score = 0; bool found = false;
    if (!FindUiDirectTarget(target, controls, index, count, score, found, detail, cap)) return false;
    response.value0 = found ? 1 : 0; response.value1 = count;
    if (!found) return true;
    if (!InvokeControl(controls[index], detail, cap)) return false;
    response.resultCode = static_cast<std::int32_t>(ActionResult::ActionInvoked);
    SetText(detail, cap, UiDirectTargetName(target)); Append(detail, cap, L" • DIRECT INVOKED • candidates="); AppendInt(detail, cap, count);
    Append(detail, cap, L" • score="); AppendInt(detail, cap, score);
    return true;
}

bool EnsureShared() {
    if (g_shared) return true;
    wchar_t name[96]{}; MappingName(GetCurrentProcessId(), name, _countof(name));
    g_mapping = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!g_mapping) return false;
    g_shared = reinterpret_cast<SharedBlock*>(MapViewOfFile(g_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedBlock)));
    if (!g_shared || g_shared->magic != kMagic || g_shared->protocolVersion != kProtocolVersion ||
        g_shared->targetPid != GetCurrentProcessId()) {
        if (g_shared) UnmapViewOfFile(g_shared);
        if (g_mapping) CloseHandle(g_mapping);
        g_shared = nullptr; g_mapping = nullptr; return false;
    }
    InterlockedExchange(&g_shared->bridgeLoaded, 1);
    return true;
}

void ProcessRequest() {
    if (!EnsureShared()) return;
    const LONG seq = g_shared->requestSeq;
    if (seq <= 0 || seq == g_shared->completedSeq) return;
    if (InterlockedCompareExchange(&g_shared->bridgeBusy, 1, 0) != 0) return;

    Response r{};
    wchar_t detail[512]{};
    bool ok = false;
    const DWORD callbackThreadId = GetCurrentThreadId();
    if (callbackThreadId != g_shared->targetWindowThreadId) {
        SetText(detail, _countof(detail), L"Sai callback thread; action bị chặn");
    } else {
        const Command cmd = static_cast<Command>(g_shared->request.command);
        const bool protectedCommand = IsLicenseProtectedCommand(cmd);
        const bool licenseProofOk = !protectedCommand ||
            (g_shared->licenseGate != 0 && g_shared->licenseSessionToken != 0 &&
             g_shared->requestLicenseProof == LicenseRequestProof(
                 g_shared->licenseSessionToken, seq, g_shared->request));
        if (!licenseProofOk) {
            SetText(detail, _countof(detail), L"LICENSE BRIDGE GUARD: mutating command bị chặn");
        } else switch (cmd) {
            case Command::ReadState:
                ok = ReadState(r.snapshot, detail, _countof(detail)); break;
            case Command::ReadCurrency:
                ok = ReadCurrency(r.value64_0, r.value64_1, r.value0, detail, _countof(detail)); break;
            case Command::ReadBagPage:
                ok = ReadBagPage(g_shared->request.arg0, r, detail, _countof(detail)); break;
            case Command::ProbeUiDirect:
                ok = ProbeUiDirect(static_cast<UiDirectTarget>(g_shared->request.arg0), r, detail, _countof(detail)); break;
            case Command::InvokeUiDirect:
                ok = InvokeUiDirect(static_cast<UiDirectTarget>(g_shared->request.arg0), r, detail, _countof(detail)); break;
            case Command::DropBagItem:
                ok = DropBagItem(RequestInstanceID(g_shared->request.arg0, g_shared->request.arg1),
                                 g_shared->request.arg2, r, detail, _countof(detail)); break;
            case Command::SellBagItem:
                ok = SellBagItem(RequestInstanceID(g_shared->request.arg0, g_shared->request.arg1),
                                 g_shared->request.arg2, r, detail, _countof(detail)); break;
            case Command::SelectTargetByRoleID:
                ok = SelectTargetByRoleID(g_shared->request.arg0, g_shared->request.arg1 != 0, r, detail, _countof(detail)); break;
            case Command::ClickTravelSemantic:
                ok = ClickTravelSemantic(static_cast<TravelSemantic>(g_shared->request.arg0), g_shared->request.arg1 != 0, r, detail, _countof(detail)); break;
            case Command::ConfirmTravelSemantic:
                ok = ConfirmTravelSemantic(r, detail, _countof(detail)); break;
            case Command::ProbeNearbyLoot:
                ok = ProbeNearbyLoot(r, detail, _countof(detail)); break;
            case Command::PickNearestLoot:
                ok = PickNearestLoot(r, detail, _countof(detail)); break;
            case Command::ToggleRide:
                ok = ToggleRide(g_shared->request.arg0 != 0, detail, _countof(detail)); break;
            case Command::StartPath:
                ok = StartPath(g_shared->request.arg0, g_shared->request.arg1, g_shared->request.arg2,
                               detail, _countof(detail)); break;
            case Command::StopPath:
                ok = StopPath(detail, _countof(detail)); break;
            case Command::ClickNpc:
                ok = ClickNpc(g_shared->request.arg0, detail, _countof(detail)); break;
            case Command::ConfirmMap:
                ok = ConfirmMap(r, detail, _countof(detail)); break;
            case Command::Revive:
                ok = Revive(r, detail, _countof(detail)); break;
            case Command::StartAutoFight:
                ok = AutoFightAction(true, r, detail, _countof(detail)); break;
            case Command::StopAutoFight:
                ok = AutoFightAction(false, r, detail, _countof(detail)); break;
            case Command::BeginBackgroundSell:
                ok = BeginBackgroundSell(g_shared->request.arg0, r, detail, _countof(detail)); break;
            case Command::AdvanceBackgroundSell:
                ok = AdvanceBackgroundSell(r, detail, _countof(detail)); break;
            case Command::SellNextBagItem:
                ok = SellFixedBagSlot(g_shared->request.arg0, g_shared->request.arg1,
                                      r, detail, _countof(detail)); break;
            case Command::CloseBackgroundSell:
                ok = CloseBackgroundSell(r, detail, _countof(detail)); break;
            case Command::ClickInternalPoint:
                ok = ClickInternalPoint(g_shared->request.arg0, g_shared->request.arg1,
                                        g_shared->request.arg2 != 0,
                                        r, detail, _countof(detail)); break;
            case Command::ClickInternalPointRawTest:
                ok = ClickInternalPointRawTest(g_shared->request.arg0, g_shared->request.arg1,
                                               r, detail, _countof(detail)); break;
            case Command::DragInternalPoint:
                ok = DragInternalPoint(g_shared->request.arg0, g_shared->request.arg1, g_shared->request.arg2, r, detail, _countof(detail)); break;
            case Command::BeginBackgroundTreatment:
                ok = BeginBackgroundTreatment(g_shared->request.arg0, r, detail, _countof(detail)); break;
            case Command::AdvanceBackgroundTreatment:
                ok = AdvanceBackgroundTreatment(r, detail, _countof(detail)); break;
            case Command::CloseBackgroundTreatment:
                ok = CloseBackgroundTreatment(r, detail, _countof(detail)); break;
            default:
                SetText(detail, _countof(detail), L"Command không hợp lệ"); break;
        }
    }
    r.ok = ok ? 1 : 0;
    SetText(r.detail, _countof(r.detail), detail);
    g_shared->response = r;
    MemoryBarrier();
    InterlockedExchange(&g_shared->completedSeq, seq);
    InterlockedExchange(&g_shared->bridgeBusy, 0);
}

} // namespace

extern "C" __declspec(dllexport) LRESULT CALLBACK TlcGetMessageHook(int code, WPARAM wParam, LPARAM lParam) {
    (void)wParam;
    if (code >= 0 && lParam) {
        const MSG* msg = reinterpret_cast<const MSG*>(lParam);
        if (msg->message == kWakeMessage) ProcessRequest();
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_shared) UnmapViewOfFile(g_shared);
        if (g_mapping) CloseHandle(g_mapping);
        g_shared = nullptr; g_mapping = nullptr;
    }
    return TRUE;
}
