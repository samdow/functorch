// Copyright (c) Facebook, Inc. and its affiliates.
// All rights reserved.
//
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree.

#include <functorch/csrc/BatchRulesHelper.h>
#include <ATen/WrapDimUtils.h>

namespace at { namespace functorch {

Tensor moveBatchDimToFront(const Tensor& tensor, optional<int64_t> maybe_batch_dim) {
  if (!maybe_batch_dim.has_value()) {
    return tensor;
  }
  if (maybe_batch_dim.value() == 0) {
    return tensor;
  }
  return tensor.movedim(maybe_batch_dim.value(), 0);
}

int64_t rankWithoutBatchDim(const Tensor& tensor, optional<int64_t> maybe_batch_dim) {
  int64_t result = tensor.dim();
  if (maybe_batch_dim.has_value()) {
    result -= 1;
  }
  return result;
}

int64_t numelWithoutBatchDim(const Tensor& tensor, optional<int64_t> maybe_batch_dim) {
  if (!maybe_batch_dim) {
    return tensor.numel();
  }
  return tensor.numel() / tensor.size(*maybe_batch_dim);
}

optional<int64_t> valIfNonempty(optional<int64_t> maybe_empty, int64_t new_val) {
  if (maybe_empty.has_value()) {
    return new_val;
  }
  return nullopt;
}

int64_t getPhysicalDim(const Tensor& tensor, bool has_batch_dim, int64_t logical_dim) {
  // NB: assumes the batch dim is at the front of the tensor
  optional<int64_t> bdim = has_batch_dim ? optional<int64_t>(0) : nullopt;
  auto rank = rankWithoutBatchDim(tensor, bdim);
  auto wrapped_dim = maybe_wrap_dim(logical_dim, rank);
  if (has_batch_dim) {
    return wrapped_dim + 1;
  }
  return wrapped_dim;
}

Tensor maybePadToLogicalRank(const Tensor& tensor, optional<int64_t> has_bdim, int64_t logical_rank) {
  if (!has_bdim) {
    return tensor;
  }
  auto tensor_logical_rank = rankWithoutBatchDim(tensor, has_bdim);
  if (tensor_logical_rank >= logical_rank) {
    return tensor;
  }
  VmapDimVector new_sizes(tensor.sizes().begin(), tensor.sizes().end());
  for (int64_t i = 0; i < logical_rank - tensor_logical_rank; i++) {
    new_sizes.insert(new_sizes.begin() + 1, 1);
  }
  return tensor.view(new_sizes);
}

Tensor reshape_dim_into(int64_t src, int64_t dst, const Tensor& x) {
  auto x_dim = x.dim();
  src = maybe_wrap_dim(src, x_dim);
  dst = maybe_wrap_dim(dst, x_dim - 1); // Returned Tensor has one fewer dim
  VmapDimVector new_shape(x.sizes().begin(), x.sizes().end());
  new_shape.erase(new_shape.begin() + src);
  new_shape[dst] *= x.sizes()[src];
  return at::reshape(x.movedim(src, dst), new_shape);
}

Tensor reshape_dim_outof(int64_t src, int64_t size1, const Tensor& x) {
  src = maybe_wrap_dim(src, x.dim());
  VmapDimVector shape(x.sizes().begin(), x.sizes().end());
  TORCH_INTERNAL_ASSERT(shape[src] % size1 == 0);
  int64_t size2 = shape[src] / size1;
  shape[src] = size1;
  shape.insert(shape.begin() + src + 1, size2);
  return at::reshape(x, shape);
}

void vmapIncompatibleInplaceError(const char* schema_name) {
  TORCH_CHECK(false,
    "vmap: ", schema_name, "(self, *extra_args) is not possible because ",
    "there exists a Tensor `other` in extra_args that has more elements ",
    "than `self`. This happened due to `other` being vmapped over but ",
    "`self` not being vmapped over in a vmap. ",
    "Please try to use out-of-place operators instead of ", schema_name, ". ",
    "If said operator is being called inside the PyTorch framework, ",
    "please file a bug report instead.");
}

}}
