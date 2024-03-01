// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "core/node.hpp"

namespace ov {
namespace frontend {
namespace onnx {
namespace op {
namespace set_1 {
///
/// \brief Convert ONNX MaxPool operation to an OV node.
///
/// \param node   The ONNX node object representing this operation.
///
/// \return The vector containing OV nodes producing output of ONNX MaxPool
///         operation.
///
ov::OutputVector max_pool(const ov::frontend::onnx::Node& node);

}  // namespace set_1

namespace set_8 {
///
/// \brief Convert ONNX MaxPool operation to an OV node.
///
/// \param node   The ONNX node object representing this operation.
///
/// \return The vector containing OV nodes producing output of ONNX MaxPool
///         operation.
///
ov::OutputVector max_pool(const ov::frontend::onnx::Node& node);

}  // namespace set_8
}  // namespace op
}  // namespace onnx
}  // namespace frontend
}  // namespace ov
