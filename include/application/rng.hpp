#include <iomanip>
#include <random>
#include <sstream>
#include <string>

using std::string;

namespace RNG {

inline string uuid_v4() {
	static thread_local std::mt19937_64 rng{std::random_device{}()};
	std::uniform_int_distribution<uint64_t> dist;

	uint64_t a = dist(rng);
	uint64_t b = dist(rng);

	// set version (4) and variant bits
	a = (a & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
	b = (b & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

	std::ostringstream oss;
	oss << std::hex << std::setfill('0') << std::setw(8) << (uint32_t)(a >> 32) << '-' << std::setw(4) << (uint16_t)(a >> 16) << '-' << std::setw(4) << (uint16_t)a << '-' << std::setw(4) << (uint16_t)(b >> 48) << '-' << std::setw(12) << (b & 0x0000FFFFFFFFFFFFULL);
	return oss.str();
}

} // namespace Random
