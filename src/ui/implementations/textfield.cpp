#include "textfield.hpp"

TextField::TextField(Scene *scene, const Model::MVP &mvp, Model::ScreenParams &screenParams, const Text::FontParams &textParams, const VkRenderPass &renderPass) : textParams(textParams), Widget(scene, mvp, screenParams, renderPass) { textModel = std::make_unique<Text>(scene, mvp, screenParams, textParams, renderPass); }

void TextField::updateScreenParams() {
	const float swapW = screenParams.viewport.width;
	const float swapH = screenParams.viewport.height;

	// ----- Inner (grid only) -----
	const float desiredX = params.center.x;
	const float desiredY = params.center.y;
	const float desiredW = params.dim.x;
	const float desiredH = params.dim.y;

	const float ix = std::max(0.0f, desiredX);
	const float iy = std::max(0.0f, desiredY);
	const float ix2 = std::min(desiredX + desiredW, swapW);
	const float iy2 = std::min(desiredY + desiredH, swapH);
	const float iW = std::max(0.0f, ix2 - ix);
	const float iH = std::max(0.0f, iy2 - iy);

	// sp.viewport.x = ix;
	// sp.viewport.y = iy;
	// sp.viewport.width = iW;
	// sp.viewport.height = iH;
	sp.viewport = screenParams.viewport;

	sp.scissor.offset = {(int32_t)std::floor(ix), (int32_t)std::floor(iy)};
	sp.scissor.extent = {(uint32_t)std::floor(iW), (uint32_t)std::floor(iH)};

	// ----- Outer (+ margins) -> bgSp -----
	const float mL = params.margins.x;
	const float mT = params.margins.y;
	const float mR = params.margins.z;
	const float mB = params.margins.w;

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

void TextField::wrap() {
	const std::string &src = params.text;
	const float maxW = params.dim.x;

	// Line height from fixed font; no scaling
	const float lineH = textModel->getPixelHeight();
	const float stepY = lineH + params.lineSpacing;

	auto width_of = [&](size_t a, size_t b) -> float {
		if (b <= a)
			return 0.0f;
		return textModel->getPixelWidth(src.substr(a, b - a)); // [a, b)
	};

	// 1) Compute wrap spans: [start,end) in ORIGINAL text (no '\n' inserted yet)
	std::vector<std::pair<size_t, size_t>> spans;

	if (src.empty() || maxW <= 0.0f) {
		spans.emplace_back(0, 0);
	} else {
		const size_t n = src.size();
		size_t lineStart = 0;
		size_t i = 0;
		size_t lastSpace = std::string::npos;

		while (i < n) {
			const char c = src[i];

			if (c == '\n') {
				spans.emplace_back(lineStart, i); // exclude '\n'
				++i;
				lineStart = i;
				lastSpace = std::string::npos;
				continue;
			}

			// Try including this char
			float w = width_of(lineStart, i + 1);
			if (w <= maxW) {
				if (c == ' ' || c == '\t')
					lastSpace = i;
				++i;
				continue;
			}

			// overflow → break (prefer lastSpace)
			if (lastSpace != std::string::npos && lastSpace >= lineStart) {
				spans.emplace_back(lineStart, lastSpace); // break before trailing space
				i = lastSpace + 1;
			} else {
				// no whitespace to break on
				if (i == lineStart) {
					spans.emplace_back(lineStart, lineStart + 1); // single huge glyph
					++i;
				} else {
					spans.emplace_back(lineStart, i);
				}
			}
			lineStart = i;
			lastSpace = std::string::npos;
		}

		if (lineStart <= n) {
			spans.emplace_back(lineStart, n);
		}
	}

	// 2) Build wrapped paragraph AND a byte-index map: original → wrapped
	//    Map size n+1 (positions "between bytes"). We’ll fill conservatively as we go.
	const size_t n = src.size();
	std::string para;
	para.reserve(src.size() + spans.size());

	std::vector<size_t> mapOrigToPara(n + 1, 0); // defaults will be overwritten

	// Keep the current caret from the Text model (in ORIGINAL coords)
	const size_t origCaret = textModel->textParams.caret.byte;

	for (size_t idx = 0; idx < spans.size(); ++idx) {
		const auto [a, b] = spans[idx];

		// Right-trim spaces/tabs/CR from this visual line (do not include them)
		size_t e = b;
		while (e > a) {
			char ch = src[e - 1];
			if (ch == ' ' || ch == '\t' || ch == '\r')
				--e;
			else
				break;
		}

		// Before copying [a, e), set mapping for every original byte boundary in this kept slice
		const size_t basePara = para.size();
		for (size_t p = a; p <= e; ++p) {
			mapOrigToPara[p] = basePara + (p - a);
		}

		// Append the kept slice
		if (e > a)
			para.append(src.data() + a, e - a);

		// For the trimmed tail [e, b): snap caret to end-of-line.
		// If we will add a '\n' for wrapping (i.e., not the last visual line),
		// prefer to snap AFTER that newline so caret shows at the start of the next line.
		const bool willInsertNL = (idx + 1 < spans.size());
		const size_t snapPosBeforeNL = para.size();
		const size_t snapPosAfterNL = willInsertNL ? (para.size() + 1) : para.size();

		for (size_t p = e; p <= b; ++p) {
			mapOrigToPara[p] = snapPosBeforeNL; // default snap before NL
		}

		// Insert soft newline between wrapped lines
		if (willInsertNL) {
			para.push_back('\n');
			// After we actually insert '\n', any caret that fell into trimmed gap
			// should map AFTER the newline (start of next visual line).
			for (size_t p = e; p <= b; ++p) {
				mapOrigToPara[p] = snapPosAfterNL;
			}
		}
	}

	// Safety: clamp mapped caret inside para bounds
	size_t newCaret = (origCaret <= n) ? mapOrigToPara[origCaret] : para.size();
	if (newCaret > para.size())
		newCaret = para.size();

	// 3) Configure the Text model (fixed-size font). Origin: top-left baseline of box.
	const float desiredX = params.center.x;
	const float desiredY = params.center.y;

	textModel->textParams.text = std::move(para);
	textModel->textParams.color = params.textColor;
	textModel->textParams.origin = glm::vec3(desiredX, desiredY + lineH, 0.0f);
	textModel->textParams.lineAdvancePx = (params.lineSpacing > 0.0f) ? stepY : lineH;

	// Update caret byte to wrapped coords
	textModel->textParams.caret.byte = newCaret;
}

void TextField::createScrollBar() {

}

void TextField::updateSlider() {

}

void TextField::dragSliderToCursor() {

}

void TextField::mouseDragY(float &scrollMinY, float &scrollMaxY, bool inverted) {

}

void TextField::swapChainUpdate() {
	updateScreenParams();
	textModel = make_unique<Text>(scene, mvp, sp, textParams, renderPass);
	wrap();
}

void TextField::updateUniformBuffers(const Model::MVP &mvp) {
	if (textModel) {
		textModel->updateMVP(std::nullopt, mvp.view, mvp.proj);
	}
}

void TextField::render() {
	if (textModel) {
		wrap();
		textModel->render();
	}
}
