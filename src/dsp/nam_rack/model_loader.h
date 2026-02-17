#pragma once

/**
 * Model loader for NAM models
 *
 * Features:
 * - C++11 compatible
 * - Uses Jansson for JSON parsing (from VCV Rack SDK)
 * - Factory pattern for model instantiation
 * - Supports: Linear, ConvNet, LSTM, WaveNet architectures
 */

#include <memory>
#include <string>
#include <vector>
#include <map>
#include "dsp.h"

// Forward declaration for Jansson
typedef struct json_t json_t;

namespace nam {

/**
 * Model configuration data
 */
struct ModelConfig {
    std::string version;
    std::string architecture;
    std::string configJson;    // Raw JSON string for config
    std::string metadataJson;  // Raw JSON string for metadata
    std::vector<float> weights;
    double expectedSampleRate;
    size_t actualWeightCount;
    size_t expectedWeightCount;
    std::string layerDimsSummary;
    std::string loadDiagnostics;

    ModelConfig()
        : expectedSampleRate(-1.0)
        , actualWeightCount(0)
        , expectedWeightCount(0) {}
};

/**
 * Version structure
 */
struct Version {
    int major;
    int minor;
    int patch;
};

/**
 * Parse version string (e.g., "0.5.0")
 */
Version parseVersion(const std::string& versionStr);

/**
 * Verify that the config version is supported
 */
void verifyConfigVersion(const std::string& versionStr);

/**
 * Load a NAM model from a file
 *
 * @param filename Path to the .nam file
 * @return DSP instance
 * @throws std::runtime_error on error
 */
std::unique_ptr<DSP> loadModel(const std::string& filename);

/**
 * Load a NAM model from a file, also returning the configuration
 *
 * @param filename Path to the .nam file
 * @param config Output parameter for model configuration
 * @return DSP instance
 * @throws std::runtime_error on error
 */
std::unique_ptr<DSP> loadModel(const std::string& filename, ModelConfig& config);

/**
 * Create a DSP instance from a ModelConfig
 *
 * @param config Model configuration
 * @return DSP instance
 * @throws std::runtime_error on error
 */
std::unique_ptr<DSP> createDSP(ModelConfig& config);

/**
 * Get sample rate from a NAM file
 *
 * @param filename Path to the .nam file
 * @return Sample rate, or -1.0 if unknown
 */
double getSampleRateFromFile(const std::string& filename);

namespace factory {

/**
 * Factory function type for creating DSP instances
 */
typedef std::unique_ptr<DSP> (*FactoryFunction)(json_t* config,
                                                 std::vector<float>& weights,
                                                 double expectedSampleRate);

/**
 * Factory registry for DSP creation
 */
class FactoryRegistry {
public:
    static FactoryRegistry& instance();

    void registerFactory(const std::string& key, FactoryFunction func);
    std::unique_ptr<DSP> create(const std::string& name,
                                json_t* config,
                                std::vector<float>& weights,
                                double expectedSampleRate) const;

private:
    FactoryRegistry() {}
    std::map<std::string, FactoryFunction> mFactories;
};

/**
 * Registration helper for automatic factory registration
 */
struct RegisterFactory {
    RegisterFactory(const std::string& name, FactoryFunction factory);
};

} // namespace factory
} // namespace nam

// Convenience macros for factory registration
#define NAM_REGISTER_FACTORY(name, function) \
    static nam::factory::RegisterFactory _register_##name(#name, function)
