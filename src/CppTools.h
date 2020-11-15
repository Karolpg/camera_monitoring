#pragma once

#include <functional>

namespace CppTools {

template<typename T, typename... U>
void* functionAddress(std::function<T(U...)> f) {
    typedef T(fnType)(U...);
    fnType **fnPtr = f.template target<fnType*>();
    return reinterpret_cast<void*>(*fnPtr);
}

} // namespace CppTools
