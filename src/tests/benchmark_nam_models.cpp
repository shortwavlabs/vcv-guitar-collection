#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../dsp/Nam.h"

namespace fs = std::filesystem;

namespace {

struct BenchCase {
    std::string architecture;
    std::string modelPath;
    int sampleRate = 48000;
};

struct BenchResult {
    std::string architecture;
    std::string modelPath;
    int sampleRate = 48000;
    int blockSize = 128;
    int warmupBlocks = 250;
    int measureBlocks = 1200;
    double meanUsPerBlock = 0.0;
    double p95UsPerBlock = 0.0;
    double maxUsPerBlock = 0.0;
    int outlierBlocks = 0;
    bool success = false;
    std::string error;
};

std::string jsonEscape(const std::string& input) {
    std::ostringstream out;
    for (const char ch : input) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << ch; break;
        }
    }
    return out.str();
}

std::string readPrefix(const std::string& path, std::size_t bytes = 8192) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        return "";
    }
    std::string out;
    out.resize(bytes);
    stream.read(out.data(), static_cast<std::streamsize>(bytes));
    out.resize(static_cast<std::size_t>(stream.gcount()));
    return out;
}

std::string detectArchitectureFromNam(const std::string& path) {
    const std::string head = readPrefix(path);
    if (head.find("\"architecture\":\"LSTM\"") != std::string::npos ||
        head.find("\"architecture\": \"LSTM\"") != std::string::npos) {
        return "LSTM";
    }
    if (head.find("\"architecture\":\"WaveNet\"") != std::string::npos ||
        head.find("\"architecture\": \"WaveNet\"") != std::string::npos) {
        return "WaveNet";
    }
    if (head.find("\"architecture\":\"ConvNet\"") != std::string::npos ||
        head.find("\"architecture\": \"ConvNet\"") != std::string::npos) {
        return "ConvNet";
    }
    if (head.find("\"architecture\":\"Linear\"") != std::string::npos ||
        head.find("\"architecture\": \"Linear\"") != std::string::npos) {
        return "Linear";
    }
    return "Unknown";
}

std::vector<int> parseSampleRates(const std::string& csv) {
    std::vector<int> rates;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        try {
            const int value = std::stoi(token);
            if (value > 0) {
                rates.push_back(value);
            }
        } catch (...) {
        }
    }
    if (rates.empty()) {
        rates = {44100, 48000};
    }
    return rates;
}

std::vector<std::string> discoverModels(const std::string& modelsDir) {
    std::vector<std::string> models;
    if (!fs::exists(modelsDir)) {
        return models;
    }

    for (const auto& entry : fs::recursive_directory_iterator(modelsDir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".nam") {
            models.push_back(entry.path().string());
        }
    }
    std::sort(models.begin(), models.end());
    return models;
}

std::vector<BenchCase> selectCases(const std::vector<std::string>& models,
                                   const std::vector<int>& sampleRates) {
    std::map<std::string, std::string> byArchitecture;
    for (const auto& path : models) {
        const std::string arch = detectArchitectureFromNam(path);
        if (arch == "Unknown") {
            continue;
        }
        if (byArchitecture.find(arch) == byArchitecture.end()) {
            byArchitecture[arch] = path;
        }
    }

    const std::vector<std::string> preferredArch = {"LSTM", "WaveNet", "ConvNet", "Linear"};
    std::vector<BenchCase> out;
    for (const auto& arch : preferredArch) {
        auto it = byArchitecture.find(arch);
        if (it == byArchitecture.end()) {
            continue;
        }
        for (const int sampleRate : sampleRates) {
            BenchCase c;
            c.architecture = arch;
            c.modelPath = it->second;
            c.sampleRate = sampleRate;
            out.push_back(c);
        }
    }
    return out;
}

BenchResult runCase(const BenchCase& c,
                    const int blockSize,
                    const int warmupBlocks,
                    const int measureBlocks) {
    BenchResult result;
    result.architecture = c.architecture;
    result.modelPath = c.modelPath;
    result.sampleRate = c.sampleRate;
    result.blockSize = blockSize;
    result.warmupBlocks = warmupBlocks;
    result.measureBlocks = measureBlocks;

    NamDSP dsp;
    dsp.setSampleRate(static_cast<double>(c.sampleRate));
    dsp.setEcoModeLevel(NamDSP::ECO_OFF);

    if (!dsp.loadModel(c.modelPath)) {
        result.error = "loadModel failed: " + dsp.getLastLoadError();
        return result;
    }

    std::vector<float> input(blockSize, 0.0f);
    std::vector<float> output(blockSize, 0.0f);

    auto fillInput = [&](int blockIndex) {
        const int baseFrame = blockIndex * blockSize;
        for (int i = 0; i < blockSize; i++) {
            const int n = baseFrame + i;
            const float x1 = std::sin(2.0f * static_cast<float>(M_PI) * 110.0f * static_cast<float>(n) / static_cast<float>(c.sampleRate));
            const float x2 = std::sin(2.0f * static_cast<float>(M_PI) * 330.0f * static_cast<float>(n) / static_cast<float>(c.sampleRate));
            input[i] = 0.18f * x1 + 0.06f * x2;
        }
    };

    for (int b = 0; b < warmupBlocks; b++) {
        fillInput(b);
        dsp.process(input.data(), output.data(), blockSize);
    }

    std::vector<double> durations;
    durations.reserve(measureBlocks);
    volatile float sink = 0.0f;

    for (int b = 0; b < measureBlocks; b++) {
        fillInput(warmupBlocks + b);
        const auto t0 = std::chrono::high_resolution_clock::now();
        dsp.process(input.data(), output.data(), blockSize);
        const auto t1 = std::chrono::high_resolution_clock::now();

        sink += output[b % blockSize];

        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        durations.push_back(static_cast<double>(us));
    }
    (void)sink;

    if (durations.empty()) {
        result.error = "no timing data";
        return result;
    }

    const double sum = std::accumulate(durations.begin(), durations.end(), 0.0);
    result.meanUsPerBlock = sum / static_cast<double>(durations.size());

    std::sort(durations.begin(), durations.end());
    const std::size_t p95Index = static_cast<std::size_t>(std::floor(0.95 * static_cast<double>(durations.size() - 1)));
    result.p95UsPerBlock = durations[p95Index];
    result.maxUsPerBlock = durations.back();

    const double blockBudgetUs = 1.0e6 * static_cast<double>(blockSize) / static_cast<double>(c.sampleRate);
    result.outlierBlocks = static_cast<int>(std::count_if(durations.begin(), durations.end(),
        [&](const double us) {
            return us > blockBudgetUs;
        }));

    result.success = true;
    return result;
}

void writeCsv(const std::string& path, const std::vector<BenchResult>& results) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Failed to write CSV: " << path << std::endl;
        return;
    }

    out << "architecture,model_path,sample_rate,block_size,warmup_blocks,measure_blocks,mean_us_per_block,p95_us_per_block,max_us_per_block,outlier_blocks,success,error\n";
    out << std::fixed << std::setprecision(3);
    for (const auto& r : results) {
        out << r.architecture << ",\"" << r.modelPath << "\","
            << r.sampleRate << ","
            << r.blockSize << ","
            << r.warmupBlocks << ","
            << r.measureBlocks << ","
            << r.meanUsPerBlock << ","
            << r.p95UsPerBlock << ","
            << r.maxUsPerBlock << ","
            << r.outlierBlocks << ","
            << (r.success ? "true" : "false") << ",\""
            << r.error << "\"\n";
    }
}

void writeJson(const std::string& path,
               const std::vector<BenchResult>& results,
               const std::string& modelsDir,
               const std::string& sampleRateCsv,
               const int blockSize,
               const int warmupBlocks,
               const int measureBlocks) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Failed to write JSON: " << path << std::endl;
        return;
    }

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"models_dir\": \"" << jsonEscape(modelsDir) << "\",\n";
    out << "  \"sample_rates\": \"" << jsonEscape(sampleRateCsv) << "\",\n";
    out << "  \"block_size\": " << blockSize << ",\n";
    out << "  \"warmup_blocks\": " << warmupBlocks << ",\n";
    out << "  \"measure_blocks\": " << measureBlocks << ",\n";
    out << "  \"results\": [\n";

    for (std::size_t i = 0; i < results.size(); i++) {
        const auto& r = results[i];
        out << "    {\n";
        out << "      \"architecture\": \"" << jsonEscape(r.architecture) << "\",\n";
        out << "      \"model_path\": \"" << jsonEscape(r.modelPath) << "\",\n";
        out << "      \"sample_rate\": " << r.sampleRate << ",\n";
        out << "      \"block_size\": " << r.blockSize << ",\n";
        out << "      \"warmup_blocks\": " << r.warmupBlocks << ",\n";
        out << "      \"measure_blocks\": " << r.measureBlocks << ",\n";
        out << "      \"mean_us_per_block\": " << std::fixed << std::setprecision(3) << r.meanUsPerBlock << ",\n";
        out << "      \"p95_us_per_block\": " << std::fixed << std::setprecision(3) << r.p95UsPerBlock << ",\n";
        out << "      \"max_us_per_block\": " << std::fixed << std::setprecision(3) << r.maxUsPerBlock << ",\n";
        out << "      \"outlier_blocks\": " << r.outlierBlocks << ",\n";
        out << "      \"success\": " << (r.success ? "true" : "false") << ",\n";
        out << "      \"error\": \"" << jsonEscape(r.error) << "\"\n";
        out << "    }";
        if (i + 1 < results.size()) {
            out << ",";
        }
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

void printUsage() {
    std::cout << "Usage: benchmark_nam_models [options]\n"
              << "  --models-dir <path>       (default: res/models)\n"
              << "  --sample-rates <csv>      (default: 44100,48000)\n"
              << "  --block-size <frames>     (default: 128)\n"
              << "  --warmup-blocks <n>       (default: 250)\n"
              << "  --measure-blocks <n>      (default: 1200)\n"
              << "  --output-json <path>      (default: docs/perf/perf-baseline-latest.json)\n"
              << "  --output-csv <path>       (default: docs/perf/perf-baseline-latest.csv)\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string modelsDir = "res/models";
    std::string sampleRateCsv = "44100,48000";
    int blockSize = 128;
    int warmupBlocks = 250;
    int measureBlocks = 1200;
    std::string outputJson = "docs/perf/perf-baseline-latest.json";
    std::string outputCsv = "docs/perf/perf-baseline-latest.csv";

    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        auto next = [&](std::string& value) -> bool {
            if (i + 1 >= argc) {
                return false;
            }
            value = argv[++i];
            return true;
        };

        if (arg == "--models-dir") {
            if (!next(modelsDir)) {
                std::cerr << "Missing value for --models-dir\n";
                return 1;
            }
        } else if (arg == "--sample-rates") {
            if (!next(sampleRateCsv)) {
                std::cerr << "Missing value for --sample-rates\n";
                return 1;
            }
        } else if (arg == "--block-size") {
            std::string value;
            if (!next(value)) {
                std::cerr << "Missing value for --block-size\n";
                return 1;
            }
            blockSize = std::max(1, std::stoi(value));
        } else if (arg == "--warmup-blocks") {
            std::string value;
            if (!next(value)) {
                std::cerr << "Missing value for --warmup-blocks\n";
                return 1;
            }
            warmupBlocks = std::max(1, std::stoi(value));
        } else if (arg == "--measure-blocks") {
            std::string value;
            if (!next(value)) {
                std::cerr << "Missing value for --measure-blocks\n";
                return 1;
            }
            measureBlocks = std::max(1, std::stoi(value));
        } else if (arg == "--output-json") {
            if (!next(outputJson)) {
                std::cerr << "Missing value for --output-json\n";
                return 1;
            }
        } else if (arg == "--output-csv") {
            if (!next(outputCsv)) {
                std::cerr << "Missing value for --output-csv\n";
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown arg: " << arg << "\n";
            printUsage();
            return 1;
        }
    }

    const auto sampleRates = parseSampleRates(sampleRateCsv);
    const auto models = discoverModels(modelsDir);
    const auto cases = selectCases(models, sampleRates);

    std::cout << "Performance benchmark config:\n";
    std::cout << "  models_dir: " << modelsDir << "\n";
    std::cout << "  discovered_models: " << models.size() << "\n";
    std::cout << "  selected_cases: " << cases.size() << "\n";
    std::cout << "  block_size: " << blockSize << "\n";
    std::cout << "  warmup_blocks: " << warmupBlocks << "\n";
    std::cout << "  measure_blocks: " << measureBlocks << "\n";

    std::vector<BenchResult> results;
    results.reserve(cases.size());

    for (const auto& c : cases) {
        std::cout << "\nRunning " << c.architecture << " @ " << c.sampleRate << " Hz\n";
        std::cout << "  model: " << c.modelPath << "\n";
        BenchResult r = runCase(c, blockSize, warmupBlocks, measureBlocks);
        if (r.success) {
            std::cout << std::fixed << std::setprecision(3)
                      << "  mean_us/block=" << r.meanUsPerBlock
                      << " p95_us/block=" << r.p95UsPerBlock
                      << " max_us/block=" << r.maxUsPerBlock
                      << " outliers=" << r.outlierBlocks << "\n";
        } else {
            std::cout << "  FAILED: " << r.error << "\n";
        }
        results.push_back(std::move(r));
    }

    if (results.empty()) {
        std::cerr << "No benchmark cases selected. Ensure models exist and include architecture metadata.\n";
        return 2;
    }

    fs::create_directories(fs::path(outputJson).parent_path());
    fs::create_directories(fs::path(outputCsv).parent_path());

    writeJson(outputJson, results, modelsDir, sampleRateCsv, blockSize, warmupBlocks, measureBlocks);
    writeCsv(outputCsv, results);

    std::cout << "\nWrote: " << outputJson << "\n";
    std::cout << "Wrote: " << outputCsv << "\n";

    const bool hasFailure = std::any_of(results.begin(), results.end(), [](const BenchResult& r) {
        return !r.success;
    });

    return hasFailure ? 3 : 0;
}
