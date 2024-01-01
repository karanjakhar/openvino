// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "acl_convert.hpp"
#include "acl_utils.hpp"

namespace ov {
namespace intel_cpu {

using namespace arm_compute;


bool ACLConvertExecutor::init(const ConvertParams& convertParams,
                              const MemoryDescPtr& srcDesc,
                              const MemoryDescPtr& dstDesc,
                              const dnnl::primitive_attr& attr) {
    aclConvertParams = convertParams;

    auto srcPrecision = precisionToAclDataType(aclConvertParams.srcPrc);
    auto dstPrecision = precisionToAclDataType(aclConvertParams.dstPrc);
    isCopyOp = aclConvertParams.srcPrc == aclConvertParams.dstPrc;
    // NECast does not support S8. It could be replaced with QASYMM8_SIGNED
    if (!isCopyOp && srcPrecision == DataType::S8) {
        srcPrecision = DataType::QASYMM8_SIGNED;
    }
    if (!isCopyOp && dstPrecision == DataType::S8) {
        dstPrecision = DataType::QASYMM8_SIGNED;
    }
    auto srcDims = srcDesc->getShape().getStaticDims();
    auto dstDims = dstDesc->getShape().getStaticDims();
    auto srcDataLayout = getAclDataLayoutByMemoryDesc(srcDesc);
    auto dstDataLayout = getAclDataLayoutByMemoryDesc(dstDesc);
    auto srcTensorInfo = TensorInfo(shapeCast(collapse_dims_to_max_rank(srcDims)), 1, srcPrecision, srcDataLayout);
    auto dstTensorInfo = TensorInfo(shapeCast(collapse_dims_to_max_rank(dstDims)), 1, dstPrecision, dstDataLayout);
    if (isCopyOp) {
        Status s = NECopy::validate(&srcTensorInfo, &dstTensorInfo);
        if (!s) {
            DEBUG_LOG("NECopy validation failed: ", s.error_description());
            return false;
        }
    } else {
        Status s = NECast::validate(&srcTensorInfo, &dstTensorInfo, ConvertPolicy::SATURATE);
        if (!s) {
            DEBUG_LOG("NECast validation failed: ", s.error_description());
            return false;
        }
    }

    srcTensor.allocator()->init(srcTensorInfo);
    dstTensor.allocator()->init(dstTensorInfo);

    if (isCopyOp) {
        acl_copy = std::make_unique<NECopy>();
        acl_copy->configure(&srcTensor, &dstTensor);
    } else {
        acl_cast = std::make_unique<NECast>();
        acl_cast->configure(&srcTensor, &dstTensor, ConvertPolicy::SATURATE);
    }
    return true;
}

void ACLConvertExecutor::exec(const MemoryCPtr& src, const MemoryPtr& dst) {
    srcTensor.allocator()->import_memory(src->getData());
    dstTensor.allocator()->import_memory(dst->getData());

    if (isCopyOp) {
        acl_copy->run();
    } else {
        acl_cast->run();
    }

    srcTensor.allocator()->free();
    dstTensor.allocator()->free();
}

bool ACLConvertExecutorBuilder::isSupported(const ConvertParams& convertParams,
                                            const MemoryDescPtr& srcDesc,
                                            const MemoryDescPtr& dstDesc) const {
    if (convertParams.srcPrc != convertParams.dstPrc) {
        if (!one_of(convertParams.srcPrc,
                    ov::element::i8,
                    ov::element::u8,
                    ov::element::u16,
                    ov::element::i16,
                    ov::element::f16,
                    ov::element::i32,
                    ov::element::f32)) {
            DEBUG_LOG("NECopy does not support source precision: ", convertParams.srcPrc.to_string());
            return false;
        }
        if ((convertParams.srcPrc == ov::element::i8 && !one_of(convertParams.dstPrc,
                                                              ov::element::i16,
                                                              ov::element::i32,
                                                              ov::element::f16,
                                                              ov::element::f32)) ||
            (convertParams.srcPrc == ov::element::u8 && !one_of(convertParams.dstPrc,
                                                              ov::element::u16,
                                                              ov::element::i16,
                                                              ov::element::i32,
                                                              ov::element::f16,
                                                              ov::element::f32)) ||
            (convertParams.srcPrc == ov::element::u16 && !one_of(convertParams.dstPrc,
                                                               ov::element::u8,
                                                               ov::element::u32)) ||
            (convertParams.srcPrc == ov::element::i16 && !one_of(convertParams.dstPrc,
                                                               ov::element::i8,
                                                               ov::element::u8,
                                                               ov::element::i32)) ||
            (convertParams.srcPrc == ov::element::f16 && !one_of(convertParams.dstPrc,
                                                                ov::element::i8,
                                                                ov::element::f32,
                                                                ov::element::i32,
                                                                ov::element::u8)) ||
            (convertParams.srcPrc == ov::element::i32 && !one_of(convertParams.dstPrc,
                                                               ov::element::i8,
                                                               ov::element::f16,
                                                               ov::element::f32,
                                                               ov::element::u8)) ||
            (convertParams.srcPrc == ov::element::f32 && !one_of(convertParams.dstPrc,
                                                                ov::element::bf16,
                                                                ov::element::f16,
                                                                ov::element::i32))) {
            DEBUG_LOG("NECopy does not support passed combination of source and destination precisions. ",
                      "source precision: ", convertParams.srcPrc.to_string(), " destination precsion: ", convertParams.dstPrc.to_string());
            return false;
        }
    }
    return true;
}

} // namespace intel_cpu
} // namespace ov