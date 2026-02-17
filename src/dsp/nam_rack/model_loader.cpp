#include "model_loader.h"
#include "linear.h"
#include "convnet.h"
#include "lstm.h"
#include "wavenet.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cmath>

// Jansson JSON library (from VCV Rack SDK)
#include <jansson.h>

namespace nam {

// ============================================================================
// Version parsing
// ============================================================================

Version parseVersion(const std::string& versionStr) {
    Version version;
    version.major = 0;
    version.minor = 0;
    version.patch = 0;

    // Split by '.'
    std::stringstream ss(versionStr);
    std::string part;

    if (std::getline(ss, part, '.')) {
        version.major = std::stoi(part);
    }
    if (std::getline(ss, part, '.')) {
        version.minor = std::stoi(part);
    }
    if (std::getline(ss, part, '.')) {
        version.patch = std::stoi(part);
    }

    return version;
}

void verifyConfigVersion(const std::string& versionStr) {
    Version version = parseVersion(versionStr);

    // NAM version 0.5.x is supported
    if (version.major != 0 || version.minor != 5) {
        std::stringstream ss;
        ss << "Model config is an unsupported version " << versionStr
           << ". Try either converting the model to a more recent version, or "
           << "update your version of the NAM plugin.";
        throw std::runtime_error(ss.str());
    }
}

// ============================================================================
// JSON utilities
// ============================================================================

namespace {

std::vector<float> getWeightsFromJson(json_t* j) {
    json_t* weightsJ = json_object_get(j, "weights");
    if (!weightsJ || !json_is_array(weightsJ)) {
        throw std::runtime_error("Corrupted model file is missing weights.");
    }

    std::vector<float> weights;
    size_t size = json_array_size(weightsJ);
    weights.reserve(size);

    for (size_t i = 0; i < size; i++) {
        json_t* valJ = json_array_get(weightsJ, i);
        if (json_is_real(valJ)) {
            weights.push_back(static_cast<float>(json_real_value(valJ)));
        } else if (json_is_integer(valJ)) {
            weights.push_back(static_cast<float>(json_integer_value(valJ)));
        } else {
            weights.push_back(0.0f);
        }
    }

    return weights;
}

double getSampleRateFromJson(json_t* j) {
    json_t* srJ = json_object_get(j, "sample_rate");
    if (srJ && json_is_real(srJ)) {
        return json_real_value(srJ);
    } else if (srJ && json_is_integer(srJ)) {
        return static_cast<double>(json_integer_value(srJ));
    }
    return -1.0;
}

std::string getArchitectureFromJson(json_t* j) {
    json_t* archJ = json_object_get(j, "architecture");
    if (archJ && json_is_string(archJ)) {
        return json_string_value(archJ);
    }
    throw std::runtime_error("Model missing architecture field.");
}

std::string getVersionFromJson(json_t* j) {
    json_t* verJ = json_object_get(j, "version");
    if (verJ && json_is_string(verJ)) {
        return json_string_value(verJ);
    }
    throw std::runtime_error("Model missing version field.");
}

std::string jsonToString(json_t* j) {
    if (!j) return "{}";
    char* str = json_dumps(j, JSON_COMPACT);
    if (!str) return "{}";
    std::string result(str);
    free(str);
    return result;
}

// Helper to get double from metadata
struct OptionalDouble {
    bool have = false;
    double value = 0.0;
};

OptionalDouble getMetadataDouble(json_t* metadata, const char* key) {
    OptionalDouble result;
    if (!metadata) return result;

    json_t* valJ = json_object_get(metadata, key);
    if (valJ && !json_is_null(valJ)) {
        if (json_is_real(valJ)) {
            result.value = json_real_value(valJ);
            result.have = true;
        } else if (json_is_integer(valJ)) {
            result.value = static_cast<double>(json_integer_value(valJ));
            result.have = true;
        }
    }
    return result;
}

} // anonymous namespace

// ============================================================================
// Factory registry
// ============================================================================

namespace factory {

FactoryRegistry& FactoryRegistry::instance() {
    static FactoryRegistry inst;
    return inst;
}

void FactoryRegistry::registerFactory(const std::string& key, FactoryFunction func) {
    if (mFactories.find(key) != mFactories.end()) {
        throw std::runtime_error("Factory already registered for key: " + key);
    }
    mFactories[key] = func;
}

std::unique_ptr<DSP> FactoryRegistry::create(const std::string& name,
                                             json_t* config,
                                             std::vector<float>& weights,
                                             double expectedSampleRate) const {
    auto it = mFactories.find(name);
    if (it != mFactories.end()) {
        return it->second(config, weights, expectedSampleRate);
    }
    throw std::runtime_error("Factory not found for architecture: " + name);
}

RegisterFactory::RegisterFactory(const std::string& name, FactoryFunction factory) {
    FactoryRegistry::instance().registerFactory(name, factory);
}

// ============================================================================
// Factory functions for each architecture
// ============================================================================

std::unique_ptr<DSP> createLinear(json_t* config, std::vector<float>& weights, double expectedSampleRate) {
    // Linear config: {"receptive_field": N}
    json_t* rfJ = json_object_get(config, "receptive_field");
    if (!rfJ || !json_is_integer(rfJ)) {
        throw std::runtime_error("Linear config missing receptive_field");
    }
    int receptive_field = static_cast<int>(json_integer_value(rfJ));

    // Check for bias
    bool bias = true;
    json_t* biasJ = json_object_get(config, "bias");
    if (biasJ && json_is_boolean(biasJ)) {
        bias = json_boolean_value(biasJ);
    }

    return linear::create(receptive_field, bias, weights, expectedSampleRate);
}

std::unique_ptr<DSP> createConvNet(json_t* config, std::vector<float>& weights, double expectedSampleRate) {
    // ConvNet config: {"channels": N, "dilations": [...], "batchnorm": bool, "activation": "Tanh"}
    json_t* channelsJ = json_object_get(config, "channels");
    if (!channelsJ || !json_is_integer(channelsJ)) {
        throw std::runtime_error("ConvNet config missing channels");
    }
    int channels = static_cast<int>(json_integer_value(channelsJ));

    // Get dilations array
    json_t* dilationsJ = json_object_get(config, "dilations");
    if (!dilationsJ || !json_is_array(dilationsJ)) {
        throw std::runtime_error("ConvNet config missing dilations array");
    }
    std::vector<int> dilations;
    size_t numDilations = json_array_size(dilationsJ);
    for (size_t i = 0; i < numDilations; i++) {
        json_t* dJ = json_array_get(dilationsJ, i);
        if (json_is_integer(dJ)) {
            dilations.push_back(static_cast<int>(json_integer_value(dJ)));
        }
    }

    // Get batchnorm flag
    bool batchnorm = false;
    json_t* bnJ = json_object_get(config, "batchnorm");
    if (bnJ && json_is_boolean(bnJ)) {
        batchnorm = json_boolean_value(bnJ);
    }

    // Get activation
    std::string activation = "Tanh";
    json_t* actJ = json_object_get(config, "activation");
    if (actJ && json_is_string(actJ)) {
        activation = json_string_value(actJ);
    }

    // Get groups (default 1)
    int groups = 1;
    json_t* groupsJ = json_object_get(config, "groups");
    if (groupsJ && json_is_integer(groupsJ)) {
        groups = static_cast<int>(json_integer_value(groupsJ));
    }

    return convnet::create(channels, dilations, batchnorm, activation, weights, expectedSampleRate, groups);
}

std::unique_ptr<DSP> createLSTM(json_t* config, std::vector<float>& weights, double expectedSampleRate) {
    // LSTM config: {"num_layers": N, "input_size": N, "hidden_size": N}
    json_t* numLayersJ = json_object_get(config, "num_layers");
    if (!numLayersJ || !json_is_integer(numLayersJ)) {
        throw std::runtime_error("LSTM config missing num_layers");
    }
    int num_layers = static_cast<int>(json_integer_value(numLayersJ));

    json_t* inputSizeJ = json_object_get(config, "input_size");
    if (!inputSizeJ || !json_is_integer(inputSizeJ)) {
        throw std::runtime_error("LSTM config missing input_size");
    }
    int input_size = static_cast<int>(json_integer_value(inputSizeJ));

    json_t* hiddenSizeJ = json_object_get(config, "hidden_size");
    if (!hiddenSizeJ || !json_is_integer(hiddenSizeJ)) {
        throw std::runtime_error("LSTM config missing hidden_size");
    }
    int hidden_size = static_cast<int>(json_integer_value(hiddenSizeJ));

    return lstm::create(num_layers, input_size, hidden_size, weights, expectedSampleRate);
}

std::unique_ptr<DSP> createWaveNet(json_t* config, std::vector<float>& weights, double expectedSampleRate) {
    // WaveNet config is more complex with layer arrays
    json_t* headScaleJ = json_object_get(config, "head_scale");
    float head_scale = 1.0f;
    if (headScaleJ && json_is_real(headScaleJ)) {
        head_scale = static_cast<float>(json_real_value(headScaleJ));
    } else if (headScaleJ && json_is_integer(headScaleJ)) {
        head_scale = static_cast<float>(json_integer_value(headScaleJ));
    }

    bool with_head = false;
    json_t* withHeadJ = json_object_get(config, "with_head");
    if (withHeadJ && json_is_boolean(withHeadJ)) {
        with_head = json_boolean_value(withHeadJ);
    }

    // Get layer arrays
    json_t* layerArraysJ = json_object_get(config, "layer_arrays");
    if (!layerArraysJ || !json_is_array(layerArraysJ)) {
        throw std::runtime_error("WaveNet config missing layer_arrays");
    }

    std::vector<wavenet::LayerArrayConfig> layerArrayConfigs;
    size_t numArrays = json_array_size(layerArraysJ);

    for (size_t i = 0; i < numArrays; i++) {
        json_t* arrJ = json_array_get(layerArraysJ, i);
        wavenet::LayerArrayConfig lac;

        // Input size
        json_t* inputSizeJ = json_object_get(arrJ, "input_size");
        lac.inputSize = inputSizeJ && json_is_integer(inputSizeJ) ?
            static_cast<int>(json_integer_value(inputSizeJ)) : 1;

        // Condition size
        json_t* condSizeJ = json_object_get(arrJ, "condition_size");
        lac.conditionSize = condSizeJ && json_is_integer(condSizeJ) ?
            static_cast<int>(json_integer_value(condSizeJ)) : 1;

        // Head size
        json_t* headSizeJ = json_object_get(arrJ, "head_size");
        lac.headSize = headSizeJ && json_is_integer(headSizeJ) ?
            static_cast<int>(json_integer_value(headSizeJ)) : 1;

        // Channels
        json_t* channelsJ = json_object_get(arrJ, "channels");
        if (!channelsJ || !json_is_integer(channelsJ)) {
            throw std::runtime_error("WaveNet layer array missing channels");
        }
        lac.channels = static_cast<int>(json_integer_value(channelsJ));

        // Bottleneck
        json_t* bottleneckJ = json_object_get(arrJ, "bottleneck");
        if (!bottleneckJ || !json_is_integer(bottleneckJ)) {
            throw std::runtime_error("WaveNet layer array missing bottleneck");
        }
        lac.bottleneck = static_cast<int>(json_integer_value(bottleneckJ));

        // Kernel size
        json_t* kernelJ = json_object_get(arrJ, "kernel_size");
        lac.kernelSize = kernelJ && json_is_integer(kernelJ) ?
            static_cast<int>(json_integer_value(kernelJ)) : 3;

        // Dilations array
        json_t* dilationsJ = json_object_get(arrJ, "dilations");
        if (dilationsJ && json_is_array(dilationsJ)) {
            size_t numDilations = json_array_size(dilationsJ);
            for (size_t d = 0; d < numDilations; d++) {
                json_t* dJ = json_array_get(dilationsJ, d);
                if (json_is_integer(dJ)) {
                    lac.dilations.push_back(static_cast<int>(json_integer_value(dJ)));
                }
            }
        }

        // Activation
        json_t* actJ = json_object_get(arrJ, "activation");
        lac.activation = actJ && json_is_string(actJ) ?
            json_string_value(actJ) : "Tanh";

        // Gated
        json_t* gatedJ = json_object_get(arrJ, "gated");
        lac.gated = gatedJ && json_is_boolean(gatedJ) ?
            json_boolean_value(gatedJ) : true;

        // Head bias
        json_t* headBiasJ = json_object_get(arrJ, "head_bias");
        lac.headBias = headBiasJ && json_is_boolean(headBiasJ) ?
            json_boolean_value(headBiasJ) : true;

        // Groups
        json_t* groupsInputJ = json_object_get(arrJ, "groups_input");
        lac.groupsInput = groupsInputJ && json_is_integer(groupsInputJ) ?
            static_cast<int>(json_integer_value(groupsInputJ)) : 1;

        json_t* groups1x1J = json_object_get(arrJ, "groups_1x1");
        lac.groups1x1 = groups1x1J && json_is_integer(groups1x1J) ?
            static_cast<int>(json_integer_value(groups1x1J)) : 1;

        layerArrayConfigs.push_back(lac);
    }

    return wavenet::create(layerArrayConfigs, head_scale, with_head, weights, expectedSampleRate);
}

// Register factories
static RegisterFactory registerLinear("Linear", createLinear);
static RegisterFactory registerConvNet("ConvNet", createConvNet);
static RegisterFactory registerLSTM("LSTM", createLSTM);
static RegisterFactory registerWaveNet("WaveNet", createWaveNet);

} // namespace factory

// ============================================================================
// Model loading
// ============================================================================

std::unique_ptr<DSP> loadModel(const std::string& filename) {
    ModelConfig config;
    return loadModel(filename, config);
}

std::unique_ptr<DSP> loadModel(const std::string& filename, ModelConfig& config) {
    // Load and parse JSON file
    json_error_t error;
    json_t* root = json_load_file(filename.c_str(), 0, &error);

    if (!root) {
        std::stringstream ss;
        ss << "Failed to load model file: " << error.text
           << " at line " << error.line;
        throw std::runtime_error(ss.str());
    }

    try {
        // Get version and verify
        config.version = getVersionFromJson(root);
        verifyConfigVersion(config.version);

        // Get architecture
        config.architecture = getArchitectureFromJson(root);

        // Get config object
        json_t* configObj = json_object_get(root, "config");
        config.configJson = jsonToString(configObj);

        // Get metadata
        json_t* metadata = json_object_get(root, "metadata");
        config.metadataJson = jsonToString(metadata);

        // Get weights
        config.weights = getWeightsFromJson(root);

        // Get sample rate
        config.expectedSampleRate = getSampleRateFromJson(root);

        // Create DSP
        json_decref(root);
        return createDSP(config);

    } catch (...) {
        json_decref(root);
        throw;
    }
}

std::unique_ptr<DSP> createDSP(ModelConfig& config) {
    // Parse config JSON
    json_t* configJ = nullptr;
    json_t* metadataJ = nullptr;

    if (!config.configJson.empty()) {
        json_error_t error;
        configJ = json_loads(config.configJson.c_str(), 0, &error);
    }

    if (!config.metadataJson.empty()) {
        json_error_t error;
        metadataJ = json_loads(config.metadataJson.c_str(), 0, &error);
    }

    // Create DSP using factory
    std::unique_ptr<DSP> dsp = factory::FactoryRegistry::instance().create(
        config.architecture, configJ, config.weights, config.expectedSampleRate
    );

    // Set metadata
    if (metadataJ) {
        OptionalDouble loudness = getMetadataDouble(metadataJ, "loudness");
        if (loudness.have) {
            dsp->setLoudness(loudness.value);
        }

        OptionalDouble inputLevel = getMetadataDouble(metadataJ, "input_level_dbu");
        if (inputLevel.have) {
            dsp->setInputLevel(inputLevel.value);
        }

        OptionalDouble outputLevel = getMetadataDouble(metadataJ, "output_level_dbu");
        if (outputLevel.have) {
            dsp->setOutputLevel(outputLevel.value);
        }

        json_decref(metadataJ);
    }

    if (configJ) {
        json_decref(configJ);
    }

    // Prewarm the model
    dsp->prewarm();

    return dsp;
}

double getSampleRateFromFile(const std::string& filename) {
    json_error_t error;
    json_t* root = json_load_file(filename.c_str(), 0, &error);

    if (!root) {
        return -1.0;
    }

    double sampleRate = getSampleRateFromJson(root);
    json_decref(root);
    return sampleRate;
}

} // namespace nam
