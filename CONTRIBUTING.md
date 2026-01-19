# Contributing to Guitar Tools

Thank you for your interest in contributing to Guitar Tools! This document provides guidelines and instructions for contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [How to Contribute](#how-to-contribute)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)
- [Reporting Bugs](#reporting-bugs)
- [Suggesting Features](#suggesting-features)
- [Documentation](#documentation)
- [Community](#community)

---

## Code of Conduct

### Our Pledge

We are committed to providing a welcoming and inspiring community for all. Please be respectful and constructive in all interactions.

### Expected Behavior

- **Be respectful** of differing viewpoints and experiences
- **Give and gracefully accept** constructive feedback
- **Focus on what is best** for the community
- **Show empathy** towards other community members

### Unacceptable Behavior

- Harassment, trolling, or discriminatory language
- Personal attacks or insults
- Publishing others' private information
- Other conduct that could reasonably be considered inappropriate

### Enforcement

Instances of unacceptable behavior may be reported to [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com). All complaints will be reviewed and investigated promptly and fairly.

---

## Getting Started

### Prerequisites

Before contributing, ensure you have:

- **Git** installed and configured
- **C++ compiler** (GCC 9+, Clang 10+, or MSVC 2019+)
- **VCV Rack SDK 2.6.x**
- **Basic knowledge** of:
  - C++ (especially C++17 features)
  - VCV Rack Module API
  - Git workflows

### Areas for Contribution

We welcome contributions in:

1. **Bug fixes** - Resolve existing issues
2. **Features** - Add new capabilities
3. **Performance** - Optimize DSP or UI
4. **Documentation** - Improve guides and examples
5. **Testing** - Add test coverage
6. **Models/IRs** - Contribute high-quality NAM models or cabinet IRs

---

## Development Setup

### 1. Fork and Clone

```bash
# Fork the repository on GitHub, then:
git clone --recursive https://github.com/YOUR_USERNAME/swv-guitar-collection.git
cd swv-guitar-collection
```

### 2. Install Dependencies

#### macOS

```bash
# Install Xcode Command Line Tools
xcode-select --install

# Run install script
./install.sh
```

#### Windows (MSYS2)

```bash
# Install MSYS2 from https://www.msys2.org/
# Open MSYS2 MinGW 64-bit terminal

# Install dependencies
pacman -S base-devel mingw-w64-x86_64-toolchain

# Run install script
./install.sh
```

#### Linux (Ubuntu/Debian)

```bash
# Install build essentials
sudo apt-get update
sudo apt-get install build-essential git curl

# Run install script
./install.sh
```

### 3. Build the Plugin

```bash
# Clean build
make clean
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS

# Install to VCV Rack
make install
```

### 4. Verify Build

```bash
# Check that plugin installed
ls ~/Documents/Rack2/plugins-*/swv-guitar-collection/

# Launch VCV Rack and verify modules appear
```

---

## How to Contribute

### Types of Contributions

#### 🐛 Bug Fixes

1. Find an issue labeled `bug` in [Issues](https://github.com/shortwavlabs/swv-guitar-collection/issues)
2. Comment that you're working on it
3. Create a feature branch
4. Fix the bug with tests
5. Submit a pull request

#### ✨ New Features

1. **Check if feature already requested** in Issues/Discussions
2. **Open a feature request** if not (see [Suggesting Features](#suggesting-features))
3. **Wait for approval** from maintainers
4. **Implement the feature** on a feature branch
5. **Add tests and documentation**
6. **Submit a pull request**

#### 📚 Documentation

Documentation improvements are always welcome:

- Fix typos or clarify existing docs
- Add examples or tutorials
- Improve API documentation
- Translate documentation

#### 🎸 NAM Models / Cabinet IRs

Contributing models or IRs:

1. **Ensure you have rights** to share the capture
2. **Include metadata**: Amp/cabinet name, settings, capture method
3. **Provide sample audio** demonstrating the tone
4. **Add to appropriate directory** (`res/models/` or `res/irs/`)
5. **Update documentation** with model information

---

## Coding Standards

### C++ Style Guide

We follow a modern C++ style based on VCV Rack conventions.

#### General Principles

- **Use C++17 features** where appropriate
- **RAII** for resource management
- **const correctness** throughout
- **Smart pointers** over raw pointers
- **Namespaces** to avoid pollution

#### Naming Conventions

```cpp
// Classes: PascalCase
class NamPlayer : public Module { };

// Functions/Methods: camelCase
void loadModel(const std::string& path);

// Variables: camelCase
int bufferSize = 128;

// Constants: UPPER_SNAKE_CASE
static constexpr int BLOCK_SIZE = 128;

// Member variables: camelCase (no prefix)
std::unique_ptr<NamDSP> namDsp;

// Private members: Consider trailing underscore
private:
    int privateValue_;

// Enums: PascalCase enum, UPPER_CASE values
enum ParamId {
    INPUT_PARAM,
    OUTPUT_PARAM,
    PARAMS_LEN
};
```

#### Code Formatting

```cpp
// Use 4 spaces for indentation (no tabs)
void process(const ProcessArgs& args) {
    if (condition) {
        // 4 space indent
        doSomething();
    }
}

// Braces on same line for functions and control flow
void myFunction() {
    while (running) {
        // ...
    }
}

// Space after keywords, no space for function calls
if (x > 0) {
    doSomething(x);
}

// One statement per line
int a = 1;
int b = 2;

// Not: int a = 1; int b = 2;
```

#### Header Guards

```cpp
#pragma once  // Preferred over traditional include guards
```

#### Comments

```cpp
// Use // for single-line comments

/*
 * Use block comments for multi-line documentation
 * describing complex logic or algorithms
 */

/**
 * Use Doxygen-style comments for API documentation
 * @param input Input buffer
 * @param output Output buffer
 */
void process(float* input, float* output);
```

#### Memory Management

```cpp
// Prefer smart pointers
std::unique_ptr<NamDSP> dsp = std::make_unique<NamDSP>();

// RAII for resources
{
    std::lock_guard<std::mutex> lock(mutex);
    // Automatically unlocked when leaving scope
}

// Avoid manual new/delete
// Bad:
MyClass* obj = new MyClass();
delete obj;

// Good:
auto obj = std::make_unique<MyClass>();
```

#### Thread Safety

```cpp
// Use std::atomic for lock-free primitives
std::atomic<bool> isLoading{false};

// Document thread affinity
void process(const ProcessArgs& args) {
    // Called on audio thread only
}

void loadModel(const std::string& path) {
    // Can be called from any thread
    // Spawns background thread for I/O
}
```

### File Organization

```
src/
├── ModuleName.cpp          # Module implementation
├── ModuleName.hpp          # Module interface
└── dsp/
    ├── DspClassName.cpp    # DSP implementation
    └── DspClassName.h      # DSP interface

res/
├── ModuleName.svg          # Module panel
└── ComponentName.svg       # UI components
```

---

## Testing

### Building Tests

```bash
make test
```

### Running Tests

```bash
./test/run_tests
```

### Writing Tests

Add tests for new features:

```cpp
#include "test_framework.h"
#include "../src/NamPlayer.hpp"

TEST(NamPlayerTest, LoadModel) {
    NamPlayer module;
    module.loadModel("test_models/test_model.nam");
    
    // Wait for async load
    while (module.isLoading) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(module.namDsp != nullptr);
    EXPECT_EQ(module.getModelName(), "test_model");
}

TEST(NamPlayerTest, ProcessAudio) {
    NamPlayer module;
    
    ProcessArgs args;
    args.sampleRate = 48000.0;
    
    // Test that processing doesn't crash without model
    EXPECT_NO_THROW(module.process(args));
}
```

### Test Coverage

Aim for:
- **Unit tests** for DSP algorithms
- **Integration tests** for module behavior
- **Edge case testing** (null inputs, extreme values)
- **Thread safety validation**

---

## Pull Request Process

### Before Submitting

1. **Create a feature branch** from `main`
   ```bash
   git checkout -b feature/my-new-feature
   ```

2. **Make your changes** following coding standards

3. **Test thoroughly**
   - Build on your platform
   - Test with VCV Rack
   - Run automated tests
   - Check for memory leaks (Valgrind/Instruments)

4. **Update documentation**
   - Update relevant .md files
   - Add API documentation for new functions
   - Update CHANGELOG.md

5. **Commit with clear messages**
   ```bash
   git commit -m "Add feature: brief description
   
   Longer description of what changed and why.
   Fixes #123"
   ```

### Submitting the PR

1. **Push to your fork**
   ```bash
   git push origin feature/my-new-feature
   ```

2. **Open Pull Request** on GitHub
   - Use descriptive title
   - Reference related issues
   - Describe changes thoroughly
   - Add screenshots/videos if UI changes

3. **PR Template**

   ```markdown
   ## Description
   Brief description of changes
   
   ## Type of Change
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Performance improvement
   - [ ] Documentation update
   
   ## Testing
   - Tested on: [OS and VCV Rack version]
   - Test cases: [describe manual testing]
   - Automated tests: [pass/fail]
   
   ## Checklist
   - [ ] Code follows style guidelines
   - [ ] Self-review completed
   - [ ] Documentation updated
   - [ ] Tests added/updated
   - [ ] CHANGELOG.md updated
   
   ## Related Issues
   Fixes #123
   Related to #456
   ```

### Review Process

1. **Automated checks** must pass (CI/CD)
2. **Code review** by maintainer(s)
3. **Address feedback** by pushing new commits
4. **Approval** and merge by maintainer

### After Merge

- Your contribution will be included in next release
- You'll be credited in CHANGELOG and release notes
- Thank you! 🎉

---

## Reporting Bugs

### Before Reporting

1. **Check existing issues** to avoid duplicates
2. **Update to latest version** - bug may be fixed
3. **Reproduce the bug** consistently
4. **Gather information** (see template below)

### Bug Report Template

Open an issue with this information:

```markdown
## Bug Description
Clear description of what's wrong

## Steps to Reproduce
1. Open VCV Rack
2. Add NAM Player module
3. Load model "xyz.nam"
4. ...

## Expected Behavior
What should happen

## Actual Behavior
What actually happens

## Environment
- **OS**: macOS 14.2 / Windows 11 / Ubuntu 22.04
- **VCV Rack Version**: 2.6.0
- **Plugin Version**: 2.0.0
- **CPU**: Intel i7 / M1 / AMD Ryzen
- **Audio Interface**: [if relevant]

## Additional Context
- Screenshots/videos
- Console output (VCV log)
- Patch file (if applicable)
- Crash reports

## Possible Solution
[Optional] Your ideas on how to fix
```

### Priority Labels

Maintainers will assign:
- `critical` - Crashes, data loss
- `high` - Major features broken
- `medium` - Minor features broken
- `low` - Cosmetic issues

---

## Suggesting Features

### Feature Request Template

```markdown
## Feature Description
What feature do you want?

## Use Case
Why is this useful? What problem does it solve?

## Proposed Solution
How could this be implemented?

## Alternatives Considered
What other approaches could work?

## Additional Context
- Screenshots/mockups
- Similar features in other plugins
- Related discussions

## Willing to Contribute?
- [ ] I can implement this myself
- [ ] I can help with testing
- [ ] I can provide feedback
```

### Feature Discussion

1. Community discussion in GitHub Discussions
2. Maintainer evaluation
3. If approved, issue created and labeled `enhancement`
4. Implementation by contributor or maintainer

---

## Documentation

### Documentation Structure

```
manual/
├── README.md              # Documentation index
├── quickstart.md          # Getting started guide
├── advanced-usage.md      # Advanced techniques
├── api-reference.md       # Technical API docs
├── faq.md                 # Common questions
└── examples/              # Example patches and code
```

### Documentation Style

- **Clear and concise** - Avoid jargon
- **Examples** - Show, don't just tell
- **Screenshots** - Visual aids for UI instructions
- **Code blocks** - Use syntax highlighting
- **Cross-references** - Link related sections

### Building Documentation Locally

Documentation is Markdown-based and can be viewed directly on GitHub or locally:

```bash
# Preview in any Markdown viewer
open manual/README.md
```

---

## Community

### Communication Channels

- **GitHub Issues**: Bug reports and feature requests
- **GitHub Discussions**: General questions and ideas
- **VCV Rack Forum**: Share patches and techniques
- **Email**: [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com)

### Getting Help

- Read the [documentation](manual/README.md)
- Check [FAQ](manual/faq.md)
- Search existing issues
- Ask in GitHub Discussions
- Join VCV Rack community forums

### Recognition

Contributors will be:
- Listed in CHANGELOG for their contributions
- Credited in release notes
- Added to project README (for significant contributions)
- Given our sincere gratitude! 🙏

---

## Development Tips

### Debugging

#### Enable VCV Rack Dev Mode

```bash
# Add to VCV Rack command line
/path/to/Rack -d  # Development mode (extra logging)
```

#### Logging

```cpp
// Use VCV Rack's logging system
DEBUG("Loading model: %s", path.c_str());
INFO("Model loaded successfully");
WARN("Sample rate mismatch");
```

#### Debugging Tools

**macOS:**
```bash
# Xcode Instruments for profiling
instruments -t "Time Profiler" /Applications/VCV\ Rack\ 2.app
```

**Linux:**
```bash
# Valgrind for memory issues
valgrind --leak-check=full Rack
```

**Windows:**
```
Use Visual Studio debugger or WinDbg
```

### Performance Profiling

```cpp
// Simple timing
auto start = std::chrono::high_resolution_clock::now();
// ... code to profile ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
DEBUG("Operation took %ld µs", duration.count());
```

### Code Review Checklist

Before submitting PR, review:

- [ ] Code compiles without warnings
- [ ] No memory leaks (verified with tools)
- [ ] Thread-safe operations
- [ ] Error handling for edge cases
- [ ] Comments for complex logic
- [ ] Documentation updated
- [ ] Tests added/passing
- [ ] No commented-out code
- [ ] No debug print statements
- [ ] Consistent style with existing code

---

## License

By contributing, you agree that your contributions will be licensed under the GPL-3.0-or-later license, matching the project's license.

### Third-Party Code

If including third-party code:
1. Ensure license compatibility (GPL-compatible)
2. Add LICENSE file to relevant directory
3. Credit authors in comments
4. Update main LICENSE.md with attribution

---

## Questions?

Don't hesitate to ask! We're here to help:

- **Email**: [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com)
- **Discussions**: [GitHub Discussions](https://github.com/shortwavlabs/swv-guitar-collection/discussions)

---

**Thank you for contributing to Guitar Tools! 🎸**

Your efforts help make this plugin better for the entire VCV Rack community.
