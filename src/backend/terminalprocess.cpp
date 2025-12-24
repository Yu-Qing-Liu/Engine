#include "terminalprocess.hpp"
#include "events.hpp"
#include "text.hpp"

#if defined(__linux__) || defined(__LINUX__) || defined(__gnu_linux__)
#include <pty.h>
#include <utmp.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

TerminalProcess::~TerminalProcess() { shutdown(); }

static inline void set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static inline ssize_t write_all(int fd, const void *buf, size_t len) {
	const uint8_t *p = static_cast<const uint8_t *>(buf);
	size_t left = len;
	while (left) {
		ssize_t n = ::write(fd, p, left);
		if (n > 0) {
			left -= (size_t)n;
			p += n;
			continue;
		}
		if (n < 0 && (errno == EINTR))
			continue;
		if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			::poll(nullptr, 0, 1);
			continue;
		}
		return n; // error
	}
	return (ssize_t)len;
}

static inline std::string utf32_to_utf8(char32_t cp) {
	std::string out;
	if (cp <= 0x7F) {
		out.push_back(char(cp));
	} else if (cp <= 0x7FF) {
		out.push_back(char(0xC0 | (cp >> 6)));
		out.push_back(char(0x80 | (cp & 0x3F)));
	} else if (cp <= 0xFFFF) {
		out.push_back(char(0xE0 | (cp >> 12)));
		out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(char(0x80 | (cp & 0x3F)));
	} else {
		out.push_back(char(0xF0 | (cp >> 18)));
		out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(char(0x80 | (cp & 0x3F)));
	}
	return out;
}

// -----------------------------------------------------------------------------
// Internal backend holder
// -----------------------------------------------------------------------------

struct TerminalProcess::TerminalBackend {
	int master_fd{-1};
	int slave_fd{-1};
	pid_t child{-1};
};

static void configure_tty_master(int fd) {
	struct termios tio{};
	tcgetattr(fd, &tio);

	// Raw-ish on master side
	cfmakeraw(&tio);
	tio.c_lflag |= ISIG;
	tio.c_cc[VERASE] = 0x7f; // DEL as backspace
	tcsetattr(fd, TCSANOW, &tio);
}

// -----------------------------------------------------------------------------
// TerminalProcess lifecycle
// -----------------------------------------------------------------------------

void TerminalProcess::init(Text *textModel) {
	this->textModel = textModel;

	// --- Spawn PTY & shell ---
	backend = std::make_shared<TerminalBackend>();

	int mfd = -1, sfd = -1;
	if (openpty(&mfd, &sfd, nullptr, nullptr, nullptr) < 0) {
		// Show an error message in the text model
		scrollback.assign("\033[31m[terminal] openpty() failed.\033[0m\n");
		screen = scrollback;
		if (textModel)
			textModel->setText(screen);
		return;
	}

	backend->master_fd = mfd;
	backend->slave_fd = sfd;

	pid_t pid = fork();
	if (pid < 0) {
		scrollback.assign("\033[31m[terminal] fork() failed.\033[0m\n");
		screen = scrollback;
		if (textModel)
			textModel->setText(screen);
		close(mfd);
		close(sfd);
		return;
	}

	if (pid == 0) {
		// --- Child branch ---
		setsid();
#if defined(TIOCSCTTY)
		ioctl(sfd, TIOCSCTTY, 0);
#endif
		dup2(sfd, STDIN_FILENO);
		dup2(sfd, STDOUT_FILENO);
		dup2(sfd, STDERR_FILENO);
		if (sfd > STDERR_FILENO)
			close(sfd);
		if (mfd >= 0)
			close(mfd);

		// TTY settings on the SLAVE, not the master.
		// Put the slave in raw mode, we do our own line editing.
		struct termios tio{};
		tcgetattr(STDIN_FILENO, &tio);
		cfmakeraw(&tio);
		// keep signals so Ctrl-C / Ctrl-Z still work
		tio.c_lflag |= ISIG;
		tio.c_cc[VERASE] = 0x7f;
#ifdef ECHOCTL
		tio.c_lflag &= ~ECHOCTL;
#endif
		tcsetattr(STDIN_FILENO, TCSANOW, &tio);

		// Env. Pick ONE TERM.
		setenv("TERM", "dumb", 1);

		const char *shell = getenv("SHELL");
		if (!shell || !*shell)
			shell = "/bin/sh";
		execlp(shell, shell, "-i", (char *)nullptr);
		perror("exec shell");
		::_exit(127);
	}

	// --- Parent ---
	backend->child = pid;
	close(sfd); // keep only master
	set_nonblocking(mfd);
	configure_tty_master(mfd);

	running.store(true);
	scrollback.clear();
	inputLine.clear();
	screen.clear();
	if (textModel)
		textModel->setText(screen);

	// Reader thread: read from PTY and append to pending buffer
	reader = std::thread([this]() {
		std::vector<char> buf(4096);
		while (running.load()) {
			pollfd pfd{backend->master_fd, POLLIN, 0};
			int pr = ::poll(&pfd, 1, 50);
			if (pr < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			if (pr == 0)
				continue;

			if (pfd.revents & POLLIN) {
				ssize_t n = ::read(backend->master_fd, buf.data(), buf.size());
				if (n > 0) {
					std::lock_guard<std::mutex> lk(mtx);
					pending.append(buf.data(), (size_t)n);
					dirty.store(true, std::memory_order_release);
				} else if (n == 0) {
					// EOF
					break;
				} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
					break;
				}
			}
		}
	});

	// --- Input wiring ---
	// Characters: modify inputLine, we handle echo/editing.
	charRegId = Events::registerCharacterInput([this](unsigned int codepoint) {
		if (!this->textModel)
			return;
		if (!running.load())
			return;

		std::string utf8 = utf32_to_utf8((char32_t)codepoint);

		if (caretInInput > inputLine.size())
			caretInInput = inputLine.size();

		inputLine.insert(caretInInput, utf8);
		caretInInput += utf8.size(); // byte-based caret; fine as long as Text uses UTF-8

		dirty.store(true, std::memory_order_release);
	});

	keyRegId = Events::registerKeyPress([this](int key, int /*scancode*/, int action, int mods) {
		if (!this->textModel)
			return;

		if (!running.load() || !backend || backend->master_fd < 0)
			return;
		if (action != Events::ACTION_PRESS && action != Events::ACTION_REPEAT)
			return;

		// Ctrl-C: send interrupt to PTY
		if ((mods & Events::MOD_CONTROL_KEY) && (key == GLFW_KEY_C || key == 'C')) {
			const char intr = 0x03;
			write_all(backend->master_fd, &intr, 1);
			return;
		}

		// Ctrl-V: paste clipboard into inputLine at caret position
		if ((mods & Events::MOD_CONTROL_KEY) && (key == GLFW_KEY_V || key == 'V')) {
			// NOTE: replace `nullptr` with your GLFWwindow* if needed
			const char *clip = glfwGetClipboardString(nullptr);
			if (clip && *clip) {
				std::string text(clip);

				// Optional: normalize CRLF to LF so we don't get stray '\r'
				std::string normalized;
				normalized.reserve(text.size());
				for (size_t i = 0; i < text.size(); ++i) {
					char ch = text[i];
					if (ch == '\r') {
						// swallow bare \r or part of \r\n
						if (i + 1 < text.size() && text[i + 1] == '\n') {
							continue; // we'll handle the '\n' in the next iteration
						}
						normalized.push_back('\n');
					} else {
						normalized.push_back(ch);
					}
				}

				if (caretInInput > inputLine.size())
					caretInInput = inputLine.size();

				inputLine.insert(caretInInput, normalized);
				caretInInput += normalized.size(); // byte-based caret (same as rest of code)

				dirty.store(true, std::memory_order_release);
			}
			return;
		}

		switch (key) {
		case GLFW_KEY_ENTER:
		case GLFW_KEY_KP_ENTER: {
			// Send the full line to PTY + newline
			std::string lineToSend = inputLine;
			lineToSend.push_back('\n');
			write_all(backend->master_fd, lineToSend.data(), lineToSend.size());

			// Add the line to scrollback; clear inputLine
			scrollback += inputLine;
			scrollback.push_back('\n');
			inputLine.clear();
			caretInInput = 0;

			dirty.store(true, std::memory_order_release);
			break;
		}

		case GLFW_KEY_BACKSPACE: {
			if (caretInInput > 0) {
				// Remove previous byte
				--caretInInput;
				inputLine.erase(caretInInput, 1);
				dirty.store(true, std::memory_order_release);
			}
			break;
		}

		case GLFW_KEY_LEFT: {
			if (caretInInput > 0) {
				--caretInInput;
				dirty.store(true, std::memory_order_release);
			}
			break;
		}

		case GLFW_KEY_RIGHT: {
			if (caretInInput < inputLine.size()) {
				++caretInInput;
				dirty.store(true, std::memory_order_release);
			}
			break;
		}

		case GLFW_KEY_HOME: {
			caretInInput = 0;
			dirty.store(true, std::memory_order_release);
			break;
		}

		case GLFW_KEY_END: {
			caretInInput = inputLine.size();
			dirty.store(true, std::memory_order_release);
			break;
		}

		// You can extend with UP/DOWN for history, DELETE, etc., if desired.
		default:
			break; // printable handled in character callback
		}
	});
}

void TerminalProcess::shutdown() {
	if (!running.exchange(false))
		return;

	// Unregister event callbacks
	if (!charRegId.empty()) {
		Events::unregisterCharacter(charRegId);
		charRegId.clear();
	}
	if (!keyRegId.empty()) {
		Events::unregisterKeyPress(keyRegId);
		keyRegId.clear();
	}

	// Close PTY master to unblock reader
	if (backend && backend->master_fd >= 0) {
		::close(backend->master_fd);
		backend->master_fd = -1;
	}

	if (reader.joinable())
		reader.join();

	// Reap child if it has exited
	if (backend && backend->child > 0) {
		int st = 0;
		::waitpid(backend->child, &st, WNOHANG);
		backend->child = -1;
	}
}

// -----------------------------------------------------------------------------
// VT filtering: append to scrollback
// -----------------------------------------------------------------------------

TerminalProcess::Action TerminalProcess::filter(const std::string &s) {
	// Byte-level VT filtering:
	//  - Keep only SGR (CSI ... 'm') for color.
	//  - Drop other CSI (K, H, J, cursor moves, DEC private like ?2004h/l).
	//  - Drop OSC/DCS/SOS/PM/APC strings.
	//  - Map "\r\n"→'\n'; lone '\r' resets to BOL.
	enum State { S_Normal, S_Esc, S_Csi, S_String } st = S_Normal;
	Action action = Action::APPEND;
	size_t i = 0;
	while (i < s.size()) {
		unsigned char c = (unsigned char)s[i];
		switch (st) {
		case S_Normal:
			if (c == '\r') {
				if (i + 1 < s.size() && s[i + 1] == '\n') {
					scrollback.push_back('\n');
					i += 2;
					break;
				}
				// BOL: truncate current line
				if (auto p = scrollback.find_last_of('\n'); p == std::string::npos)
					scrollback.clear();
				else
					scrollback.erase(p + 1);
				++i;
				break;
			}
			if (c == 0x08 || c == 0x7F) { // BS/DEL
				if (!scrollback.empty() && scrollback.back() != '\n')
					scrollback.pop_back();
				action = Action::DEL;
				++i;
				break;
			}
			if (c == 0x1B) {
				st = S_Esc;
				++i;
				break;
			} // ESC
			// swallow other C0 except \n and \t
			if (c < 0x20 && c != '\n' && c != '\t') {
				++i;
				break;
			}
			scrollback.push_back((char)c);
			++i;
			break;

		case S_Esc:
			if (i < s.size()) {
				unsigned char n = (unsigned char)s[i];
				if (n == '[') {
					st = S_Csi;
					++i;
					break;
				} // CSI
				if (n == ']') {
					st = S_String;
					++i;
					break;
				} // OSC to BEL/ST
				if (n == 'P' || n == 'X' || n == '^' || n == '_') { // DCS/SOS/PM/APC
					st = S_String;
					++i;
					break;
				}
			}
			// Unknown single-ESC sequence → drop
			st = S_Normal;
			break;

		case S_Csi: {
			size_t j = i;
			// Optional private marker '?'
			if (j < s.size() && s[j] == '?')
				++j;
			// Params & intermediates up to final (0x40..0x7E)
			while (j < s.size()) {
				unsigned char ch = (unsigned char)s[j];
				if (ch >= 0x40 && ch <= 0x7E)
					break;
				++j;
			}
			if (j >= s.size()) { // truncated; drop
				st = S_Normal;
				i = j;
				break;
			}
			char final = s[j];
			if (final == 'm') {
				// Keep SGR for renderer to colorize
				scrollback.append("\x1b[", 2);
				scrollback.append(s.data() + i, (j - i) + 1);
			}
			// else: DROP all other CSI (K, H, J, A/B/C/D, ?2004h/l, etc.)
			st = S_Normal;
			i = j + 1;
			break;
		}

		case S_String: {
			// OSC/DCS/etc until BEL (0x07) or ST (ESC \)
			size_t j = i;
			bool done = false;
			while (j < s.size()) {
				unsigned char ch = (unsigned char)s[j];
				if (ch == 0x07) {
					++j;
					done = true;
					break;
				} // BEL
				if (ch == 0x1B && j + 1 < s.size() && s[j + 1] == '\\') {
					j += 2;
					done = true;
					break;
				} // ST
				++j;
			}
			st = S_Normal;
			i = j;
			(void)done; // intentionally dropped
			break;
		}
		}
	}
	return action;
}

// -----------------------------------------------------------------------------
// UI flush: rebuild Text from scrollback + inputLine
// -----------------------------------------------------------------------------

bool TerminalProcess::flushUI() {
	if (!dirty.exchange(false, std::memory_order_acq_rel))
		return false;

	std::string delta;
	{
		std::lock_guard<std::mutex> lk(mtx);
		delta.swap(pending);
	}

	if (!delta.empty()) {
		filter(delta); // updates scrollback
	}

	// Rebuild screen = scrollback + inputLine
	screen.clear();
	screen.reserve(scrollback.size() + inputLine.size());
	screen.append(scrollback);
	screen.append(inputLine);

	if (!textModel)
		return false;

	textModel->setText(screen);
	textModel->rebuild();

	// Length in *Text* coordinates (visible glyphs, no ANSI)
	size_t len = textModel->textLength();

	// Compute visible scrollback length:
	//   len = visible(scrollback) + inputLine.size()
	size_t visibleScrollbackLen = len;
	if (inputLine.size() <= len) {
		visibleScrollbackLen = len - inputLine.size();
	}
	// This is the boundary: everything before is scrollback (read-only)
	maxCursorIndex = visibleScrollbackLen - 1;

	// Desired caret position in Text coordinates
	size_t caretPos = visibleScrollbackLen + caretInInput - 1;

	cursorIndex = caretPos;
	textModel->setCaret(cursorIndex);
	textModel->rebuild();

    return true;
}

// -----------------------------------------------------------------------------
// Mouse caret integration
// -----------------------------------------------------------------------------

void TerminalProcess::setCaretFromAbsolutePos(size_t absolutePos) {
	if (!textModel)
		return;

	size_t len = textModel->textLength();
	if (len == 0)
		return;

	// Clamp to valid caret indices in Text
	if (absolutePos >= len)
		absolutePos = len - 1;

	// Only allow selecting inside the editable input line
	if (absolutePos < maxCursorIndex)
		return;

	// Convert to inputLine-local index (still in visible units)
	size_t local = absolutePos - maxCursorIndex;

	// inputLine has no ANSI, so bytes == visible chars
	if (local > inputLine.size())
		local = inputLine.size();

	caretInInput = local;
	dirty.store(true, std::memory_order_release);
}

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <util.h>

#elif defined(_WIN32)
#include <windows.h>

// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

TerminalProcess::~TerminalProcess() { shutdown(); }

static inline void set_nonblocking(int fd) {
	
}

static inline size_t write_all(int fd, const void *buf, size_t len) {
	return -1;
}

static inline std::string utf32_to_utf8(char32_t cp) {
	
}

// -----------------------------------------------------------------------------
// Internal backend holder
// -----------------------------------------------------------------------------

struct TerminalProcess::TerminalBackend {
	
};

static void configure_tty_master(int fd) {
	
}

// -----------------------------------------------------------------------------
// TerminalProcess lifecycle
// -----------------------------------------------------------------------------

void TerminalProcess::init(Text *textModel) {
	
}

void TerminalProcess::shutdown() {
	
}

// -----------------------------------------------------------------------------
// VT filtering: append to scrollback
// -----------------------------------------------------------------------------

TerminalProcess::Action TerminalProcess::filter(const std::string &s) {
	return Action::APPEND;
}

// -----------------------------------------------------------------------------
// UI flush: rebuild Text from scrollback + inputLine
// -----------------------------------------------------------------------------

bool TerminalProcess::flushUI() {
	return false;
}

// -----------------------------------------------------------------------------
// Mouse caret integration
// -----------------------------------------------------------------------------

void TerminalProcess::setCaretFromAbsolutePos(size_t absolutePos) {
	
}

#endif
