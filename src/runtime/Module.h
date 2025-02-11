/*
 * Copyright (c) 2022-present Samsung Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WalrusModule__
#define __WalrusModule__

#include <numeric>
#include "runtime/Value.h"
#include "util/Vector.h"

namespace wabt {
class WASMBinaryReader;
}

namespace Walrus {

class Store;
class Module;
class Instance;

class FunctionType : public gc {
public:
    typedef Vector<Value::Type, GCUtil::gc_malloc_atomic_allocator<Value::Type>>
        FunctionTypeVector;
    FunctionType(uint32_t index,
                 FunctionTypeVector&& param,
                 FunctionTypeVector&& result)
        : m_index(index)
        , m_param(std::move(param))
        , m_result(std::move(result))
        , m_paramStackSize(computeStackSize(m_param))
        , m_resultStackSize(computeStackSize(m_result))
    {
    }

    uint32_t index() const { return m_index; }

    const FunctionTypeVector& param() const { return m_param; }

    const FunctionTypeVector& result() const { return m_result; }

    size_t paramStackSize() const { return m_paramStackSize; }

    size_t resultStackSize() const { return m_resultStackSize; }

private:
    uint32_t m_index;
    FunctionTypeVector m_param;
    FunctionTypeVector m_result;
    size_t m_paramStackSize;
    size_t m_resultStackSize;

    static size_t computeStackSize(const FunctionTypeVector& v)
    {
        size_t s = 0;
        for (size_t i = 0; i < v.size(); i++) {
            s += valueSizeInStack(v[i]);
        }
        return s;
    }
};

// https://webassembly.github.io/spec/core/syntax/modules.html#syntax-import
class ModuleImport : public gc {
public:
    enum Type { Function,
                Table,
                Memory,
                Global };

    ModuleImport(uint32_t importIndex,
                 String* moduleName,
                 String* fieldName,
                 uint32_t functionIndex,
                 uint32_t functionTypeIndex)
        : m_type(Type::Function)
        , m_importIndex(importIndex)
        , m_moduleName(std::move(moduleName))
        , m_fieldName(std::move(fieldName))
        , m_functionIndex(functionIndex)
        , m_functionTypeIndex(functionTypeIndex)
    {
    }

    ModuleImport(uint32_t importIndex,
                 String* moduleName,
                 String* fieldName,
                 uint32_t globalIndex)
        : m_type(Type::Global)
        , m_importIndex(importIndex)
        , m_moduleName(std::move(moduleName))
        , m_fieldName(std::move(fieldName))
        , m_globalIndex(globalIndex)
    {
    }

    Type type() const { return m_type; }

    uint32_t importIndex() const { return m_importIndex; }

    String* moduleName() const { return m_moduleName; }

    String* fieldName() const { return m_fieldName; }

    uint32_t functionIndex() const
    {
        ASSERT(type() == Type::Function);
        return m_functionIndex;
    }

    uint32_t functionTypeIndex() const
    {
        ASSERT(type() == Type::Function);
        return m_functionTypeIndex;
    }

    uint32_t globalIndex() const
    {
        ASSERT(type() == Type::Global);
        return m_globalIndex;
    }

private:
    Type m_type;
    uint32_t m_importIndex;
    String* m_moduleName;
    String* m_fieldName;

    union {
        struct {
            uint32_t m_functionIndex;
            uint32_t m_functionTypeIndex;
        };
        struct {
            uint32_t m_globalIndex;
        };
    };
};

class ModuleExport : public gc {
public:
    // matches binary format, do not change
    enum Type { Function,
                Table,
                Memory,
                Global };

    ModuleExport(Type type,
                 String* name,
                 uint32_t exportIndex,
                 uint32_t itemIndex)
        : m_type(type)
        , m_name(name)
        , m_exportIndex(exportIndex)
        , m_itemIndex(itemIndex)
    {
    }

    Type type() const { return m_type; }

    uint32_t exportIndex() const { return m_exportIndex; }

    String* name() const { return m_name; }

    uint32_t itemIndex() const
    {
        return m_itemIndex;
    }

private:
    Type m_type;
    String* m_name;
    uint32_t m_exportIndex;
    uint32_t m_itemIndex;
};

class ModuleFunction : public gc {
    friend class wabt::WASMBinaryReader;

public:
    typedef Vector<Value::Type, GCUtil::gc_malloc_atomic_allocator<Value::Type>>
        LocalValueVector;

    ModuleFunction(Module* module, uint32_t functionIndex, uint32_t functionTypeIndex)
        : m_module(module)
        , m_functionIndex(functionIndex)
        , m_functionTypeIndex(functionTypeIndex)
        , m_requiredStackSize(0)
        , m_requiredStackSizeDueToLocal(0)
    {
    }

    Module* module() const { return m_module; }

    uint32_t functionIndex() const { return m_functionIndex; }

    uint32_t functionTypeIndex() const { return m_functionTypeIndex; }

    uint32_t requiredStackSize() const { return m_requiredStackSize; }
    uint32_t requiredStackSizeDueToLocal() const { return m_requiredStackSizeDueToLocal; }

    template <typename CodeType>
    void pushByteCode(const CodeType& code)
    {
        char* first = (char*)&code;
        size_t start = m_byteCode.size();

        m_byteCode.resizeWithUninitializedValues(m_byteCode.size() + sizeof(CodeType));
        for (size_t i = 0; i < sizeof(CodeType); i++) {
            m_byteCode[start++] = *first;
            first++;
        }
    }

    template <typename CodeType>
    CodeType* peekByteCode(size_t position)
    {
        return reinterpret_cast<CodeType*>(&m_byteCode[position]);
    }

    void expandByteCode(size_t s)
    {
        m_byteCode.resizeWithUninitializedValues(m_byteCode.size() + s);
    }

    void shrinkByteCode(size_t s)
    {
        m_byteCode.resize(m_byteCode.size() - s);
    }

    size_t currentByteCodeSize() const
    {
        return m_byteCode.size();
    }

    uint8_t* byteCode() { return m_byteCode.data(); }
#if !defined(NDEBUG)
    void dumpByteCode();
#endif

private:
    Module* m_module;
    uint32_t m_functionIndex;
    uint32_t m_functionTypeIndex;
    uint32_t m_requiredStackSize;
    uint32_t m_requiredStackSizeDueToLocal;
    LocalValueVector m_local;
    Vector<uint8_t, GCUtil::gc_malloc_atomic_allocator<uint8_t>> m_byteCode;
};

class Module : public gc {
    friend class wabt::WASMBinaryReader;

public:
    Module(Store* store)
        : m_store(store)
        , m_seenStartAttribute(false)
        , m_version(0)
        , m_start(0)
    {
    }

    ModuleFunction* function(uint32_t index)
    {
        for (size_t i = 0; i < m_function.size(); i++) {
            if (m_function[i]->functionIndex() == index) {
                return m_function[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    FunctionType* functionType(uint32_t index)
    {
        for (size_t i = 0; i < m_functionType.size(); i++) {
            if (m_functionType[i]->index() == index) {
                return m_functionType[i];
            }
        }
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    const Vector<ModuleImport*, GCUtil::gc_malloc_allocator<ModuleImport*>>& moduleImport() const
    {
        return m_import;
    }

    const Vector<ModuleExport*, GCUtil::gc_malloc_allocator<ModuleExport*>>& moduleExport() const
    {
        return m_export;
    }

    Instance* instantiate(const ValueVector& imports);

private:
    Store* m_store;
    bool m_seenStartAttribute;
    uint32_t m_version;
    uint32_t m_start;
    Vector<ModuleImport*, GCUtil::gc_malloc_allocator<ModuleImport*>> m_import;
    Vector<ModuleExport*, GCUtil::gc_malloc_allocator<ModuleExport*>> m_export;
    Vector<FunctionType*, GCUtil::gc_malloc_allocator<FunctionType*>>
        m_functionType;
    Vector<ModuleFunction*, GCUtil::gc_malloc_allocator<ModuleFunction*>>
        m_function;
    /* initialSize, maximumSize */
    Vector<std::pair<size_t, size_t>, GCUtil::gc_malloc_atomic_allocator<std::pair<size_t, size_t>>>
        m_memory;
    Vector<std::tuple<Value::Type, size_t, size_t>, GCUtil::gc_malloc_atomic_allocator<std::tuple<Value::Type, size_t, size_t>>>
        m_table;
    Vector<std::tuple<Value::Type, bool>, GCUtil::gc_malloc_atomic_allocator<std::tuple<Value::Type, bool>>>
        m_global;
    Optional<ModuleFunction*> m_globalInitBlock;
};

} // namespace Walrus

#endif // __WalrusModule__
