#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

/**
 * WavFile - Simple WAV file loader
 * 
 * Supports:
 * - PCM formats: 8-bit, 16-bit, 24-bit, 32-bit integer
 * - 32-bit float format
 * - Mono and stereo files
 * - Standard RIFF WAV structure
 */
class WavFile {
public:
    WavFile() = default;
    ~WavFile() = default;
    
    /**
     * Load a WAV file from disk
     * @param path File path to load
     * @return true if successful
     */
    bool load(const std::string& path) {
        reset();
        
        FILE* file = fopen(path.c_str(), "rb");
        if (!file) {
            return false;
        }
        
        bool success = parseFile(file);
        fclose(file);
        
        if (success) {
            filePath = path;
        }
        
        return success;
    }
    
    /**
     * Reset all data
     */
    void reset() {
        samples.clear();
        channels = 0;
        sampleRate = 0;
        filePath.clear();
    }
    
    // Getters
    const std::vector<float>& getSamples() const { return samples; }
    uint32_t getChannels() const { return channels; }
    uint32_t getSampleRate() const { return sampleRate; }
    size_t getFrameCount() const { return channels > 0 ? samples.size() / channels : 0; }
    const std::string& getPath() const { return filePath; }
    bool isLoaded() const { return !samples.empty(); }
    
private:
    std::vector<float> samples;
    uint32_t channels = 0;
    uint32_t sampleRate = 0;
    std::string filePath;
    
    // WAV format constants
    static constexpr uint16_t WAVE_FORMAT_PCM = 1;
    static constexpr uint16_t WAVE_FORMAT_IEEE_FLOAT = 3;
    static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;
    
    // RIFF chunk header
    struct ChunkHeader {
        char id[4];
        uint32_t size;
    };
    
    // WAV format chunk
    struct FmtChunk {
        uint16_t audioFormat;
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
    };
    
    bool parseFile(FILE* file) {
        // Read RIFF header
        char riffId[4];
        if (fread(riffId, 1, 4, file) != 4 || memcmp(riffId, "RIFF", 4) != 0) {
            return false;
        }
        
        uint32_t fileSize;
        if (fread(&fileSize, 4, 1, file) != 1) {
            return false;
        }
        
        char waveId[4];
        if (fread(waveId, 1, 4, file) != 4 || memcmp(waveId, "WAVE", 4) != 0) {
            return false;
        }
        
        // Parse chunks
        FmtChunk fmt = {};
        bool foundFmt = false;
        bool foundData = false;
        
        while (!foundData) {
            ChunkHeader chunk;
            if (fread(&chunk.id, 1, 4, file) != 4) {
                break;
            }
            if (fread(&chunk.size, 4, 1, file) != 1) {
                return false;
            }
            
            if (memcmp(chunk.id, "fmt ", 4) == 0) {
                // Format chunk
                if (chunk.size < sizeof(FmtChunk)) {
                    return false;
                }
                
                if (fread(&fmt, sizeof(FmtChunk), 1, file) != 1) {
                    return false;
                }
                
                // Handle extensible format
                if (fmt.audioFormat == WAVE_FORMAT_EXTENSIBLE && chunk.size >= 40) {
                    // Skip to the actual format code in extensible header
                    uint16_t cbSize, validBits;
                    uint32_t channelMask;
                    uint16_t subFormat;
                    
                    if (fread(&cbSize, 2, 1, file) != 1) return false;
                    if (fread(&validBits, 2, 1, file) != 1) return false;
                    if (fread(&channelMask, 4, 1, file) != 1) return false;
                    if (fread(&subFormat, 2, 1, file) != 1) return false;
                    
                    fmt.audioFormat = subFormat;
                    
                    // Skip rest of extensible header
                    size_t remaining = chunk.size - sizeof(FmtChunk) - 10;
                    if (remaining > 0) {
                        fseek(file, (long)remaining, SEEK_CUR);
                    }
                } else {
                    // Skip any extra format bytes
                    size_t extra = chunk.size - sizeof(FmtChunk);
                    if (extra > 0) {
                        fseek(file, (long)extra, SEEK_CUR);
                    }
                }
                
                foundFmt = true;
            }
            else if (memcmp(chunk.id, "data", 4) == 0) {
                // Data chunk
                if (!foundFmt) {
                    return false;
                }
                
                if (!readSampleData(file, chunk.size, fmt)) {
                    return false;
                }
                
                foundData = true;
            }
            else {
                // Skip unknown chunk
                fseek(file, chunk.size, SEEK_CUR);
            }
            
            // Chunks are word-aligned
            if (chunk.size & 1) {
                fseek(file, 1, SEEK_CUR);
            }
        }
        
        return foundFmt && foundData && !samples.empty();
    }
    
    bool readSampleData(FILE* file, uint32_t dataSize, const FmtChunk& fmt) {
        channels = fmt.numChannels;
        sampleRate = fmt.sampleRate;
        
        if (channels == 0 || sampleRate == 0) {
            return false;
        }
        
        uint32_t bytesPerSample = fmt.bitsPerSample / 8;
        uint32_t totalSamples = dataSize / bytesPerSample;
        
        samples.reserve(totalSamples);
        
        if (fmt.audioFormat == WAVE_FORMAT_IEEE_FLOAT && fmt.bitsPerSample == 32) {
            // 32-bit float
            return readFloat32(file, totalSamples);
        }
        else if (fmt.audioFormat == WAVE_FORMAT_PCM) {
            switch (fmt.bitsPerSample) {
                case 8:
                    return readPCM8(file, totalSamples);
                case 16:
                    return readPCM16(file, totalSamples);
                case 24:
                    return readPCM24(file, totalSamples);
                case 32:
                    return readPCM32(file, totalSamples);
                default:
                    return false;
            }
        }
        
        return false;
    }
    
    bool readFloat32(FILE* file, uint32_t count) {
        samples.resize(count);
        return fread(samples.data(), sizeof(float), count, file) == count;
    }
    
    bool readPCM8(FILE* file, uint32_t count) {
        std::vector<uint8_t> buffer(count);
        if (fread(buffer.data(), 1, count, file) != count) {
            return false;
        }
        
        samples.resize(count);
        for (uint32_t i = 0; i < count; i++) {
            // 8-bit PCM is unsigned, centered at 128
            samples[i] = (buffer[i] - 128) / 128.f;
        }
        return true;
    }
    
    bool readPCM16(FILE* file, uint32_t count) {
        std::vector<int16_t> buffer(count);
        if (fread(buffer.data(), sizeof(int16_t), count, file) != count) {
            return false;
        }
        
        samples.resize(count);
        constexpr float scale = 1.f / 32768.f;
        for (uint32_t i = 0; i < count; i++) {
            samples[i] = buffer[i] * scale;
        }
        return true;
    }
    
    bool readPCM24(FILE* file, uint32_t count) {
        // Read 3 bytes per sample
        std::vector<uint8_t> buffer(count * 3);
        if (fread(buffer.data(), 1, count * 3, file) != count * 3) {
            return false;
        }
        
        samples.resize(count);
        constexpr float scale = 1.f / 8388608.f;  // 2^23
        for (uint32_t i = 0; i < count; i++) {
            // Little-endian 24-bit signed
            int32_t value = buffer[i * 3] | (buffer[i * 3 + 1] << 8) | (buffer[i * 3 + 2] << 16);
            // Sign extend from 24-bit to 32-bit
            if (value & 0x800000) {
                value |= 0xFF000000;
            }
            samples[i] = value * scale;
        }
        return true;
    }
    
    bool readPCM32(FILE* file, uint32_t count) {
        std::vector<int32_t> buffer(count);
        if (fread(buffer.data(), sizeof(int32_t), count, file) != count) {
            return false;
        }
        
        samples.resize(count);
        constexpr float scale = 1.f / 2147483648.f;  // 2^31
        for (uint32_t i = 0; i < count; i++) {
            samples[i] = buffer[i] * scale;
        }
        return true;
    }
};
