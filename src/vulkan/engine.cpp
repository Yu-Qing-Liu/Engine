#include "engine.hpp"
#include "rendering.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>

using namespace Rendering;

// -------------------------------------
// Engine lifecycle
// -------------------------------------
void Engine::init(GLFWwindow *w) {
	window = w;

	try {
		debug = std::make_unique<Debug>();
		surface = std::make_unique<Surface>(debug->getInstance(), window);
		physicalDevice = std::make_unique<PhysicalDevice>(debug->getInstance(), surface->getSurface());
		logicalDevice = std::make_unique<LogicalDevice>(physicalDevice->getPhysicalDevice(), physicalDevice->getQueueFamilies(), requiredDeviceExtensions, enableValidationLayers);
		swapchain = std::make_unique<Swapchain>(physicalDevice->getPhysicalDevice(), logicalDevice->getDevice(), surface->getSurface(), physicalDevice->getQueueFamilies(), window);
		graphicsBuffers = std::make_unique<GraphicsBuffers>();
		graphicsBuffers->create(physicalDevice->getPhysicalDevice(), logicalDevice->getDevice(), swapchain->getExtent(), VK_FORMAT_R16G16B16A16_SFLOAT, static_cast<uint32_t>(swapchain->getImages().size()));

		commandBuffers = std::make_unique<CommandBuffers>();
		commandBuffers->create(logicalDevice->getDevice(), logicalDevice->getGraphicsQueueFamily(), 2);

		synchronization = std::make_unique<Synchronization>();
		synchronization->create(logicalDevice->getDevice(), 2, static_cast<uint32_t>(swapchain->getImages().size()));

		imgui = std::make_unique<DearImGui>();
		imgui->init(w, debug->getInstance(), logicalDevice->getPhysicalDevice(), logicalDevice->getDevice(), logicalDevice->getGraphicsQueueFamily(), logicalDevice->getGraphicsQueue(), swapchain->getImageFormat(), static_cast<uint32_t>(swapchain->getImages().size()), 2);

		currentFrameIndex = 0;
	} catch (const std::exception &e) {
		std::fprintf(stderr, "Engine init failed: %s\n", e.what());
		throw;
	}
}

void Engine::beginImGuiFrame() { imgui->newFrame(); }

// -------------------------------------
// Swapchain recreation / resize
// -------------------------------------

void Engine::recreateSwapchain(Scenes &scenes) {
	// Wait until not minimized (we just use this to block, not to size scenes)
	int w_fb = 0, h_fb = 0;
	do {
		glfwGetFramebufferSize(window, &w_fb, &h_fb);
		glfwWaitEvents();
	} while (w_fb == 0 || h_fb == 0);

	// Make sure nothing is using the old swapchain
	vkDeviceWaitIdle(logicalDevice->getDevice());

	// Recreate swapchain for the new extent
	swapchain->recreate(physicalDevice->getPhysicalDevice(), logicalDevice->getDevice(), surface->getSurface(), physicalDevice->getQueueFamilies(), window);

	// Recreate offscreen/color/depth buffers for the *new* extent
	graphicsBuffers->destroy();
	graphicsBuffers->create(physicalDevice->getPhysicalDevice(), logicalDevice->getDevice(), swapchain->getExtent(), VK_FORMAT_R16G16B16A16_SFLOAT, (uint32_t)swapchain->getImages().size());

	// Recreate sync in case counts changed
	synchronization->destroy();
	synchronization->create(logicalDevice->getDevice(), swapImageCount, (uint32_t)swapchain->getImages().size());

	// ImGui backend swapchain-format update
	imgui->onSwapchainRecreated(swapchain->getImageFormat(), (uint32_t)swapchain->getImages().size(), swapImageCount);

	const VkExtent2D e = swapchain->getExtent();
	scenes.swapChainUpdate(float(e.width), float(e.height), w_fb, h_fb);

	currentFrameIndex = 0;
}

// -------------------------------------
// drawFrame(): multipass blur compositing
// -------------------------------------

void Engine::drawFrame(Scenes &scenes, bool framebufferResizedFlag) {
	VkDevice dev = logicalDevice->getDevice();
	VkQueue gfxQ = logicalDevice->getGraphicsQueue();
	VkQueue presQ = logicalDevice->getPresentQueue();

	// --- CPU-GPU sync for this overlapping frame ---
	VkFence frameFence = synchronization->inFlightFence(currentFrameIndex);
	vkWaitForFences(dev, 1, &frameFence, VK_TRUE, UINT64_MAX);

	// Also ensure last compute using this frame index is finished  // NEW
	VkFence compFence = synchronization->computeFence(currentFrameIndex);
	vkWaitForFences(dev, 1, &compFence, VK_TRUE, UINT64_MAX);

	// --- Acquire swapchain image ---
	uint32_t imageIndex = 0;
	VkSemaphore imageAvail = synchronization->imageAvailable(currentFrameIndex);

	VkResult acq = vkAcquireNextImageKHR(dev, swapchain->getHandle(), UINT64_MAX, imageAvail, VK_NULL_HANDLE, &imageIndex);

	if (acq == VK_ERROR_OUT_OF_DATE_KHR || framebufferResizedFlag) {
		recreateSwapchain(scenes);
		return;
	} else if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Engine::drawFrame: failed to acquire swapchain image");
	}

	// -----------------------------
	// GENERAL COMPUTE (pre-graphics)
	// -----------------------------
	{
		// Reuse per-frame compute cmd
		VkCommandBuffer ccmd = commandBuffers->getComputeCmd(currentFrameIndex);
		vkResetFences(dev, 1, &compFence);
		vkResetCommandBuffer(ccmd, 0);

		VkCommandBufferBeginInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		VK_CHECK(vkBeginCommandBuffer(ccmd, &cbi));

		// Let scenes encode anything: culling, particles, light lists, prefix sums, etc.
		scenes.compute(ccmd);

		VK_CHECK(vkEndCommandBuffer(ccmd));

		// Submit to compute queue, signal compute-finished semaphore & fence
		VkSemaphore compDone = synchronization->computeFinished(currentFrameIndex);

		VkSubmitInfo csubmit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		csubmit.commandBufferCount = 1;
		csubmit.pCommandBuffers = &ccmd;
		csubmit.signalSemaphoreCount = 1;
		csubmit.pSignalSemaphores = &compDone;

		VK_CHECK(vkQueueSubmit(logicalDevice->getComputeQueue(), 1, &csubmit, compFence));
	}

	vkWaitForFences(dev, 1, &compFence, VK_TRUE, UINT64_MAX);

	// We'll re-record this frame's command buffer
	VkCommandBuffer cmd = commandBuffers->getGraphicsCmd(currentFrameIndex);

	vkResetFences(dev, 1, &frameFence);
	vkResetCommandBuffer(cmd, 0);

	// We describe state of "src" and "dst" accumulation targets.
	// We'll ping-pong them as we go through blur layers.
	struct AccumState {
		VkImage img;
		VkImageView attView;	 // mip0 attachment view
		VkImageView sampleView;	 // full-mip SRV view
		VkDescriptorSet descSet; // already bound to sampleView+sampler
	};
	auto makeA = [&](uint32_t idx) -> AccumState { return AccumState{graphicsBuffers->getColorAImage(idx), graphicsBuffers->getColorAAttView(idx), graphicsBuffers->getColorASampleView(idx), graphicsBuffers->getSceneSetA(idx)}; };
	auto makeB = [&](uint32_t idx) -> AccumState { return AccumState{graphicsBuffers->getColorBImage(idx), graphicsBuffers->getColorBAttView(idx), graphicsBuffers->getColorBSampleView(idx), graphicsBuffers->getSceneSetB(idx)}; };

	AccumState accumA = makeA(imageIndex);
	AccumState accumB = makeB(imageIndex);

	// we will treat accumDst = accumA, accumSrc = accumB initially,
	// but it's arbitrary. Let's pick:
	AccumState accumSrc = accumA; // "what has been composited so far"
	AccumState accumDst = accumB; // "where we'll render next"
	// after bootstrap opaque pass we will flip, etc.

	VkImage depthImg = graphicsBuffers->getDepthImage();
	VkImageView depthView = graphicsBuffers->getDepthView();

	const VkExtent2D extent = swapchain->getExtent();
	const uint32_t mipLevels = graphicsBuffers->getMipLevels();

	// convenience lambdas for swapping and for depth layout ensure

	auto swapAccum = [&](void) {
		AccumState tmp = accumSrc;
		accumSrc = accumDst;
		accumDst = tmp;
	};

	// record command buffer
	{
		VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		bi.flags = 0;
		VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

		//-----------------------------------------
		// 0) Prep depth to DEPTH_ATTACHMENT_OPTIMAL
		//-----------------------------------------
		cmdTransitionImage(cmd, depthImg, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

		//-----------------------------------------
		// 1) OPAQUE / BACKGROUND BOOTSTRAP
		//    We render opaque world into accumDst.mip0, clearing color+depth.
		//-----------------------------------------
		// accumDst.mip0: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
		cmdTransitionImage(cmd, accumDst.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

		// Begin rendering opaque world.
		{
			VkClearColorValue clrBlack{};
			clrBlack.float32[0] = 0.f;
			clrBlack.float32[1] = 0.f;
			clrBlack.float32[2] = 0.f;
			clrBlack.float32[3] = 0.f;

			VkClearDepthStencilValue clrDepth{};
			clrDepth.depth = 1.0f;
			clrDepth.stencil = 0;

			cmdBeginRenderingColorDepth(cmd, accumDst.attView, depthView, extent, clrBlack, clrDepth,
										/*colorLoad*/ VK_ATTACHMENT_LOAD_OP_CLEAR,
										/*colorStore*/ VK_ATTACHMENT_STORE_OP_STORE,
										/*depthLoad*/ VK_ATTACHMENT_LOAD_OP_CLEAR,
										/*depthStore*/ VK_ATTACHMENT_STORE_OP_STORE);

			// opaque scene draw call(s)
			scenes.record(cmd);
			vkCmdEndRendering(cmd);
		}

		// Now accumDst.mip0 has opaque+depth. We want accumDst to become "src" for next steps.
		// For mipgen we expect level0 in COLOR_ATTACHMENT_OPTIMAL.
		// We'll build mips and leave accumDst in SHADER_READ_ONLY_OPTIMAL.
		cmdBuildMipsForImageRuntime(cmd, accumDst.img, extent.width, extent.height, mipLevels);

		// After mip build, accumDst is SHADER_READ_ONLY_OPTIMAL.
		// For future blits we may have to TRANSFER_SRC_OPTIMAL again, but see below.

		// So after opaque stage, the fully composited scene so far is in accumDst.
		// Make that the "src".
		swapAccum(); // accumSrc now has opaque scene in SHADER_READ_ONLY_OPTIMAL

		//-----------------------------------------
		// 2) TRANSLUCENT / BLUR LAYERS
		// We'll render each blur layer back-to-front.
		//-----------------------------------------

		for (uint32_t layerIdx = 0; layerIdx < blurLayerCount; ++layerIdx) {
			// Before we can copy accumSrc.mip0 into accumDst.mip0, we need accumSrc.mip0 usable as TRANSFER_SRC.
			// Right now after mip build accumSrc was SHADER_READ_ONLY_OPTIMAL.
			// We'll do:
			//   SHADER_READ_ONLY_OPTIMAL -> TRANSFER_SRC_OPTIMAL (only mip0 is needed for blit)
			cmdTransitionImage(cmd, accumSrc.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

			// Copy src.mip0 -> dst.mip0, then dst goes COLOR_ATTACHMENT_OPTIMAL
			cmdCopyBaseMipToDstAndMakeColorAttachment(cmd, accumSrc.img, accumDst.img, extent.width, extent.height);

			// Make sure depth is already DEPTH_ATTACHMENT_OPTIMAL (it is).
			// We are doing a LOAD on color (because dst already has copy of background),
			// and LOAD on depth (keep same depth buffer, no clear).
			{
				VkClearColorValue dummyClr{};
				dummyClr.float32[0] = 0.f;
				dummyClr.float32[1] = 0.f;
				dummyClr.float32[2] = 0.f;
				dummyClr.float32[3] = 0.f;

				VkClearDepthStencilValue dummyDepth{};
				dummyDepth.depth = 1.0f;
				dummyDepth.stencil = 0;

				cmdBeginRenderingColorDepth(cmd, accumDst.attView, depthView, extent, dummyClr, dummyDepth,
											/*colorLoad*/ VK_ATTACHMENT_LOAD_OP_LOAD,
											/*colorStore*/ VK_ATTACHMENT_STORE_OP_STORE,
											/*depthLoad*/ VK_ATTACHMENT_LOAD_OP_LOAD,
											/*depthStore*/ VK_ATTACHMENT_STORE_OP_STORE);

				// Draw all objects in this blur layer.
				// They sample accumSrc via descriptor set (accumSrc.descSet),
				// depth test ON, depthWrite OFF, alpha blend ON in the bound pipeline.
				scenes.recordUI(cmd, layerIdx);
				vkCmdEndRendering(cmd);
			}

			// After we're done drawing into accumDst, accumDst.mip0 is in COLOR_ATTACHMENT_OPTIMAL.
			// Build its mip chain so future layers can blur it.
			cmdBuildMipsForImageRuntime(cmd, accumDst.img, extent.width, extent.height, mipLevels);

			// Swap roles so next layer sees new composite as src.
			swapAccum();
		}

		//-----------------------------------------
		// 3) FINAL COMPOSITE + IMGUI to swapchain image
		//-----------------------------------------
		// We assume accumSrc currently holds final composited scene in
		// SHADER_READ_ONLY_OPTIMAL (after its last mip build).
		// We'll render fullscreen quad + imgui into swapchain img.

		VkImage swapImg = swapchain->getImages()[imageIndex];

		// accumSrc.img: SHADER_READ_ONLY_OPTIMAL -> TRANSFER_SRC_OPTIMAL (only mip0)
		cmdTransitionImage(cmd, accumSrc.img, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);

		// swapImg: UNDEFINED -> TRANSFER_DST_OPTIMAL
		cmdTransitionImage(cmd, swapImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 1, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);

		// blit base level
		VkImageBlit2 blit{VK_STRUCTURE_TYPE_IMAGE_BLIT_2};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = 0;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {int(extent.width), int(extent.height), 1};

		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = 0;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {int(extent.width), int(extent.height), 1};

		VkBlitImageInfo2 bi2{VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2};
		bi2.srcImage = accumSrc.img;
		bi2.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		bi2.dstImage = swapImg;
		bi2.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		bi2.filter = VK_FILTER_NEAREST;
		bi2.regionCount = 1;
		bi2.pRegions = &blit;

		vkCmdBlitImage2(cmd, &bi2);

		// swapImg: TRANSFER_DST_OPTIMAL -> PRESENT_SRC_KHR
		cmdTransitionImage(cmd, swapImg, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, 0, 1, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);

		VK_CHECK(vkEndCommandBuffer(cmd));
	}

	// --- Submit graphics queue work ---
	VkSemaphore waitSems[] = {synchronization->imageAvailable(currentFrameIndex), synchronization->computeFinished(currentFrameIndex)};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT};

	VkSemaphore signalSems[] = {synchronization->renderFinishedForImage(imageIndex)};

	VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
	submit.waitSemaphoreCount = (uint32_t)(sizeof(waitSems) / sizeof(waitSems[0]));
	submit.pWaitSemaphores = waitSems;
	submit.pWaitDstStageMask = waitStages;
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;
	submit.signalSemaphoreCount = (uint32_t)(sizeof(signalSems) / sizeof(signalSems[0]));
	submit.pSignalSemaphores = signalSems;

	VK_CHECK(vkQueueSubmit(gfxQ, 1, &submit, synchronization->inFlightFence(currentFrameIndex)));

	// --- Present ---
	VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	VkSwapchainKHR sc = swapchain->getHandle();

	VkSemaphore presentWait[] = {synchronization->renderFinishedForImage(imageIndex)};
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = presentWait;
	present.swapchainCount = 1;
	present.pSwapchains = &sc;
	present.pImageIndices = &imageIndex;
	present.pResults = nullptr;

	VkResult presRes = vkQueuePresentKHR(presQ, &present);
	if (presRes == VK_ERROR_OUT_OF_DATE_KHR || presRes == VK_SUBOPTIMAL_KHR || framebufferResizedFlag) {
		recreateSwapchain(scenes);
	} else if (presRes != VK_SUCCESS) {
		throw std::runtime_error("Engine::drawFrame: present failed");
	}

	// advance frame overlap index
	currentFrameIndex = (currentFrameIndex + 1) % 2; // frameOverlap == 2
}
