/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_queue/dispatch_walker.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"
#include <new>

namespace OCLRT {

template <typename GfxFamily>
struct EnqueueOperation<GfxFamily, CL_COMMAND_NDRANGE_KERNEL> {
    static size_t getSizeRequiredCS(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
        size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + KernelCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                      sizeof(typename GfxFamily::PIPE_CONTROL) * (KernelCommandsHelper<GfxFamily>::isPipeControlWArequired() ? 2 : 1);
        size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(commandQueue.getDevice());
        if (reserveProfilingCmdsSpace) {
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL) + 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        }
        if (reservePerfCounters) {
            //start cmds
            //P_C: flush CS & TimeStamp BEGIN
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL);
            //SRM NOOPID & Frequency
            size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //gp registers
            size += OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //report perf count
            size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
            //user registers
            size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);

            //end cmds
            //P_C: flush CS & TimeStamp END;
            size += 2 * sizeof(typename GfxFamily::PIPE_CONTROL);
            //OA buffer (status head, tail)
            size += 3 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //report perf count
            size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
            //gp registers
            size += OCLRT::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //SRM NOOPID & Frequency
            size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
            //user registers
            size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        }
        size += getSizeForWADisableLSQCROPERFforOCL<GfxFamily>(pKernel);

        return size;
    }
};

template <typename GfxFamily>
cl_int CommandQueueHw<GfxFamily>::enqueueKernel(
    cl_kernel clKernel,
    cl_uint workDim,
    const size_t *globalWorkOffsetIn,
    const size_t *globalWorkSizeIn,
    const size_t *localWorkSizeIn,
    cl_uint numEventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *event) {

    size_t region[3] = {1, 1, 1};
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workGroupSize[3] = {1, 1, 1};

    auto &kernel = *castToObject<Kernel>(clKernel);
    const auto &kernelInfo = kernel.getKernelInfo();

    if (!kernel.isPatched()) {
        if (event) {
            *event = nullptr;
        }

        return CL_INVALID_KERNEL_ARGS;
    }

    if (kernel.isUsingSharedObjArgs()) {
        kernel.resetSharedObjectsPatchAddresses();
    }

    bool haveRequiredWorkGroupSize = false;

    if (kernelInfo.reqdWorkGroupSize[0] != WorkloadInfo::undefinedOffset) {
        haveRequiredWorkGroupSize = true;
    }

    size_t remainder = 0;
    const size_t *localWkgSizeToPass = localWorkSizeIn ? workGroupSize : nullptr;

    for (auto i = 0u; i < workDim; i++) {
        region[i] = globalWorkSizeIn ? globalWorkSizeIn[i] : 0;
        globalWorkOffset[i] = globalWorkOffsetIn
                                  ? globalWorkOffsetIn[i]
                                  : 0;

        if (localWorkSizeIn) {
            if (haveRequiredWorkGroupSize) {
                if (kernelInfo.reqdWorkGroupSize[i] != localWorkSizeIn[i]) {
                    return CL_INVALID_WORK_GROUP_SIZE;
                }
            }
            workGroupSize[i] = localWorkSizeIn[i];
        }

        remainder += region[i] % workGroupSize[i];
    }

    if (remainder != 0 && !kernel.getAllowNonUniform()) {
        return CL_INVALID_WORK_GROUP_SIZE;
    }

    if (haveRequiredWorkGroupSize) {
        localWkgSizeToPass = kernelInfo.reqdWorkGroupSize;
    }

    NullSurface s;
    Surface *surfaces[] = {&s};

    if (context->isProvidingPerformanceHints()) {
        if (kernel.hasPrintfOutput()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, PRINTF_DETECTED_IN_KERNEL, kernel.getKernelInfo().name.c_str());
        }
        if (kernel.requiresCoherency()) {
            context->providePerformanceHint(CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL, KERNEL_REQUIRES_COHERENCY, kernel.getKernelInfo().name.c_str());
        }
    }

    if (kernel.getKernelInfo().builtinDispatchBuilder != nullptr) {
        cl_int err = kernel.getKernelInfo().builtinDispatchBuilder->validateDispatch(&kernel, workDim, Vec3<size_t>(region), Vec3<size_t>(workGroupSize), Vec3<size_t>(globalWorkOffset));
        if (err != CL_SUCCESS)
            return err;
    }

    enqueueHandler<CL_COMMAND_NDRANGE_KERNEL>(
        surfaces,
        false,
        &kernel,
        workDim,
        globalWorkOffset,
        region,
        localWkgSizeToPass,
        numEventsInWaitList,
        eventWaitList,
        event);

    return CL_SUCCESS;
}
} // namespace OCLRT
