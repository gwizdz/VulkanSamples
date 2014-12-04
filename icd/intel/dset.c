/*
 * XGL
 *
 * Copyright (C) 2014 LunarG, Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *   Chia-I Wu <olv@lunarg.com>
 */

#include "dev.h"
#include "sampler.h"
#include "dset.h"

static bool dset_img_state_read_only(XGL_IMAGE_LAYOUT layout)
{
    switch (layout) {
    case XGL_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case XGL_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    case XGL_IMAGE_LAYOUT_TRANSFER_SOURCE_OPTIMAL:
        return true;
    default:
        return false;
    }
}

static void dset_destroy(struct intel_obj *obj)
{
    struct intel_dset *dset = intel_dset_from_obj(obj);

    intel_dset_destroy(dset);
}

XGL_RESULT intel_dset_create(struct intel_dev *dev,
                             const XGL_DESCRIPTOR_SET_CREATE_INFO *info,
                             struct intel_dset **dset_ret)
{
    struct intel_dset *dset;

    dset = (struct intel_dset *) intel_base_create(dev, sizeof(*dset),
            dev->base.dbg, XGL_DBG_OBJECT_DESCRIPTOR_SET, info, 0);
    if (!dset)
        return XGL_ERROR_OUT_OF_MEMORY;

    dset->dev = dev;
    dset->slots = icd_alloc(sizeof(dset->slots[0]) * info->slots,
            0, XGL_SYSTEM_ALLOC_INTERNAL);
    if (!dset->slots) {
        intel_dset_destroy(dset);
        return XGL_ERROR_OUT_OF_MEMORY;
    }

    dset->obj.destroy = dset_destroy;

    *dset_ret = dset;

    return XGL_SUCCESS;
}

void intel_dset_destroy(struct intel_dset *dset)
{
    icd_free(dset->slots);
    intel_base_destroy(&dset->obj.base);
}

ICD_EXPORT XGL_RESULT XGLAPI xglCreateDescriptorSet(
    XGL_DEVICE                                  device,
    const XGL_DESCRIPTOR_SET_CREATE_INFO*       pCreateInfo,
    XGL_DESCRIPTOR_SET*                         pDescriptorSet)
{
    struct intel_dev *dev = intel_dev(device);

    return intel_dset_create(dev, pCreateInfo,
            (struct intel_dset **) pDescriptorSet);
}

ICD_EXPORT XGL_VOID XGLAPI xglBeginDescriptorSetUpdate(
    XGL_DESCRIPTOR_SET                          descriptorSet)
{
    /* no-op */
}

ICD_EXPORT XGL_VOID XGLAPI xglEndDescriptorSetUpdate(
    XGL_DESCRIPTOR_SET                          descriptorSet)
{
    /* no-op */
}

ICD_EXPORT XGL_VOID XGLAPI xglAttachSamplerDescriptors(
    XGL_DESCRIPTOR_SET                          descriptorSet,
    XGL_UINT                                    startSlot,
    XGL_UINT                                    slotCount,
    const XGL_SAMPLER*                          pSamplers)
{
    struct intel_dset *dset = intel_dset(descriptorSet);
    XGL_UINT i;

    for (i = 0; i < slotCount; i++) {
        struct intel_dset_slot *slot = &dset->slots[startSlot + i];
        struct intel_sampler *sampler = intel_sampler(pSamplers[i]);

        slot->type = INTEL_DSET_SLOT_SAMPLER;
        slot->read_only = true;
        slot->u.sampler = sampler;
    }
}

ICD_EXPORT XGL_VOID XGLAPI xglAttachImageViewDescriptors(
    XGL_DESCRIPTOR_SET                          descriptorSet,
    XGL_UINT                                    startSlot,
    XGL_UINT                                    slotCount,
    const XGL_IMAGE_VIEW_ATTACH_INFO*           pImageViews)
{
    struct intel_dset *dset = intel_dset(descriptorSet);
    XGL_UINT i;

    for (i = 0; i < slotCount; i++) {
        struct intel_dset_slot *slot = &dset->slots[startSlot + i];
        const XGL_IMAGE_VIEW_ATTACH_INFO *info = &pImageViews[i];
        struct intel_img_view *view = intel_img_view(info->view);

        slot->type = INTEL_DSET_SLOT_IMG_VIEW;
        slot->read_only = dset_img_state_read_only(info->layout);
        slot->u.img_view = view;
    }
}

ICD_EXPORT XGL_VOID XGLAPI xglAttachBufferViewDescriptors(
    XGL_DESCRIPTOR_SET                          descriptorSet,
    XGL_UINT                                    startSlot,
    XGL_UINT                                    slotCount,
    const XGL_BUFFER_VIEW_ATTACH_INFO*          pBufferViews)
{
    struct intel_dset *dset = intel_dset(descriptorSet);
    XGL_UINT i;

    for (i = 0; i < slotCount; i++) {
        struct intel_dset_slot *slot = &dset->slots[startSlot + i];
        const XGL_BUFFER_VIEW_ATTACH_INFO *info = &pBufferViews[i];

        slot->type = INTEL_DSET_SLOT_BUF_VIEW;
        slot->read_only = false;
        slot->u.buf_view = intel_buf_view(info->view);
    }
}

ICD_EXPORT XGL_VOID XGLAPI xglAttachNestedDescriptors(
    XGL_DESCRIPTOR_SET                          descriptorSet,
    XGL_UINT                                    startSlot,
    XGL_UINT                                    slotCount,
    const XGL_DESCRIPTOR_SET_ATTACH_INFO*       pNestedDescriptorSets)
{
    struct intel_dset *dset = intel_dset(descriptorSet);
    XGL_UINT i;

    for (i = 0; i < slotCount; i++) {
        struct intel_dset_slot *slot = &dset->slots[startSlot + i];
        const XGL_DESCRIPTOR_SET_ATTACH_INFO *info = &pNestedDescriptorSets[i];
        struct intel_dset *nested = intel_dset(info->descriptorSet);

        slot->type = INTEL_DSET_SLOT_NESTED;
        slot->read_only = true;
        slot->u.nested.dset = nested;
        slot->u.nested.slot_offset = info->slotOffset;
    }
}

ICD_EXPORT XGL_VOID XGLAPI xglClearDescriptorSetSlots(
    XGL_DESCRIPTOR_SET                          descriptorSet,
    XGL_UINT                                    startSlot,
    XGL_UINT                                    slotCount)
{
    struct intel_dset *dset = intel_dset(descriptorSet);
    XGL_UINT i;

    for (i = 0; i < slotCount; i++) {
        struct intel_dset_slot *slot = &dset->slots[startSlot + i];

        slot->type = INTEL_DSET_SLOT_UNUSED;
        slot->read_only = true;
        slot->u.unused = NULL;
    }
}
