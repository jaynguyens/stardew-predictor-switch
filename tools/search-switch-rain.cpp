#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
	"Switch seed normalization requires IEEE-754 binary32 floats");

constexpr uint32_t P1 = 2654435761U;
constexpr uint32_t P2 = 2246822519U;
constexpr uint32_t P3 = 3266489917U;
constexpr uint32_t P4 = 668265263U;
constexpr uint32_t JK_A = 314527869U;
constexpr uint32_t JK_B = 1234567U;

constexpr uint32_t rotl(uint32_t value, int bits) {
	return (value << bits) | (value >> (32 - bits));
}

constexpr uint32_t xx_round(uint32_t acc, uint32_t input) {
	return rotl(acc + input * P2, 13) * P1;
}

uint32_t xxh32_words(uint32_t w0, uint32_t w1, uint32_t w2) {
	uint32_t h = rotl(xx_round(P1 + P2, w0), 1)
		+ rotl(xx_round(P2, w1), 7)
		+ rotl(xx_round(0, w2), 12)
		+ rotl(xx_round(0U - P1, 0), 18);
	h += 20;
	// The predictor hashes five Int32 values; the fifth word is zero here.
	h = rotl(h, 17) * P4;
	h ^= h >> 15;
	h *= P2;
	h ^= h >> 13;
	h *= P3;
	h ^= h >> 16;
	return h;
}

constexpr uint32_t y_step(uint32_t y) {
	y ^= y << 5;
	y ^= y >> 7;
	y ^= y << 22;
	return y;
}

struct FixedJkState {
	uint32_t k1;
	uint32_t k2;
};

constexpr FixedJkState make_fixed_jk_state() {
	uint32_t y = 987654321U;
	uint32_t z = 43219876U;
	uint32_t c = 6543217U;
	y = y_step(y);
	uint64_t t = 4294584393ULL * z + c;
	c = static_cast<uint32_t>(t >> 32);
	z = static_cast<uint32_t>(t);
	uint32_t k1 = y + z;
	y = y_step(y);
	t = 4294584393ULL * z + c;
	c = static_cast<uint32_t>(t >> 32);
	z = static_cast<uint32_t>(t);
	return {k1, static_cast<uint32_t>(y + z)};
}

constexpr FixedJkState fixed_jk = make_fixed_jk_state();

inline uint32_t jk_first_sample(uint32_t seed) {
	return JK_A * seed + JK_B + fixed_jk.k1;
}

inline bool jk_next_double_below(uint32_t seed, double scaled_threshold) {
	uint32_t x1 = JK_A * seed + JK_B;
	uint32_t r1 = x1 + fixed_jk.k1;
	uint32_t x2 = JK_A * x1 + JK_B;
	uint32_t r2 = x2 + fixed_jk.k2;
	uint64_t numerator = (static_cast<uint64_t>(r1 >> 6) << 27) + (r2 >> 5);
	return static_cast<double>(numerator) < scaled_threshold;
}

constexpr double SAMPLE_SCALE = 9007199254740992.0; // 2^53
constexpr uint32_t LOCATION_WEATHER_HASH = static_cast<uint32_t>(-1513201250);
constexpr uint32_t SUMMER_RAIN_HASH = static_cast<uint32_t>(-309161378);

struct Result {
	uint32_t entered_seed;
	uint32_t effective_seed;
	uint8_t spring;
	uint8_t summer;

	int total() const { return spring + summer; }
};

uint32_t effective_switch_seed(uint32_t entered_seed) {
	// Console observations show the Switch new-game field behaving as if its
	// value passes through a single-precision representation. Above 2^24,
	// nearby decimal entries can therefore resolve to the same internal ID.
	return static_cast<uint32_t>(static_cast<float>(entered_seed));
}

inline bool spring_is_wet(uint32_t seed, int day) {
	if (day == 3) return true;
	if (day == 1 || day == 2 || day == 4 || day == 5 || day == 13 || day == 24) return false;
	uint32_t hash = xxh32_words(LOCATION_WEATHER_HASH, seed, static_cast<uint32_t>(day - 1));
	return jk_next_double_below(hash, 0.183 * SAMPLE_SCALE);
}

int summer_green_rain_day(uint32_t seed) {
	static constexpr int green_days[] = {5, 6, 7, 14, 15, 16, 18, 23};
	uint32_t hash = xxh32_words(777U, seed, 0U);
	return green_days[jk_first_sample(hash) % 8];
}

inline bool summer_is_wet(uint32_t seed, int day, int green_day) {
	if (day == 1 || day == 11 || day == 28) return false;
	if (day == green_day || day == 13 || day == 26) return true;
	uint32_t hash = xxh32_words(static_cast<uint32_t>(day + 27), seed / 2,
		SUMMER_RAIN_HASH);
	double chance = 0.12 + 0.003 * (day - 1);
	return jk_next_double_below(hash, chance * SAMPLE_SCALE);
}

Result score_seed(uint32_t entered_seed, int best_to_beat) {
	uint32_t seed = effective_switch_seed(entered_seed);
	int spring = 1; // Spring 3 is guaranteed rain.
	int summer = 3; // Summer 13/26 storms plus one Green Rain day.
	int successes = 0;
	int remaining = 43; // 21 Spring rolls + 22 non-Green-Rain Summer rolls.

	for (int day = 6; day <= 28; ++day) {
		if (day == 13 || day == 24) {
			continue;
		}
		if (spring_is_wet(seed, day)) {
			++spring;
			++successes;
		}
		--remaining;
		if (4 + successes + remaining < best_to_beat) {
			return {entered_seed, seed, 0, 0};
		}
	}

	int green_day = summer_green_rain_day(seed);
	for (int day = 2; day <= 27; ++day) {
		if (day == 11 || day == 13 || day == 26 || day == green_day) {
			continue;
		}
		if (summer_is_wet(seed, day, green_day)) {
			++summer;
			++successes;
		}
		--remaining;
		if (4 + successes + remaining < best_to_beat) {
			return {entered_seed, seed, 0, 0};
		}
	}
	return {entered_seed, seed, static_cast<uint8_t>(spring), static_cast<uint8_t>(summer)};
}

void print_calendar(uint32_t entered_seed) {
	uint32_t seed = effective_switch_seed(entered_seed);
	int green_day = summer_green_rain_day(seed);
	int spring_count = 0;
	int summer_count = 0;
	bool first = true;

	std::cout << "entered_seed=" << entered_seed
		<< "\neffective_seed=" << seed << "\nspring=";
	for (int day = 1; day <= 28; ++day) {
		if (!spring_is_wet(seed, day)) continue;
		std::cout << (first ? "" : ",") << day;
		first = false;
		++spring_count;
	}
	std::cout << "\nsummer=";
	first = true;
	for (int day = 1; day <= 28; ++day) {
		if (!summer_is_wet(seed, day, green_day)) continue;
		std::cout << (first ? "" : ",") << day;
		first = false;
		++summer_count;
	}
	std::cout << "\ngreen_rain=" << green_day
		<< "\nstorms=13,26"
		<< "\nspring_count=" << spring_count
		<< " summer_count=" << summer_count
		<< " total=" << spring_count + summer_count << '\n';
}

bool parse_u64(const char *text, uint64_t &value) {
	char *end = nullptr;
	errno = 0;
	unsigned long long parsed = std::strtoull(text, &end, 10);
	if (errno != 0 || text[0] == '\0' || end == nullptr || end[0] != '\0') return false;
	value = static_cast<uint64_t>(parsed);
	return true;
}

bool better_secondary(const Result &a, const Result &b) {
	if (a.total() != b.total()) return a.total() > b.total();
	int a_min = std::min(a.spring, a.summer);
	int b_min = std::min(b.spring, b.summer);
	if (a_min != b_min) return a_min > b_min;
	if (a.spring != b.spring) return a.spring > b.spring;
	bool a_is_exact = a.entered_seed == a.effective_seed;
	bool b_is_exact = b.entered_seed == b.effective_seed;
	if (a_is_exact != b_is_exact) return a_is_exact;
	return a.entered_seed < b.entered_seed;
}

} // namespace

int main(int argc, char **argv) {
	if (argc == 3 && std::string(argv[1]) == "calendar") {
		uint64_t seed = 0;
		if (!parse_u64(argv[2], seed) || seed > 999999999ULL) {
			std::cerr << "calendar seed must be in 0..999999999\n";
			return 2;
		}
		print_calendar(static_cast<uint32_t>(seed));
		return 0;
	}
	if (argc > 5) {
		std::cerr << "usage: search-switch-rain [begin] [end-exclusive<=1000000000] [threads] [total|spring|summer]\n"
			<< "       search-switch-rain calendar ENTERED_SEED\n";
		return 2;
	}
	uint64_t begin = 0;
	uint64_t end = 1000000000ULL;
	uint64_t parsed_threads = std::max(1U, std::thread::hardware_concurrency());
	if (argc > 1 && !parse_u64(argv[1], begin)) begin = end;
	if (argc > 2 && !parse_u64(argv[2], end)) begin = end;
	if (argc > 3 && !parse_u64(argv[3], parsed_threads)) parsed_threads = 0;
	unsigned thread_count = static_cast<unsigned>(parsed_threads);
	std::string objective = argc > 4 ? argv[4] : "total";
	if (begin >= end || end > 1000000000ULL || thread_count == 0
		|| parsed_threads > static_cast<uint64_t>(UINT32_MAX)
		|| (objective != "total" && objective != "spring" && objective != "summer")) {
		std::cerr << "usage: search-switch-rain [begin] [end-exclusive<=1000000000] [threads] [total|spring|summer]\n"
			<< "       search-switch-rain calendar ENTERED_SEED\n";
		return 2;
	}
	auto metric = [&](const Result &result) {
		if (objective == "spring") return static_cast<int>(result.spring);
		if (objective == "summer") return static_cast<int>(result.summer);
		return result.total();
	};

	constexpr uint64_t chunk_size = 100000;
	std::atomic<uint64_t> next{begin};
	std::atomic<int> best_total{0};
	std::mutex results_mutex;
	std::vector<Result> best_results;
	uint64_t best_count = 0;
	std::vector<std::thread> workers;
	auto started = std::chrono::steady_clock::now();

	for (unsigned worker = 0; worker < thread_count; ++worker) {
		workers.emplace_back([&] {
			while (true) {
				uint64_t chunk_begin = next.fetch_add(chunk_size, std::memory_order_relaxed);
				if (chunk_begin >= end) break;
				uint64_t chunk_end = std::min(end, chunk_begin + chunk_size);
				for (uint64_t raw_seed = chunk_begin; raw_seed < chunk_end; ++raw_seed) {
					uint32_t entered_seed = static_cast<uint32_t>(raw_seed);
					// Every rounded result is itself an enterable integer. Searching
					// only stable entries removes aliases without losing any possible
					// effective Game ID.
					if (effective_switch_seed(entered_seed) != entered_seed) continue;
					int current_best = best_total.load(std::memory_order_relaxed);
					Result result = score_seed(entered_seed, objective == "total" ? current_best : 0);
					int result_metric = metric(result);
					if (result_metric < current_best) continue;
					std::lock_guard<std::mutex> lock(results_mutex);
					current_best = best_total.load(std::memory_order_relaxed);
					if (result_metric > current_best) {
						best_total.store(result_metric, std::memory_order_relaxed);
						best_results.clear();
						best_count = 0;
					}
					if (result_metric == best_total.load(std::memory_order_relaxed)) {
						++best_count;
						if (best_results.size() < 10000) best_results.push_back(result);
					}
				}
			}
		});
	}
	for (auto &worker : workers) worker.join();

	std::sort(best_results.begin(), best_results.end(), [&](const Result &a, const Result &b) {
		if (metric(a) != metric(b)) return metric(a) > metric(b);
		return better_secondary(a, b);
	});
	auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
	std::cout << "searched=[" << begin << ',' << end << ") threads=" << thread_count
		<< " seconds=" << std::fixed << std::setprecision(3) << elapsed
		<< " objective=" << objective << " best=" << best_total.load() << " ties=" << best_count << "\n";
	for (const Result &result : best_results) {
		if (metric(result) != best_total.load()) continue;
		std::cout << "entered_seed=" << result.entered_seed
			<< " effective_seed=" << result.effective_seed
			<< " spring=" << static_cast<int>(result.spring)
			<< " summer=" << static_cast<int>(result.summer) << " total=" << result.total() << "\n";
	}
	if (best_count > best_results.size()) {
		std::cout << "results_truncated_after=" << best_results.size() << "\n";
	}
}
