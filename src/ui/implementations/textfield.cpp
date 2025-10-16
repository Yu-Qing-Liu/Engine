#include "textfield.hpp"
#include <algorithm>

TextField::TextField(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass) : textParams(textParams), Widget(scene, mvp, screenParams, renderPass) {
	textModel = std::make_unique<Text>(scene, mvp, screenParams, textParams, renderPass);

	auto barElements = std::make_shared<std::unordered_map<int, InstancedRectangleData>>(4);
	scrollBar = make_unique<InstancedRectangle>(scene, mvp, sp, barElements, 4, renderPass);
	scrollBar->enableRayTracing(true);
	scrollBar->setOnMouseClick([&](int button, int action, int mods) {
		if (!enableSlider) {
			return;
		}
		if (action == Events::ACTION_PRESS && button == Events::MOUSE_BUTTON_LEFT) {
			int id = scrollBar->rayTracing->hitMapped->primId;
			if (id == 1) {
				usingSlider = true;
				slider.color = params.sliderColorPressed;
			}
		}
	});
	Events::mouseCallbacks.push_back([&](int button, int action, int mods) {
		if (!enableSlider) {
			return;
		}
		if (action == Events::ACTION_RELEASE && button == Events::MOUSE_BUTTON_LEFT) {
			slider.color = params.sliderColor;
			usingSlider = false;
		}
	});
}

void TextField::updateScreenParams() {
	sp.viewport = screenParams.viewport; // this is your *grid* viewport (logical units)

	// --- inner rect in GRID (logical) units ---
	const float fieldL = params.center.x; // top-left, Y down
	const float fieldT = params.center.y;

	const float innerL0 = fieldL + params.padding.x;
	const float innerT0 = fieldT + params.padding.y;
	const float innerW0 = std::max(0.0f, params.dim.x - (params.padding.x + params.padding.z));
	const float innerH0 = std::max(0.0f, params.dim.y - (params.padding.y + params.padding.w));

	const float barW = showScrollBar ? params.scrollBarWidth : 0.0f;
	const float innerW = std::max(0.0f, innerW0 - barW);
	const float innerH = innerH0;

	// Clamp inner box to the *grid* viewport (still logical units)
	const float vpL = 0.0f, vpT = 0.0f;
	const float vpR = sp.viewport.width;
	const float vpB = sp.viewport.height;

	const float innerR = std::min(innerL0 + innerW, vpR);
	const float innerB = std::min(innerT0 + innerH, vpB);
	const float innerL = std::max(innerL0, vpL);
	const float innerT = std::max(innerT0, vpT);

	const float iW_grid = std::max(0.0f, innerR - innerL);
	const float iH_grid = std::max(0.0f, innerB - innerT);

	if (!(params.crop && iW_grid > 0.0f && iH_grid > 0.0f)) {
		sp.scissor = screenParams.scissor;
	} else {
		// --- map GRID → FRAMEBUFFER per-axis ---
		const VkViewport fbViewport = screenParams.viewport; // (x,y,width,height in FB px)

		// Scale factors from grid to framebuffer **per axis**
		const float sx = fbViewport.width / sp.viewport.width;
		const float sy = fbViewport.height / sp.viewport.height; // this is your “~2.2”

		// Map grid-inner rect into FB pixels (top-left anchored)
		const float fbX = fbViewport.x + innerL * sx;
		const float fbY = fbViewport.y + innerT * sy;
		const float fbW = iW_grid * sx;
		const float fbH = iH_grid * sy;

		// Clamp to the *framebuffer* (or to fbViewport rectangle, either is fine)
		const float raL = fbViewport.x;
		const float raT = fbViewport.y;
		const float raR = fbViewport.x + fbViewport.width;
		const float raB = fbViewport.y + fbViewport.height;

		const float scL = std::max(fbX, raL);
		const float scT = std::max(fbY, raT);
		const float scR = std::min(fbX + fbW, raR);
		const float scB = std::min(fbY + fbH, raB);

		const float scW = std::max(0.0f, scR - scL);
		const float scH = std::max(0.0f, scB - scT);

		sp.scissor.offset = {(int32_t)std::floor(scL), (int32_t)std::floor(scT)};
		sp.scissor.extent = {(uint32_t)std::ceil(scW), (uint32_t)std::ceil(scH)};
	}

	// ----- Outer (grid + margins) -> bgSp -----
	const float swapW = screenParams.viewport.width;
	const float swapH = screenParams.viewport.height;

	const float mL = params.margins.x;
	const float mT = params.margins.y;
	const float mR = params.margins.z;
	const float mB = params.margins.w;

	const float desiredX = params.center.x;
	const float desiredY = params.center.y;
	const float desiredW = params.dim.x;
	const float desiredH = params.dim.y;

	const float ex = desiredX - mL;
	const float ey = desiredY - mT;
	const float eW = desiredW + mL + mR;
	const float eH = desiredH + mT + mB;

	const float bix = std::max(0.0f, ex);
	const float biy = std::max(0.0f, ey);
	const float bix2 = std::min(ex + eW, swapW);
	const float biy2 = std::min(ey + eH, swapH);
	const float bW = std::max(0.0f, bix2 - bix);
	const float bH = std::max(0.0f, biy2 - biy);

	bgSp.viewport.x = bix;
	bgSp.viewport.y = biy;
	bgSp.viewport.width = bW;
	bgSp.viewport.height = bH;
	bgSp.viewport.minDepth = sp.viewport.minDepth; // or 0.0f
	bgSp.viewport.maxDepth = sp.viewport.maxDepth; // or 1.0f
	bgSp.scissor.offset = {(int32_t)std::floor(bix), (int32_t)std::floor(biy)};
	bgSp.scissor.extent = {(uint32_t)std::floor(bW), (uint32_t)std::floor(bH)};
}

void TextField::setCaretFromWrappedCpIndex(size_t cpIndex) {
	// If we don’t yet have a cp→byte table (first frame), defer: caller can retry next frame.
	if (paraCpIndexToParaByte.empty()) {
		caretFromWrapped = false;
		return;
	}
	// Clamp to sentinel range and convert to wrapped byte offset
	size_t k = std::min(cpIndex, paraCpIndexToParaByte.size() - 1);
	size_t paraByte = paraCpIndexToParaByte[k];
	// Set caret directly in wrapped space
	if (textModel) {
		textModel->textParams.caret.byte = std::min(paraByte, textModel->textParams.text.size());
	}
	caretFromWrapped = true;
}

void TextField::wrap() {
	const std::string &src = params.text;
	const float maxW = params.dim.x - (showScrollBar ? params.scrollBarWidth : 0.0f) - params.padding.x - params.padding.z;

	// Font metrics (fixed-size font)
	const float lineH = textModel->getPixelHeight();
	const float stepY = lineH + params.lineSpacing;

	auto width_of = [&](size_t a, size_t b) -> float {
		if (b <= a)
			return 0.0f;
		return textModel->getPixelWidth(src.substr(a, b - a)); // [a, b)
	};

	// 1) Spans in ORIGINAL text
	std::vector<std::pair<size_t, size_t>> spans;
	if (src.empty() || maxW <= 0.0f) {
		spans.emplace_back(0, 0);
	} else {
		const size_t nsrc = src.size();
		size_t lineStart = 0, i = 0;
		size_t lastSpace = std::string::npos;
		while (i < nsrc) {
			const char c = src[i];
			if (c == '\n') {
				spans.emplace_back(lineStart, i);
				++i;
				lineStart = i;
				lastSpace = std::string::npos;
				continue;
			}
			float w = width_of(lineStart, i + 1);
			if (w <= maxW) {
				if (c == ' ' || c == '\t')
					lastSpace = i;
				++i;
				continue;
			}
			if (lastSpace != std::string::npos && lastSpace >= lineStart) {
				spans.emplace_back(lineStart, lastSpace);
				i = lastSpace + 1;
			} else {
				if (i == lineStart) {
					spans.emplace_back(lineStart, lineStart + 1);
					++i;
				} else {
					spans.emplace_back(lineStart, i);
				}
			}
			lineStart = i;
			lastSpace = std::string::npos;
		}
		if (lineStart <= nsrc)
			spans.emplace_back(lineStart, nsrc);
	}

	// 2) Build wrapped paragraph + maps
	const size_t n = src.size();
	std::string para;
	para.reserve(src.size() + spans.size());

	// forward map (orig->para)
	mapOrigToParaByte.assign(n + 1, 0);
	// reverse map (para->orig)
	mapParaByteToOrig.clear();
	mapParaByteToOrig.reserve(n + spans.size() + 1);
	auto push_rev = [&](size_t pb, size_t ob) {
		if (pb >= mapParaByteToOrig.size())
			mapParaByteToOrig.resize(pb + 1);
		mapParaByteToOrig[pb] = ob;
	};

	const size_t origCaret = textModel->textParams.caret.byte; // ORIGINAL coords

	for (size_t idx = 0; idx < spans.size(); ++idx) {
		const auto [a, b] = spans[idx];
		size_t e = b;
		while (e > a) {
			char ch = src[e - 1];
			if (ch == ' ' || ch == '\t' || ch == '\r')
				--e;
			else
				break;
		}
		const size_t basePara = para.size();
		for (size_t p = a; p <= e; ++p) {
			const size_t pb = basePara + (p - a);
			mapOrigToParaByte[p] = pb;
			push_rev(pb, p);
		}
		if (e > a)
			para.append(src.data() + a, e - a);

		const bool willNL = (idx + 1 < spans.size());
		const size_t snapBeforeNL = para.size();
		const size_t snapAfterNL = willNL ? (para.size() + 1) : para.size();
		for (size_t p = e; p <= b; ++p)
			mapOrigToParaByte[p] = snapBeforeNL;
		if (willNL) {
			push_rev(para.size(), b);
			para.push_back('\n');
			for (size_t p = e; p <= b; ++p)
				mapOrigToParaByte[p] = snapAfterNL;
		}
	}

	// 3) Build cp-index -> para-byte
	paraCpIndexToParaByte.clear();
	{
		auto is_cont = [](unsigned char B) { return (B & 0xC0) == 0x80; };
		auto cp_len = [&](unsigned char lead) -> size_t {
			if (lead < 0x80u)
				return 1;
			if ((lead >> 5) == 0x6)
				return 2;
			if ((lead >> 4) == 0xE)
				return 3;
			if ((lead >> 3) == 0x1E)
				return 4;
			return 1;
		};
		size_t byte = 0;
		while (byte < para.size()) {
			paraCpIndexToParaByte.push_back(byte);
			unsigned char lead = static_cast<unsigned char>(para[byte]);
			size_t len = cp_len(lead);
			byte = std::min(byte + len, para.size());
			while (byte < para.size() && is_cont(static_cast<unsigned char>(para[byte])))
				++byte;
		}
		paraCpIndexToParaByte.push_back(para.size()); // sentinel
	}

	// 4) Configure text model
	const float desiredX = params.center.x + params.padding.x;
	const float desiredY = params.center.y + params.padding.y;
	textModel->textParams.text = std::move(para);
	textModel->textParams.color = params.textColor;
	textModel->textParams.origin = glm::vec3(desiredX, desiredY + lineH, 0.0f);
	textModel->textParams.lineAdvancePx = (params.lineSpacing > 0.0f) ? stepY : lineH;

	// 5) Caret: if override says caret is already PARA bytes, keep it; else remap from ORIGINAL
	{
		const size_t paraN = textModel->textParams.text.size();
		if (!caretFromWrapped) {
			size_t newCaretPara = (origCaret <= n) ? mapOrigToParaByte[origCaret] : paraN;
			if (newCaretPara > paraN)
				newCaretPara = paraN;
			textModel->textParams.caret.byte = newCaretPara;
		} else {
			textModel->textParams.caret.byte = std::min(textModel->textParams.caret.byte, paraN);
		}
	}

	// 6) Height/scroll limits
	const float advance = (params.lineSpacing > 0.0f) ? (lineH + params.lineSpacing) : lineH;
	size_t lines = 1;
	for (char c : textModel->textParams.text)
		if (c == '\n')
			++lines;
	contentH = lines * advance;

	const float fieldH = params.dim.y;
	scrollMinY = 0.0f;
	scrollMaxY = std::max(0.0f, contentH - fieldH);
}

void TextField::viewTop() {
	const float topY = scrollMinY;
	lookAtCoords.y = topY;
	camPosOrtho.y = topY;
	camTarget.y = topY;
}

void TextField::viewBottom() {
	const float bottomY = scrollMaxY;
	lookAtCoords.y = bottomY;
	camPosOrtho.y = bottomY;
	camTarget.y = bottomY;
}

void TextField::createScrollBar() {
	const float sbW = params.scrollBarWidth;
	const float btnH = params.scrollBarWidth;

	// Keep your X placement as-is: right edge of the field
	const float gw_pos = params.center.x + params.dim.x; // field right edge (screen/grid space)
	const float fieldH = params.dim.y;
	const float fieldTop = params.center.y; // screen-space top of the field
	const float yBase = fieldTop;			// convenience alias

	// Y padding (top/bottom) — makes the track shorter than the field
	const float padTop = params.scrollBarOffset * 0.5f;
	const float padBot = params.scrollBarOffset * 0.5f;

	// Content scroll limits (unchanged)
	scrollMinY = 0.0f;
	scrollMaxY = std::max(0.0f, contentH - fieldH);

	// Track geometry (shorter, with padding)
	const float trackTop = yBase + padTop;			   // top of track in screen space
	trackH = std::max(0.0f, fieldH - padTop - padBot); // <— shortened height
	const float trackCY = trackTop + trackH * 0.5f;	   // center Y of track

	// Track X is the center of the narrow bar at the field's right edge
	const float trackCX = gw_pos - sbW;
	trackX = trackCX; // keep for thumb/drag logic

	// Track (idx 0) — NOTE: scale Y uses trackH (no '* 2.0f')
	InstancedRectangleData r0{};
	r0.color = Colors::Gray(0.05f);
	r0.borderRadius = 6.0f;
	r0.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackCX, trackCY, -0.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, trackH, 1.0f));
	scrollBar->updateInstance(0, r0);

	// Up button (idx 2) — anchored inside the padded track
	InstancedRectangleData r2{};
	r2.color = Colors::Gray(0.35f);
	r2.borderRadius = 6.0f;
	r2.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackCX, trackTop + btnH * 0.5f, -0.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, btnH, 1.0f));
	scrollBar->updateInstance(2, r2);

	// Down button (idx 3) — anchored inside the padded track
	InstancedRectangleData r3{};
	r3.color = Colors::Gray(0.35f);
	r3.borderRadius = 6.0f;
	r3.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackCX, trackTop + trackH - btnH * 0.5f, -0.1f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, btnH, 1.0f));
	scrollBar->updateInstance(3, r3);

	// Thumb (idx 1) — updateSlider() will place it within [trackTop, trackTop+trackH]
	slider.borderRadius = 25.0f;
	slider.color = Colors::Gray(0.55f);

	// If updateSlider()/drag use these, keep them consistent:
	// - trackX: center X of the track (screen space)
	// - trackH: height of the track (already set above)
	// Add this optional cache if your updateSlider() needs the absolute top:
	// this->trackTop = trackTop;

	updateSlider();

	// Bar is in screen space → identity view
	scrollBar->updateMVP(std::nullopt, glm::mat4(1.0f), mvp.proj);
}

void TextField::updateSlider() {
	const float sbW = params.scrollBarWidth;
	const float btnH = params.scrollBarWidth;

	const float fieldH = params.dim.y;		// viewport (visible) height
	const float fieldTop = params.center.y; // screen-space top of field

	// Same padding used in createScrollBar()
	const float padTop = params.scrollBarOffset * 0.5f;
	const float padBot = params.scrollBarOffset * 0.5f;

	// Track geometry (already set: trackH is shortened; trackX is center X)
	const float trackTop = fieldTop + padTop; // screen-space Y of track top
	const float innerH = std::max(0.0f, trackH - 2.0f * btnH);

	// Thumb size proportional to visible fraction (fieldH / contentH), clamped to [sbW, innerH]
	float thumbH = 0.0f;
	if (innerH > 0.0f) {
		if (contentH <= fieldH) {
			thumbH = innerH; // all content visible
		} else {
			thumbH = std::clamp(innerH * (fieldH / contentH), sbW, innerH);
		}
	}

	// Scroll position to 0..1 (protect against degenerate ranges)
	const float scrollY = glm::clamp(lookAtCoords.y, scrollMinY, scrollMaxY);
	const float t = (scrollMaxY > 0.0f && innerH > thumbH) ? (scrollY / scrollMaxY) : 0.0f;

	// Thumb center Y inside the padded track
	const float thumbYLocal = btnH + t * (innerH - thumbH); // distance from trackTop
	const float thumbCenterY = trackTop + thumbYLocal + thumbH * 0.5f;

	// Build thumb model (trackX is center X of the track in screen space)
	slider.model = glm::translate(glm::mat4(1.0f), glm::vec3(trackX, thumbCenterY, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(sbW, thumbH, 1.0f));

	scrollBar->updateInstance(1, slider);
}

void TextField::dragSliderToCursor() {
	GLFWwindow *w = Engine::window;
	if (!w)
		return;

	double mx, my;
	glfwGetCursorPos(w, &mx, &my);

	float xscale = 1.0f, yscale = 1.0f;
	glfwGetWindowContentScale(w, &xscale, &yscale);

	const float cursorFbY = float(my) * yscale;

	const float btnH = params.scrollBarWidth;
	const float fieldH = params.dim.y;
	const float fieldTop = params.center.y;

	// same padding as createScrollBar/updateSlider
	const float padTop = params.scrollBarOffset * 0.5f;
	const float padBot = params.scrollBarOffset * 0.5f;

	// padded track geometry (must match what you computed in createScrollBar)
	const float trackTopFB = fieldTop + padTop;
	const float trackHFB = std::max(0.0f, fieldH - padTop - padBot);
	const float innerH = std::max(0.0f, trackHFB - 2.0f * btnH);

	// thumb height (same policy as updateSlider)
	float thumbH = 0.0f;
	if (innerH > 0.0f) {
		if (contentH <= fieldH) {
			thumbH = innerH;
		} else {
			thumbH = std::clamp(innerH * (fieldH / contentH), btnH, innerH);
		}
	}

	if (innerH <= 0.0f || thumbH <= 0.0f || scrollMaxY <= scrollMinY + 1e-6f)
		return;

	// Cursor in track-local coords (pixels from top of the padded track)
	const float localY = cursorFbY - trackTopFB;

	// Slider center travel in local track coords
	const float minCenter = btnH + thumbH * 0.5f;
	const float maxCenter = trackHFB - btnH - thumbH * 0.5f;

	const float centerY = glm::clamp(localY, minCenter, maxCenter);

	// Map slider position -> t -> scrollY
	const float denom = std::max(1e-6f, (trackHFB - 2.0f * btnH) - thumbH);
	const float t = (centerY - minCenter) / denom; // 0..1
	const float newY = scrollMinY + t * (scrollMaxY - scrollMinY);

	const float clamped = glm::clamp(newY, scrollMinY, scrollMaxY);
	camPosOrtho.y = clamped;
	lookAtCoords.y = clamped;
	camTarget.y = clamped;
}

void TextField::mouseDragY(float &scrollMinY, float &scrollMaxY, bool inverted) {
	GLFWwindow *win = Engine::window;
	if (!win || !glfwGetWindowAttrib(win, GLFW_FOCUSED))
		return;

	// Remember the initial Y once (top-most allowed position)
	if (!s_initY) {
		s_initialY = camPosOrtho.y;
		s_initY = true;
	}

	// Hook wheel: vertical pan only (no zoom)
	if (!s_hookedScroll && enableScrolling) {
		Events::scrollCallbacks.push_back([&, inverted](double /*xoff*/, double yoff) {
			if (usingSlider)
				return;

			double cx, cy;
			glfwGetCursorPos(Engine::window, &cx, &cy);
			float xs = 1.f, ys = 1.f;
			glfwGetWindowContentScale(Engine::window, &xs, &ys);
			const float fx = float(cx) * xs;
			const float fy = float(cy) * ys;
			const bool inside = fx >= sp.viewport.x && fx <= sp.viewport.x + sp.viewport.width && fy >= sp.viewport.y && fy <= sp.viewport.y + sp.viewport.height;
			if (!inside)
				return;

			int winW = 0, winH = 0;
			glfwGetWindowSize(Engine::window, &winW, &winH);
			if (winW <= 0 || winH <= 0)
				return;

			const float aspect = sp.viewport.height > 0.f ? (sp.viewport.width / sp.viewport.height) : 1.0f;
			const float visH = params.dim.y / glm::max(zoom, 1e-6f);
			const float visW = params.dim.x / glm::max(zoom, 1e-6f);
			const float effH = (aspect >= 1.0f) ? (visW / aspect) : visH;

			const float worldPerPxY = effH / float(winH);
			const float scrollPixels = 40.0f;
			const float dyWorld = float(yoff) * worldPerPxY * scrollPixels;

			applyVerticalDeltaClamped(inverted ? -dyWorld : dyWorld, scrollMinY, scrollMaxY);
		});
		s_hookedScroll = true;
	}

	if (!enableMouseDrag) {
		return;
	}

	{
		double mx, my;
		glfwGetCursorPos(win, &mx, &my);
		float xs = 1.f, ys = 1.f;
		glfwGetWindowContentScale(win, &xs, &ys);
		const float fx = float(mx) * xs;
		const float fy = float(my) * ys;

		const bool inside = fx >= sp.viewport.x && fx <= sp.viewport.x + sp.viewport.width - params.scrollBarWidth && fy >= sp.viewport.y && fy <= sp.viewport.y + sp.viewport.height;

		if (!inside) {
			// reset anchor so there’s no jump when re-entering
			lastPointerX = -1.0;
			lastPointerY = -1.0;
			return;
		}
	}

	// Drag-to-pan (vertical only)
	double mx = 0.0, my = 0.0;
	glfwGetCursorPos(win, &mx, &my);
	const bool lmb = glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

	if (lastPointerX < 0.0 || lastPointerY < 0.0) {
		lastPointerX = mx;
		lastPointerY = my;
		return;
	}

	const double dy = my - lastPointerY;
	lastPointerX = mx;
	lastPointerY = my;

	if (!lmb)
		return;

	int winW = 0, winH = 0;
	glfwGetWindowSize(win, &winW, &winH);
	if (winW <= 0 || winH <= 0)
		return;

	const float aspect = sp.viewport.height > 0.f ? (sp.viewport.width / sp.viewport.height) : 1.0f;
	const float visH = params.dim.y / glm::max(zoom, 1e-6f);
	const float visW = params.dim.x / glm::max(zoom, 1e-6f);
	const float effH = (aspect >= 1.0f) ? (visW / aspect) : visH;
	const float worldPerPxY = effH / float(winH);

	applyVerticalDeltaClamped(inverted ? -float(dy) * worldPerPxY : float(dy) * worldPerPxY, scrollMinY, scrollMaxY);
}

void TextField::swapChainUpdate() {
	updateScreenParams();
	textModel = make_unique<Text>(scene, mvp, sp, textParams, renderPass);
	wrap();
	createScrollBar();
}

void TextField::updateUniformBuffers(optional<Model::MVP> mvp) {
	if (params.text != cache) {
		cache = params.text;
		wrap();
	}
	if (!usingSlider) {
		mouseDragY(scrollMinY, scrollMaxY, /*inverted=*/true);
	}
	if (enableSlider && usingSlider) {
		dragSliderToCursor();
	}
	glm::mat4 pan = glm::translate(glm::mat4(1.0f), glm::vec3(-lookAtCoords.x, -lookAtCoords.y, 0.0f));
	if (mvp.has_value()) {
		glm::mat4 view = pan * mvp->view;
		textModel->updateMVP(std::nullopt, view, mvp->proj);
		scrollBar->updateMVP(std::nullopt, mvp->view, mvp->proj);
	} else {
		textModel->updateMVP(std::nullopt, pan);
		scrollBar->updateMVP(std::nullopt, std::nullopt, mvp->proj);
	}
	updateSlider();
}

bool TextField::is_cont_byte(unsigned char b) { return (b & 0xC0) == 0x80; }

size_t TextField::utf8_len_from_lead(unsigned char b) {
	if (b < 0x80u)
		return 1;
	if ((b >> 5) == 0x6)
		return 2;
	if ((b >> 4) == 0xE)
		return 3;
	if ((b >> 3) == 0x1E)
		return 4;
	return 1;
}

size_t TextField::snap_to_cp_start(const std::string &s, size_t pos) {
	pos = std::min(pos, s.size());
	while (pos > 0 && is_cont_byte(static_cast<unsigned char>(s[pos - 1])))
		--pos;
	return pos;
}

size_t TextField::cp_left_utf8(const std::string &s, size_t pos) {
	if (s.empty() || pos == 0)
		return 0;
	pos = std::min(pos, s.size());
	size_t i = pos - 1;
	while (i > 0 && is_cont_byte(static_cast<unsigned char>(s[i])))
		--i;
	return i;
}

size_t TextField::cp_right_utf8(const std::string &s, size_t pos) {
	if (s.empty())
		return 0;
	pos = std::min(pos, s.size());
	pos = snap_to_cp_start(s, pos);
	if (pos == s.size())
		return pos;
	const unsigned char lead = static_cast<unsigned char>(s[pos]);
	const size_t len = utf8_len_from_lead(lead);
	return std::min(pos + len, s.size());
}

// Insert cp at (possibly mid-cp) position; returns new caret position (just after inserted cp)
size_t TextField::utf8_append_at(std::string &out, unsigned int cp, size_t position) {
	if ((cp >= 0xD800u && cp <= 0xDFFFu) || cp > 0x10FFFFu)
		return position; // invalid scalar
	size_t pos = std::min(position, out.size());
	size_t start = snap_to_cp_start(out, pos);
	size_t insert_pos = start;
	if (pos > start) {
		const unsigned char lead = static_cast<unsigned char>(out[start]);
		const size_t len = utf8_len_from_lead(lead);
		insert_pos = std::min(start + len, out.size());
	}
	char buf[4];
	size_t n = 0;
	if (cp <= 0x7Fu) {
		buf[n++] = static_cast<char>(cp);
	} else if (cp <= 0x7FFu) {
		buf[n++] = static_cast<char>(0xC0u | (cp >> 6));
		buf[n++] = static_cast<char>(0x80u | (cp & 0x3Fu));
	} else if (cp <= 0xFFFFu) {
		buf[n++] = static_cast<char>(0xE0u | (cp >> 12));
		buf[n++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
		buf[n++] = static_cast<char>(0x80u | (cp & 0x3Fu));
	} else {
		buf[n++] = static_cast<char>(0xF0u | (cp >> 18));
		buf[n++] = static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu));
		buf[n++] = static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu));
		buf[n++] = static_cast<char>(0x80u | (cp & 0x3Fu));
	}
	out.insert(out.begin() + static_cast<std::ptrdiff_t>(insert_pos), buf, buf + n);
	return insert_pos + n;
}

// Delete cp immediately to the left of position; returns new caret position (start of deleted cp)
size_t TextField::utf8_delete_prev(std::string &s, size_t position) {
	if (s.empty())
		return 0;
	size_t pos = std::min(position, s.size());
	if (pos == 0)
		return 0;
	size_t start = snap_to_cp_start(s, pos);
	size_t del_start = (pos > start) ? start : cp_left_utf8(s, start);
	if (del_start >= s.size())
		return 0;
	const unsigned char lead = static_cast<unsigned char>(s[del_start]);
	size_t len = utf8_len_from_lead(lead);
	size_t del_end = std::min(del_start + len, s.size());
	while (del_end < s.size() && is_cont_byte(static_cast<unsigned char>(s[del_end])))
		++del_end;
	s.erase(del_start, del_end - del_start);
	return del_start;
}

// --- caret mapping helpers ---
size_t TextField::caretParaByte() const { return textModel ? std::min(textModel->textParams.caret.byte, textModel->textParams.text.size()) : 0; }

size_t TextField::caretOrigByte() const {
	if (!textModel || mapParaByteToOrig.empty()) {
		// best effort fallback: clamp to original
		return std::min(textModel ? textModel->textParams.caret.byte : size_t(0), params.text.size());
	}
	size_t paraB = std::min(caretParaByte(), mapParaByteToOrig.size() - 1);
	return mapParaByteToOrig[paraB];
}

void TextField::setCaretOrigByte(size_t b) {
	if (!textModel)
		return;
	// mark as "caret is ORIGINAL"; wrap() will map it into para on next run
	caretFromWrapped = false;
	textModel->textParams.caret.byte = std::min(b, params.text.size());
}

void TextField::insertCodepointAtCaretInto(std::string &external, unsigned int cp) {
	// caret in PARA bytes -> original byte
	size_t paraB = caretParaByte();
	size_t ob = (paraB < mapParaByteToOrig.size()) ? mapParaByteToOrig[paraB] : external.size();

	// edit external (utf8 safe)
	size_t newOb = utf8_append_at(external, cp, ob);

	// sync field and caret for next wrap
	params.text = external;
	setCaretOrigByte(newOb);   // mark as original; wrap() remaps
	onTextChangedExternally(); // caretFromWrapped = false
}

void TextField::backspaceAtCaretInto(std::string &external) {
	size_t paraB = caretParaByte();
	size_t ob = (paraB < mapParaByteToOrig.size()) ? mapParaByteToOrig[paraB] : external.size();

	size_t newOb = utf8_delete_prev(external, ob);

	params.text = external;
	setCaretOrigByte(newOb);
	onTextChangedExternally();
}

void TextField::moveCaretLeftInto(const std::string &external) {
	size_t paraB = caretParaByte();
	size_t ob = (paraB < mapParaByteToOrig.size()) ? mapParaByteToOrig[paraB] : external.size();
	setCaretOrigByte(cp_left_utf8(external, ob));
}

void TextField::moveCaretRightInto(const std::string &external) {
	size_t paraB = caretParaByte();
	size_t ob = (paraB < mapParaByteToOrig.size()) ? mapParaByteToOrig[paraB] : external.size();
	setCaretOrigByte(cp_right_utf8(external, ob));
}

void TextField::render() {
	textModel->render();
	if (showScrollBar) {
		scrollBar->render();
	}
}
