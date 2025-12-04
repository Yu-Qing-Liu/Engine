#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class Text;

class TerminalProcess {
  public:
	enum class Action { APPEND, DEL };

	TerminalProcess() = default;
	~TerminalProcess();

	void init(Text *textModel);
	void shutdown();
	bool flushUI();

	// Everything before this index is scrollback (read-only).
	// Caret selection is only allowed for positions >= maxCursorIndex.
	size_t getMaxCursorIndex() const { return maxCursorIndex; }

	// Called by UI when the user clicks somewhere in the text.
	// We only care about positions >= maxCursorIndex (inside the input line).
	void setCaretFromAbsolutePos(size_t absolutePos);

  private:
	// Filter/clean VT sequences from PTY output and append to scrollback.
	Action filter(const std::string &s);

	struct TerminalBackend;
	std::shared_ptr<TerminalBackend> backend;

	Text *textModel{nullptr};

	std::thread reader;
	std::atomic<bool> running{false};
	std::atomic<bool> dirty{false};
	std::mutex mtx;
	std::string pending; // raw bytes from PTY waiting to be filtered

	// Rendering model
	std::string scrollback; // PTY output (filtered)
	std::string inputLine;	// editable command line
	std::string screen;		// scrollback + inputLine (what Text sees)

	// Cursor bookkeeping
	size_t caretInInput{0};	  // cursor index inside inputLine (0..inputLine.size())
	size_t cursorIndex{0};	  // absolute cursor index in screen
	size_t maxCursorIndex{0}; // == scrollback.size()

	// Event registration IDs
	std::string charRegId;
	std::string keyRegId;
};
