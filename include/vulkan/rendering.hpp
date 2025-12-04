#pragma once

#include <vulkan/vulkan_core.h>

namespace Rendering {
// generic layout / access transition for a whole subresource range
inline static void cmdTransitionImage(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMip, uint32_t levelCount, VkPipelineStageFlags2 srcStageMask, VkAccessFlags2 srcAccessMask, VkPipelineStageFlags2 dstStageMask, VkAccessFlags2 dstAccessMask) {
	VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
	barrier.srcStageMask = srcStageMask;
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstStageMask = dstStageMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = baseMip;
	barrier.subresourceRange.levelCount = levelCount;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
	dep.dependencyFlags = 0;
	dep.imageMemoryBarrierCount = 1;
	dep.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(cmd, &dep);
}

// Runtime mip-chain builder for a single RGBA16F color attachment texture.
// Assumptions:
// - level0 currently in COLOR_ATTACHMENT_OPTIMAL (freshly rendered OR copied).
// - we produce all lower mips with blits.
// - at the end, we transition the whole chain to SHADER_READ_ONLY_OPTIMAL.
//
// NOTE: we do NOT assume VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT -> TRANSFER
//       for every mip, we pipeline barrier explicitly.
inline static void cmdBuildMipsForImageRuntime(VkCommandBuffer cmd, VkImage image, uint32_t w, uint32_t h, uint32_t mipLevels) {
	if (mipLevels <= 1)
		return;

	// Transition level0 COLOR_ATTACHMENT_OPTIMAL -> TRANSFER_SRC_OPTIMAL
	cmdTransitionImage(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

	int32_t mipW = (int32_t)w;
	int32_t mipH = (int32_t)h;

	for (uint32_t level = 1; level < mipLevels; ++level) {
		// Transition this dst mip from UNDEFINED -> TRANSFER_DST_OPTIMAL
		cmdTransitionImage(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, level, 1, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

		VkImageBlit blit{};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = level - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mipW, mipH, 1};

		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = level;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {std::max(1, mipW / 2), std::max(1, mipH / 2), 1};

		vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		// After we fill this mip, mark it TRANSFER_SRC_OPTIMAL for next loop level
		cmdTransitionImage(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, level, 1, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

		mipW = std::max(1, mipW / 2);
		mipH = std::max(1, mipH / 2);
	}

	// final all-mips → SHADER_READ_ONLY_OPTIMAL
	cmdTransitionImage(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, mipLevels, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
}

// Copy src.mip0 -> dst.mip0 before we start drawing new translucent layer.
// We'll:
//   dst: UNDEFINED -> TRANSFER_DST_OPTIMAL
//   vkCmdBlitImage src->dst
//   dst: TRANSFER_DST_OPTIMAL -> COLOR_ATTACHMENT_OPTIMAL
//
// After this, dst.mip0 has previous composite and is ready for LOAD blending.
inline void cmdCopyBaseMipToDstAndMakeColorAttachment(VkCommandBuffer cmd, VkImage srcImg, VkImage dstImg, uint32_t w, uint32_t h) {
	// prepare dst for copy
	cmdTransitionImage(cmd, dstImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

	VkImageBlit blit{};
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {(int32_t)w, (int32_t)h, 1};

	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;
	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {(int32_t)w, (int32_t)h, 1};

	vkCmdBlitImage(cmd, srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // we guarantee src is TRANSFER_SRC_OPTIMAL or SHADER_READ_ONLY_OPTIMAL ??? wait:
																	  // after cmdBuildMipsForImageRuntime we left src in SHADER_READ_ONLY_OPTIMAL,
																	  // but SHADER_READ_ONLY_OPTIMAL is *not* valid for vkCmdBlitImage source.
																	  // So before calling this we either:
																	  //    - do the blit BEFORE final transition in cmdBuildMipsForImageRuntime, or
																	  //    - here we transition srcImg SHADER_READ_ONLY_OPTIMAL -> TRANSFER_SRC_OPTIMAL (mip0 only),
																	  // then back.
																	  //
																	  // We'll handle that just above/below in the caller.
				   dstImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

	// dst -> COLOR_ATTACHMENT_OPTIMAL
	cmdTransitionImage(cmd, dstImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
}

// Begin dynamic rendering to a color attachment (and optional depth).
// loadOp configurable: for opaque bootstrap we CLEAR, for translucent layers we LOAD.
inline void cmdBeginRenderingColorDepth(VkCommandBuffer cmd, VkImageView colorAttView, VkImageView depthView, VkExtent2D extent, VkClearColorValue clearColor, VkClearDepthStencilValue clearDepth, VkAttachmentLoadOp colorLoad, VkAttachmentStoreOp colorStore, VkAttachmentLoadOp depthLoad, VkAttachmentStoreOp depthStore) {
	// color attachment info
	VkRenderingAttachmentInfo colorAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	colorAtt.imageView = colorAttView;
	colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAtt.loadOp = colorLoad;
	colorAtt.storeOp = colorStore;
	VkClearValue cv{};
	cv.color = clearColor;
	colorAtt.clearValue = cv;

	// depth (can be null if not needed)
	VkRenderingAttachmentInfo depthAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	depthAtt.imageView = depthView;
	depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAtt.loadOp = depthLoad;
	depthAtt.storeOp = depthStore;
	VkClearValue dv{};
	dv.depthStencil = clearDepth;
	depthAtt.clearValue = dv;

	VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
	ri.renderArea.offset = {0, 0};
	ri.renderArea.extent = extent;
	ri.layerCount = 1;
	ri.colorAttachmentCount = 1;
	ri.pColorAttachments = &colorAtt;
	ri.pDepthAttachment = &depthAtt;
	ri.pStencilAttachment = nullptr;

	vkCmdBeginRendering(cmd, &ri);
}

// Same as above but color-only (for final composite to swapchain)
inline void cmdBeginRenderingColorOnly(VkCommandBuffer cmd, VkImageView colorAttView, VkExtent2D extent, VkClearColorValue clr, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp) {
	VkRenderingAttachmentInfo colorAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
	colorAtt.imageView = colorAttView;
	colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAtt.loadOp = loadOp;
	colorAtt.storeOp = storeOp;
	VkClearValue cv{};
	cv.color = clr;
	colorAtt.clearValue = cv;

	VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
	ri.renderArea.offset = {0, 0};
	ri.renderArea.extent = extent;
	ri.layerCount = 1;
	ri.colorAttachmentCount = 1;
	ri.pColorAttachments = &colorAtt;
	ri.pDepthAttachment = nullptr;
	ri.pStencilAttachment = nullptr;

	vkCmdBeginRendering(cmd, &ri);
}
} // namespace Rendering
