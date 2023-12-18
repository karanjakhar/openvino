// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "ov_lpt_models/fuse_convert.hpp"

#include "openvino/opsets/opset1.hpp"
#include "ov_models/subgraph_builders.hpp"
#include "ov_lpt_models/common/builders.hpp"

namespace ngraph {
namespace builder {
namespace subgraph {

std::shared_ptr<ov::Model> FuseConvertFunction::get(
    const ov::PartialShape& inputShape,
    const ov::element::Type inputPrecision,
    const ngraph::builder::subgraph::DequantizationOperations& dequantization,
    const ngraph::builder::subgraph::FakeQuantizeOnData& fakeQuantize,
    const bool constInput) {
    std::shared_ptr<Node> parent;
    std::shared_ptr<op::Parameter> input;
    if (constInput) {
        parent = std::make_shared<ov::opset1::Constant>(inputPrecision, inputShape.to_shape(), std::vector<float>{ 128.f });
    } else {
        input = std::make_shared<ov::opset1::Parameter>(
            inputPrecision,
            inputShape);
        parent = input;
    }

    parent = makeDequantization(parent, dequantization);

    if (!fakeQuantize.empty()) {
        parent = makeFakeQuantize(parent, fakeQuantize.outputPrecision, fakeQuantize);
    }

    parent->set_friendly_name("output");

    auto parameters = constInput ?
        ov::ParameterVector{}:
        ov::ParameterVector{ input };

    ov::ResultVector results{std::make_shared<ov::opset1::Result>(parent)};
    return std::make_shared<ov::Model>(results, parameters, "FuseConvertFunction");
}

std::shared_ptr<ov::Model> FuseConvertFunction::getWithFQ(
    const ov::PartialShape& inputShape,
    const ov::element::Type inputPrecision,
    const ngraph::builder::subgraph::DequantizationOperations& dequantization,
    const bool constInput) {
    std::shared_ptr<Node> parent;
    std::shared_ptr<op::Parameter> input1;
    if (constInput) {
        parent = std::make_shared<ov::opset1::Constant>(inputPrecision, inputShape.to_shape(), std::vector<float>{ 128.f });
    } else {
        input1 = std::make_shared<ov::opset1::Parameter>(
                inputPrecision,
                inputShape);
        parent = input1;
    }
    auto deqStructure = dequantization;
    deqStructure.multiply.outPrecision = inputPrecision;
    const std::shared_ptr<Node> dequantizationOp = makeDequantization(parent, deqStructure);

    std::shared_ptr<op::Parameter> input2 = std::make_shared<ov::opset1::Parameter>(
            inputPrecision,
            inputShape);

    const auto fakeQuantizeOnActivations = ngraph::builder::makeFakeQuantize(
        input2, inputPrecision, 256ul, { 1ul },
        { 0.f }, { 255.f }, { 0.f }, { 255.f });

    // just some non-transparent layer
    const auto power = std::make_shared<ov::opset1::Power>(
        fakeQuantizeOnActivations,
        std::make_shared<ov::opset1::Constant>(inputPrecision, Shape{}, std::vector<float>{2.f}));

    const auto add = std::make_shared<ov::opset1::Add>(
        dequantizationOp,
        power);

    add->set_friendly_name("output");

    auto parameters = constInput ?
                      ov::ParameterVector{ input2 }:
                      ov::ParameterVector{ input1, input2 };

    ov::ResultVector results{ std::make_shared<ov::opset1::Result>(add) };
    return std::make_shared<ov::Model>(results, parameters, "FuseConvertFunction");
}


}  // namespace subgraph
}  // namespace builder
}  // namespace ngraph
