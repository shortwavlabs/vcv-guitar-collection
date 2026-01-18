# Cabinet Simulator Risk Assessment

## Overview

This document identifies potential risks in the Cabinet Simulator (CabSim) module implementation and provides mitigation strategies for each.

## Risk Categories

### 1. Performance Risks

#### 1.1 High CPU Usage from Convolution

**Risk Level:** HIGH

**Description:** 
FFT-based convolution with long impulse responses can be CPU-intensive, especially when running two convolvers for IR blending.

**Note:** Mono-only processing reduces CPU load compared to stereo.

**Impact:**
- Audio dropouts/glitches
- Unable to use multiple instances
- Poor user experience

**Mitigation Strategies:**
1. **Limit IR Length:** Cap maximum IR length at 1 second (48,000 samples @ 48kHz)
2. **Optimize Block Size:** Use 256 or 512 sample blocks for efficient FFT
3. **Lazy Convolver B:** Only process second convolver when blend > 0 or < 1
4. **SIMD Optimization:** Use PFFFT's optimized FFT (already included in VCV Rack)

**Metrics to Monitor:**
- CPU usage per convolver instance
- Audio buffer underruns

#### 1.2 Memory Usage with Long IRs

**Risk Level:** MEDIUM

**Description:**
Each convolver allocates memory proportional to IR length for FFT buffers.

**Impact:**
- High memory consumption with multiple instances
- Potential memory exhaustion on low-RAM systems

**Mitigation Strategies:**
1. **Memory Limit:** Max ~400KB per convolver (1 second IR)
2. **Lazy Allocation:** Only allocate memory when IR is loaded
3. **Release Memory:** Free convolver memory when IR is unloaded

**Estimated Memory per Instance:**
| IR Length | Memory (per convolver) | Total (dual IR) |
|-----------|------------------------|-----------------|
| 200ms | ~80 KB | ~160 KB |
| 500ms | ~200 KB | ~400 KB |
| 1000ms | ~400 KB | ~800 KB |

---

### 2. Threading Risks

#### 2.1 Audio Thread Blocking During IR Load

**Risk Level:** HIGH

**Description:**
Loading and resampling IRs on the audio thread would cause audio dropouts.

**Impact:**
- Audio glitches during IR loading
- Unresponsive UI

**Mitigation Strategies:**
1. **Async Loading:** Load IRs on separate thread
2. **Atomic Swap:** Use mutex-protected pointer swap
3. **Loading Indicator:** Show "loading" state to user

**Implementation Pattern:**
```cpp
// Thread-safe convolver swap
std::mutex convMutex;
std::unique_ptr<Convolver> newConvolver;  // Prepared on load thread

void swapConvolver(int slot) {
    std::lock_guard<std::mutex> lock(convMutex);
    convolvers[slot] = std::move(newConvolver);
}
```

#### 2.2 Race Conditions in Dual Convolver Access

**Risk Level:** MEDIUM

**Description:**
Both convolvers are accessed during blend processing; one might be swapped while reading.

**Impact:**
- Crash or undefined behavior
- Audio glitches

**Mitigation Strategies:**
1. **Coarse Mutex:** Lock during entire process block
2. **Copy-on-Read:** Copy convolver pointers under lock, process outside
3. **Atomic Pointers:** Use atomic shared_ptr operations

**Preferred Approach:**
```cpp
void process() {
    // Copy pointers under lock
    std::unique_lock<std::mutex> lock(convMutex);
    auto convA = convolverA.get();
    auto convB = convolverB.get();
    lock.unlock();
    
    // Process without lock
    if (convA) convA->processBlock(...);
    if (convB) convB->processBlock(...);
}
```

---

### 3. Audio Quality Risks

#### 3.1 Sample Rate Conversion Artifacts

**Risk Level:** MEDIUM

**Description:**
Resampling IRs from their native sample rate to engine rate may introduce artifacts.

**Impact:**
- Audible distortion or aliasing
- Altered frequency response

**Mitigation Strategies:**
1. **High-Quality Resampler:** Use quality setting 8+ (out of 10)
2. **Pre-filter:** Apply anti-aliasing filter before downsampling
3. **Native Rate Warning:** Show indicator when IR rate ≠ engine rate
4. **Recommend Matching:** Advise users to use IRs at engine sample rate

**Quality vs Speed Tradeoffs:**
| Quality Setting | Latency | Artifacts | Use Case |
|-----------------|---------|-----------|----------|
| 4-6 | Low | Some | Real-time |
| 8-10 | Higher | Minimal | IR loading |

#### 3.2 Normalization Level Jumps

**Risk Level:** LOW

**Description:**
Enabling/disabling normalization causes sudden volume changes.

**Impact:**
- Unexpected loud output
- User surprise/ear damage potential

**Mitigation Strategies:**
1. **Soft Transition:** Crossfade when toggling normalization
2. **Reload Required:** Only apply normalization on next load
3. **Warning:** Show volume change warning in UI

**Recommendation:** Apply normalization only at load time, require reload to change.

#### 3.3 Phase Cancellation in IR Blending

**Risk Level:** MEDIUM

**Description:**
Blending two IRs with different phase characteristics can cause frequency cancellation.

**Impact:**
- Thin or hollow tone at certain blend positions
- Unexpected frequency response

**Mitigation Strategies:**
1. **Document Limitation:** Inform users about phase considerations
2. **Phase Align Option:** (Future) Auto-align IR phases
3. **Blend Preview:** Allow preview before committing blend position

---

### 4. Stability Risks

#### 4.1 Invalid WAV File Handling

**Risk Level:** HIGH

**Description:**
Malformed or unsupported WAV files could crash the module.

**Impact:**
- Module crash
- VCV Rack crash
- Data loss

**Mitigation Strategies:**
1. **Validation:** Check WAV header before processing
2. **Try-Catch:** Wrap all file operations
3. **Error Messages:** Show user-friendly error on failure
4. **Supported Formats:** Document supported formats clearly

**Supported WAV Formats:**
- PCM 16-bit
- PCM 24-bit  
- PCM 32-bit
- IEEE Float 32-bit
- Mono or Stereo (stereo converted to mono)

**Not Supported:**
- Compressed formats (MP3, AAC, etc.)
- Multi-channel (>2 channels)
- Non-WAV files

#### 4.2 Missing IR Files on Patch Load

**Risk Level:** MEDIUM

**Description:**
Saved IR paths may become invalid if files are moved/deleted.

**Impact:**
- IR fails to load on patch open
- Silent output (unexpected)

**Mitigation Strategies:**
1. **Graceful Fail:** Don't crash, show "IR missing" indicator
2. **Relative Paths:** Store relative paths when possible
3. **Patch Storage:** Copy IRs to patch storage directory
4. **Warning Dialog:** Notify user of missing files

**Path Storage Options:**
| Strategy | Pros | Cons |
|----------|------|------|
| Absolute Path | Simple | Breaks if moved |
| Relative Path | Portable | Assumes structure |
| Patch Storage | Self-contained | Increases file size |

**Recommendation:** Start with absolute paths, add patch storage option later.

#### 4.3 Sample Rate Change Crashes

**Risk Level:** MEDIUM

**Description:**
Changing engine sample rate while IRs are loaded requires IR resampling.

**Impact:**
- Potential crash if not handled
- Audio artifacts if IR not updated

**Mitigation Strategies:**
1. **Reload IRs:** Re-resample IRs on sample rate change
2. **Clear Convolvers:** Reset convolver state on rate change
3. **Async Reload:** Don't block audio thread during reload

**Implementation:**
```cpp
void onSampleRateChange(const SampleRateChangeEvent& e) {
    currentSampleRate = e.sampleRate;
    
    // Reload IRs at new sample rate (async)
    if (!irPathA.empty()) loadIR(0, irPathA);
    if (!irPathB.empty()) loadIR(1, irPathB);
}
```

---

### 5. Usability Risks

#### 5.1 Confusing Blend Behavior with One IR

**Risk Level:** LOW

**Description:**
Blend control doesn't make sense when only one IR is loaded.

**Impact:**
- User confusion
- Unexpected silence at blend extremes

**Mitigation Strategies:**
1. **Disable Blend:** Gray out blend when only one IR
2. **Smart Blend:** Treat blend as wet/dry when single IR
3. **Document:** Clear explanation in manual

**Recommendation:** When only IR A loaded, ignore blend. When only IR B loaded, blend acts as wet/dry.

#### 5.2 Latency Not Communicated

**Risk Level:** LOW

**Description:**
Convolution introduces latency (block size samples) which isn't obvious to users.

**Impact:**
- Timing issues when combining with other modules
- Confusion about "sluggish" response

**Mitigation Strategies:**
1. **Document Latency:** State in manual and module description
2. **Show Latency:** Display current latency in context menu
3. **Latency Options:** (Future) Adjustable block size for latency/CPU tradeoff

**Latency Values:**
| Block Size | Latency @ 48kHz |
|------------|-----------------|
| 128 | 2.67 ms |
| 256 | 5.33 ms |
| 512 | 10.67 ms |

---

### 6. Compatibility Risks

#### 6.1 Platform-Specific Issues

**Risk Level:** LOW

**Description:**
File dialog behavior differs across macOS, Windows, Linux.

**Impact:**
- Inconsistent user experience
- Potential crashes on some platforms

**Mitigation Strategies:**
1. **Use osdialog:** VCV Rack's cross-platform dialog library
2. **Test All Platforms:** CI testing on all platforms
3. **Path Normalization:** Handle path separators correctly

#### 6.2 VCV Rack Version Compatibility

**Risk Level:** LOW

**Description:**
API changes in future VCV Rack versions could break module.

**Impact:**
- Module fails to load
- Incorrect behavior

**Mitigation Strategies:**
1. **minRackVersion:** Set appropriate minimum version (2.6.0)
2. **Stable APIs:** Only use documented, stable APIs
3. **Version Checks:** Conditional compilation for version-specific code

---

## Risk Summary Matrix

| Risk | Likelihood | Impact | Priority | Status |
|------|------------|--------|----------|--------|
| CPU Usage | Medium | High | HIGH | Mitigated |
| Thread Blocking | High | High | HIGH | Mitigated |
| Race Conditions | Medium | High | HIGH | Mitigated |
| Invalid WAV | Medium | High | HIGH | Mitigated |
| Sample Rate Artifacts | Medium | Medium | MEDIUM | Mitigated |
| Missing IR Files | Medium | Medium | MEDIUM | Mitigated |
| Sample Rate Change | Medium | Medium | MEDIUM | Mitigated |
| Memory Usage | Low | Medium | LOW | Monitored |
| Phase Cancellation | Medium | Low | LOW | Documented |
| Normalization Jumps | Low | Low | LOW | Mitigated |
| Blend Confusion | Low | Low | LOW | Mitigated |
| Latency Communication | Low | Low | LOW | Documented |
| Platform Issues | Low | Medium | LOW | Tested |
| Version Compatibility | Low | Medium | LOW | Mitigated |

## Contingency Plans

### If CPU Usage is Unacceptable:
1. Reduce default block size
2. Implement convolver quality settings
3. Add CPU usage meter to module

### If Memory Usage is Problematic:
1. Reduce maximum IR length
2. Implement IR compression/streaming
3. Add memory usage warnings

### If Threading Issues Persist:
1. Switch to single-threaded loading with progress bar
2. Implement lock-free convolver swap
3. Use VCV Rack's async loading utilities

### If Audio Quality Issues Reported:
1. Increase resampler quality
2. Add quality settings option
3. Recommend native sample rate IRs

## Monitoring and Metrics

After release, monitor:
1. Crash reports (via user feedback)
2. Performance complaints
3. Feature requests
4. Compatibility issues

## Review Schedule

- **Pre-Implementation:** Review risks with team
- **Post-Phase 2 (DSP):** Re-evaluate performance risks
- **Pre-Release:** Full risk review
- **Post-Release (1 week):** User feedback review
- **Post-Release (1 month):** Stability assessment
