//===-- LibCxxVector.cpp --------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LibCxx.h"

#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/lldb-forward.h"
#include <optional>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private {
namespace formatters {
class LibcxxStdVectorSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  LibcxxStdVectorSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  ~LibcxxStdVectorSyntheticFrontEnd() override;

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override;

private:
  ValueObject *m_start = nullptr;
  ValueObject *m_finish = nullptr;
  CompilerType m_element_type;
  uint32_t m_element_size = 0;
};

class LibcxxVectorBoolSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  LibcxxVectorBoolSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  llvm::Expected<uint32_t> CalculateNumChildren() override;

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override;

private:
  CompilerType m_bool_type;
  ExecutionContextRef m_exe_ctx_ref;
  uint64_t m_count = 0;
  lldb::addr_t m_base_data_address = 0;
  std::map<size_t, lldb::ValueObjectSP> m_children;
};

} // namespace formatters
} // namespace lldb_private

lldb_private::formatters::LibcxxStdVectorSyntheticFrontEnd::
    LibcxxStdVectorSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_element_type() {
  if (valobj_sp)
    Update();
}

lldb_private::formatters::LibcxxStdVectorSyntheticFrontEnd::
    ~LibcxxStdVectorSyntheticFrontEnd() {
  // these need to stay around because they are child objects who will follow
  // their parent's life cycle
  // delete m_start;
  // delete m_finish;
}

llvm::Expected<uint32_t> lldb_private::formatters::
    LibcxxStdVectorSyntheticFrontEnd::CalculateNumChildren() {
  if (!m_start || !m_finish)
    return llvm::createStringError(
        "Failed to determine start/end of vector data.");

  uint64_t start_val = m_start->GetValueAsUnsigned(0);
  uint64_t finish_val = m_finish->GetValueAsUnsigned(0);

  // A default-initialized empty vector.
  if (start_val == 0 && finish_val == 0)
    return 0;

  if (start_val == 0)
    return llvm::createStringError("Invalid value for start of vector.");

  if (finish_val == 0)
    return llvm::createStringError("Invalid value for end of vector.");

  if (start_val > finish_val)
    return llvm::createStringError(
        "Start of vector data begins after end pointer.");

  size_t num_children = (finish_val - start_val);
  if (num_children % m_element_size)
    return llvm::createStringError("Size not multiple of element size.");

  return num_children / m_element_size;
}

lldb::ValueObjectSP
lldb_private::formatters::LibcxxStdVectorSyntheticFrontEnd::GetChildAtIndex(
    uint32_t idx) {
  if (!m_start || !m_finish)
    return lldb::ValueObjectSP();

  uint64_t offset = idx * m_element_size;
  offset = offset + m_start->GetValueAsUnsigned(0);
  StreamString name;
  name.Printf("[%" PRIu64 "]", (uint64_t)idx);
  return CreateValueObjectFromAddress(name.GetString(), offset,
                                      m_backend.GetExecutionContextRef(),
                                      m_element_type);
}

static ValueObjectSP GetDataPointer(ValueObject &root) {
  if (auto cap_sp = root.GetChildMemberWithName("__cap_"))
    return cap_sp;

  ValueObjectSP cap_sp = root.GetChildMemberWithName("__end_cap_");
  if (!cap_sp)
    return nullptr;

  if (!isOldCompressedPairLayout(*cap_sp))
    return nullptr;

  return GetFirstValueOfLibCXXCompressedPair(*cap_sp);
}

lldb::ChildCacheState
lldb_private::formatters::LibcxxStdVectorSyntheticFrontEnd::Update() {
  m_start = m_finish = nullptr;
  ValueObjectSP data_sp(GetDataPointer(m_backend));

  if (!data_sp)
    return lldb::ChildCacheState::eRefetch;

  m_element_type = data_sp->GetCompilerType().GetPointeeType();
  llvm::Expected<uint64_t> size_or_err = m_element_type.GetByteSize(nullptr);
  if (!size_or_err)
    LLDB_LOG_ERRORV(GetLog(LLDBLog::DataFormatters), size_or_err.takeError(),
                    "{0}");
  else {
    m_element_size = *size_or_err;

    if (m_element_size > 0) {
      // store raw pointers or end up with a circular dependency
      m_start = m_backend.GetChildMemberWithName("__begin_").get();
      m_finish = m_backend.GetChildMemberWithName("__end_").get();
    }
  }
  return lldb::ChildCacheState::eRefetch;
}

llvm::Expected<size_t>
lldb_private::formatters::LibcxxStdVectorSyntheticFrontEnd::
    GetIndexOfChildWithName(ConstString name) {
  if (!m_start || !m_finish)
    return llvm::createStringError("Type has no child named '%s'",
                                   name.AsCString());
  size_t index = formatters::ExtractIndexFromString(name.GetCString());
  if (index == UINT32_MAX) {
    return llvm::createStringError("Type has no child named '%s'",
                                   name.AsCString());
  }
  return index;
}

lldb_private::formatters::LibcxxVectorBoolSyntheticFrontEnd::
    LibcxxVectorBoolSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp), m_bool_type(), m_exe_ctx_ref(),
      m_children() {
  if (valobj_sp) {
    Update();
    m_bool_type =
        valobj_sp->GetCompilerType().GetBasicTypeFromAST(lldb::eBasicTypeBool);
  }
}

llvm::Expected<uint32_t> lldb_private::formatters::
    LibcxxVectorBoolSyntheticFrontEnd::CalculateNumChildren() {
  return m_count;
}

lldb::ValueObjectSP
lldb_private::formatters::LibcxxVectorBoolSyntheticFrontEnd::GetChildAtIndex(
    uint32_t idx) {
  auto iter = m_children.find(idx), end = m_children.end();
  if (iter != end)
    return iter->second;
  if (idx >= m_count)
    return {};
  if (m_base_data_address == 0 || m_count == 0)
    return {};
  if (!m_bool_type)
    return {};
  size_t byte_idx = (idx >> 3); // divide by 8 to get byte index
  size_t bit_index = (idx & 7); // efficient idx % 8 for bit index
  lldb::addr_t byte_location = m_base_data_address + byte_idx;
  ProcessSP process_sp(m_exe_ctx_ref.GetProcessSP());
  if (!process_sp)
    return {};
  uint8_t byte = 0;
  uint8_t mask = 0;
  Status err;
  size_t bytes_read = process_sp->ReadMemory(byte_location, &byte, 1, err);
  if (err.Fail() || bytes_read == 0)
    return {};
  mask = 1 << bit_index;
  bool bit_set = ((byte & mask) != 0);
  std::optional<uint64_t> size =
      llvm::expectedToOptional(m_bool_type.GetByteSize(nullptr));
  if (!size)
    return {};
  WritableDataBufferSP buffer_sp(new DataBufferHeap(*size, 0));
  if (bit_set && buffer_sp && buffer_sp->GetBytes()) {
    // regardless of endianness, anything non-zero is true
    *(buffer_sp->GetBytes()) = 1;
  }
  StreamString name;
  name.Printf("[%" PRIu64 "]", (uint64_t)idx);
  ValueObjectSP retval_sp(CreateValueObjectFromData(
      name.GetString(),
      DataExtractor(buffer_sp, process_sp->GetByteOrder(),
                    process_sp->GetAddressByteSize()),
      m_exe_ctx_ref, m_bool_type));
  if (retval_sp)
    m_children[idx] = retval_sp;
  return retval_sp;
}

lldb::ChildCacheState
lldb_private::formatters::LibcxxVectorBoolSyntheticFrontEnd::Update() {
  m_children.clear();
  ValueObjectSP valobj_sp = m_backend.GetSP();
  if (!valobj_sp)
    return lldb::ChildCacheState::eRefetch;
  m_exe_ctx_ref = valobj_sp->GetExecutionContextRef();
  ValueObjectSP size_sp(valobj_sp->GetChildMemberWithName("__size_"));
  if (!size_sp)
    return lldb::ChildCacheState::eRefetch;
  m_count = size_sp->GetValueAsUnsigned(0);
  if (!m_count)
    return lldb::ChildCacheState::eReuse;
  ValueObjectSP begin_sp(valobj_sp->GetChildMemberWithName("__begin_"));
  if (!begin_sp) {
    m_count = 0;
    return lldb::ChildCacheState::eRefetch;
  }
  m_base_data_address = begin_sp->GetValueAsUnsigned(0);
  if (!m_base_data_address) {
    m_count = 0;
    return lldb::ChildCacheState::eRefetch;
  }
  return lldb::ChildCacheState::eRefetch;
}

llvm::Expected<size_t>
lldb_private::formatters::LibcxxVectorBoolSyntheticFrontEnd::
    GetIndexOfChildWithName(ConstString name) {
  if (!m_count || !m_base_data_address)
    return llvm::createStringError("Type has no child named '%s'",
                                   name.AsCString());
  const char *item_name = name.GetCString();
  uint32_t idx = ExtractIndexFromString(item_name);
  if (idx == UINT32_MAX ||
      (idx < UINT32_MAX && idx >= CalculateNumChildrenIgnoringErrors()))
    return llvm::createStringError("Type has no child named '%s'",
                                   name.AsCString());
  return idx;
}

lldb_private::SyntheticChildrenFrontEnd *
lldb_private::formatters::LibcxxStdVectorSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp)
    return nullptr;
  CompilerType type = valobj_sp->GetCompilerType();
  if (!type.IsValid() || type.GetNumTemplateArguments() == 0)
    return nullptr;
  CompilerType arg_type = type.GetTypeTemplateArgument(0);
  if (arg_type.GetTypeName() == "bool")
    return new LibcxxVectorBoolSyntheticFrontEnd(valobj_sp);
  return new LibcxxStdVectorSyntheticFrontEnd(valobj_sp);
}
