//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <memory>

#include "rocksdb/merge_operator.h"
#include "rocksdb/lazy_buffer.h"
#include "utilities/merge_operators.h"

using rocksdb::Slice;
using rocksdb::LazyBuffer;
using rocksdb::LazyBufferReference;
using rocksdb::Logger;
using rocksdb::MergeOperator;

namespace {  // anonymous namespace

// Merge operator that picks the maximum operand, Comparison is based on
// Slice::compare
class MaxOperator : public MergeOperator {
 public:
  virtual bool FullMergeV2(const MergeOperationInput& merge_in,
                           MergeOperationOutput* merge_out) const override {
    LazyBuffer max_slice;
    const LazyBuffer*& max = merge_out->existing_operand;
    if (merge_in.existing_value) {
      max_slice = LazyBufferReference(*merge_in.existing_value);
    } else {
      max_slice.clear();
    }

    for (const auto& op : merge_in.operand_list) {
      if (!max_slice.fetch().ok() || !op.fetch().ok()) {
        return false;
      }
      if (max_slice.compare(op) < 0) {
        max = &op;
        max_slice = LazyBufferReference(op);
      }
    }

    return true;
  }

  virtual bool PartialMerge(const Slice& /*key*/,
                            const LazyBuffer& left_operand,
                            const LazyBuffer& right_operand,
                            LazyBuffer* new_value,
                            Logger* /*logger*/) const override {
    if (!left_operand.fetch().ok() || !right_operand.fetch().ok()) {
      return false;
    }
    if (left_operand.compare(right_operand) >= 0) {
      new_value->assign(left_operand);
    } else {
      new_value->assign(right_operand);
    }
    return true;
  }

  virtual bool PartialMergeMulti(const Slice& /*key*/,
                                 const std::vector<LazyBuffer>& operand_list,
                                 LazyBuffer* new_value,
                                 Logger* /*logger*/) const override {
    LazyBuffer max;
    for (const auto& operand : operand_list) {
      if (!max.fetch().ok() || !operand.fetch().ok()) {
        return false;
      }
      if (max.compare(operand) < 0) {
        max = LazyBufferReference(operand);
      }
    }

    new_value->assign(max);
    return true;
  }

  virtual const char* Name() const override { return "MaxOperator"; }
};

}  // end of anonymous namespace

namespace rocksdb {

std::shared_ptr<MergeOperator> MergeOperators::CreateMaxOperator() {
  return std::make_shared<MaxOperator>();
}
}
