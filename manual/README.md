# Guitar Tools Documentation

Complete documentation for the Guitar Tools VCV Rack plugin.

## 📚 Documentation Index

Welcome to the comprehensive documentation for **Guitar Tools** by Shortwav Labs. Whether you're a beginner just getting started or an advanced user looking to master every feature, you'll find everything you need here.

---

## Quick Navigation

### 🚀 [Quickstart Guide](quickstart.md)
**Start here if you're new!**

Get up and running in 5 minutes. Learn the basics of NAM Player and Cabinet Simulator, dial in your first tone, and understand the fundamental concepts.

**Perfect for:**
- First-time users
- Quick reference
- Basic patching

**Estimated time:** 15-20 minutes

---

### 🎓 [Advanced Usage](advanced-usage.md)
**Level up your skills**

Deep dive into performance optimization, custom model creation, advanced patching techniques, and professional workflows.

**Perfect for:**
- Experienced users
- Studio production
- Performance optimization
- Custom model creation

**Estimated time:** 1-2 hours

---

### 🔧 [API Reference](api-reference.md)
**Technical documentation**

Complete technical reference for developers. Covers all classes, methods, parameters, and usage examples for extending or integrating with Guitar Tools.

**Perfect for:**
- Developers
- Custom modifications
- Integration projects
- Technical deep dives

**Reference document** - Browse as needed

---

### ❓ [FAQ](faq.md)
**Common questions answered**

Frequently asked questions organized by topic. Find quick answers to common problems and learn troubleshooting techniques.

**Perfect for:**
- Troubleshooting
- Quick answers
- Common issues
- Best practices

**Reference document** - Search for your question

---

### 💡 [Examples](examples/)
**Real-world patches and tutorials**

Step-by-step examples demonstrating various use cases, from basic guitar rigs to advanced professional setups.

**Available examples:**
- [Basic Guitar Rig](examples/basic-guitar-rig.md) - Perfect starting point
- High-Gain Metal Setup
- Dual Amp Configuration
- Re-amping Workflow
- Creative Applications
- And more...

**Perfect for:**
- Learning by example
- Patch ideas
- Specific use cases
- Inspiration

**Time:** 10-30 minutes per example

---

## 📖 Additional Resources

### [CHANGELOG](../CHANGELOG.md)
Version history, release notes, and migration guides. Stay up to date with new features and improvements.

### [CONTRIBUTING](../CONTRIBUTING.md)
Guidelines for contributing to Guitar Tools. Learn how to report bugs, suggest features, or contribute code, models, and documentation.

### [Main README](../README.md)
Plugin overview, installation instructions, and quick reference for both modules.

---

## 🎯 Learning Paths

### Path 1: Beginner
**Goal:** Get started and create your first guitar tones

1. Read [Main README](../README.md) (10 min)
2. Follow [Quickstart Guide](quickstart.md) (20 min)
3. Try [Basic Guitar Rig Example](examples/basic-guitar-rig.md) (15 min)
4. Experiment with different models and IRs (∞)

**Total time:** ~1 hour to proficiency

---

### Path 2: Intermediate User
**Goal:** Master all features and optimize your workflow

1. Complete Beginner path
2. Read [Advanced Usage](advanced-usage.md) (1-2 hours)
3. Review [FAQ](faq.md) for tips and tricks (30 min)
4. Try advanced examples (1 hour)

**Total time:** ~4-5 hours to mastery

---

### Path 3: Developer / Power User
**Goal:** Extend, modify, or deeply integrate Guitar Tools

1. Complete Intermediate path
2. Study [API Reference](api-reference.md) (2-3 hours)
3. Read [CONTRIBUTING](../CONTRIBUTING.md) (30 min)
4. Explore source code (variable)
5. Create custom models or contribute (variable)

**Total time:** Variable, ongoing learning

---

## 📋 Documentation by Topic

### Installation & Setup
- [Installation](../README.md#installation) - How to install the plugin
- [System Requirements](../README.md#system-requirements) - Minimum requirements
- [Building from Source](../README.md#building-from-source) - Compile it yourself

### Basic Usage
- [Your First Patch](quickstart.md#basic-setup) - Create a simple guitar rig
- [Loading Models](quickstart.md#loading-models) - Browse and load NAM models
- [Using Cabinet Sim](quickstart.md#cabinet-simulator-module) - Add speaker simulation
- [Basic Guitar Rig Example](examples/basic-guitar-rig.md) - Complete walkthrough

### NAM Player
- [Module Overview](../README.md#nam-player-module) - Features and controls
- [Loading Models](quickstart.md#loading-models) - How to load .nam files
- [Input/Output Gain](quickstart.md#dialing-in-your-tone) - Set proper levels
- [Noise Gate](quickstart.md#using-the-noise-gate) - Reduce unwanted noise
- [Eco Mode](quickstart.md#eco-mode-cpu-savings) - Lower CPU with one toggle
- [5-Band EQ](quickstart.md#tone-shaping-with-eq) - Shape your tone
- [API: NamPlayer Class](api-reference.md#namplayer-module) - Technical reference

### Cabinet Simulator
- [Module Overview](../README.md#cabinet-simulator-module) - Features and controls
- [Loading IRs](quickstart.md#loading-impulse-responses) - Load impulse responses
- [Blending IRs](quickstart.md#blending-irs) - Mix two cabinets
- [Tone Shaping](quickstart.md#tone-shaping) - Filters and adjustments
- [API: CabSim Class](api-reference.md#cabsim-module) - Technical reference

### Performance & Optimization
- [CPU Optimization](advanced-usage.md#performance-optimization) - Reduce CPU usage
- [Eco Mode](advanced-usage.md#eco-mode-offon) - Fast performance toggle for NAM Player
- [Memory Management](advanced-usage.md#memory-management) - Efficient resource use
- [Sample Rate Considerations](advanced-usage.md#sample-rate-considerations) - Quality vs. performance
- [Model Selection](advanced-usage.md#choosing-the-right-model) - Pick the right model

### Advanced Techniques
- [Custom Model Creation](advanced-usage.md#custom-model-creation) - Capture your own gear
- [Advanced Patching](advanced-usage.md#advanced-patching-techniques) - Complex signal routing
- [Polyphonic Processing](advanced-usage.md#polyphonic-processing) - Multiple voices
- [DAW Integration](advanced-usage.md#integration-with-daws) - Use with your DAW
- [Professional Mixing](advanced-usage.md#professional-mixing-tips) - Studio techniques

### Troubleshooting
- [Common Issues](faq.md#troubleshooting) - Quick fixes
- [No Sound Output](faq.md#no-sound-is-coming-out) - Audio troubleshooting
- [Performance Issues](faq.md#vcv-rack-is-using-too-much-cpu-how-can-i-optimize) - CPU problems
- [Model Loading Issues](faq.md#model-wont-load--green-light-doesnt-turn-on) - Loading errors
- [Audio Quality](faq.md#audio-quality) - Sound quality issues

### Development
- [API Reference](api-reference.md) - Complete technical documentation
- [Contributing Guide](../CONTRIBUTING.md) - How to contribute
- [Coding Standards](../CONTRIBUTING.md#coding-standards) - Style guide
- [Testing](../CONTRIBUTING.md#testing) - Testing practices
- [Pull Request Process](../CONTRIBUTING.md#pull-request-process) - Submitting changes

---

## 🗺️ Documentation Structure

```
swv-guitar-collection/
├── README.md                    # Main plugin overview
├── CHANGELOG.md                 # Version history
├── CONTRIBUTING.md              # Contribution guidelines
├── LICENSE.md                   # License information
└── manual/                      # Documentation directory (you are here)
    ├── README.md                # This index file
    ├── quickstart.md            # Getting started guide
    ├── advanced-usage.md        # Advanced features and techniques
    ├── api-reference.md         # Technical API documentation
    ├── faq.md                   # Frequently asked questions
    └── examples/                # Example patches and tutorials
        ├── README.md            # Examples index
        ├── basic-guitar-rig.md  # Basic setup example
        └── [more examples...]   # Additional examples
```

---

## 🔍 Search Tips

### Finding Information Quickly

**For specific topics:**
1. Use your browser's search (Cmd/Ctrl + F)
2. Check the relevant section above
3. Browse the FAQ for common questions

**For code examples:**
1. Check the [Examples](examples/) directory
2. Look in [API Reference](api-reference.md#examples)
3. Search [Advanced Usage](advanced-usage.md) for techniques

**For troubleshooting:**
1. Start with [FAQ](faq.md#troubleshooting)
2. Check [Common Issues](faq.md#troubleshooting)
3. Search [GitHub Issues](https://github.com/shortwavlabs/swv-guitar-collection/issues)

---

## 🆘 Getting Help

### Self-Service Resources

1. **Search this documentation** - Most questions are answered here
2. **Check the FAQ** - Common questions and solutions
3. **Try examples** - Learn by doing
4. **Read error messages** - Often contain useful info

### Community Support

- **GitHub Discussions**: [Ask questions](https://github.com/shortwavlabs/swv-guitar-collection/discussions)
- **VCV Rack Forum**: [Community help](https://community.vcvrack.com/)
- **GitHub Issues**: [Report bugs](https://github.com/shortwavlabs/swv-guitar-collection/issues)

### Direct Support

- **Email**: [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com)
- **Support the project**: [Ko-fi](https://ko-fi.com/shortwavlabs)

---

## 📝 Documentation Updates

This documentation is continuously updated. Check the [CHANGELOG](../CHANGELOG.md) for documentation improvements in each release.

**Last major update:** Version 2.0.0 (January 2026)

### Contributing to Documentation

Found an error or want to improve the docs?

1. See [CONTRIBUTING.md](../CONTRIBUTING.md#documentation)
2. Submit a pull request with your changes
3. Help others learn!

---

## 🌟 Quick Links

### External Resources

**NAM Models:**
- [ToneHunt](https://tonehunt.org/) - Community model library
- [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler) - Official NAM project

**Cabinet IRs:**
- [Kalthallen Cabs](https://www.kalthallen-cabs.com/) - Free IR library
- [God's Cab](https://wilkinson-audio.com/) - High-quality free IRs

**VCV Rack:**
- [VCV Rack Manual](https://vcvrack.com/manual/) - Official VCV documentation
- [VCV Community](https://community.vcvrack.com/) - User forum

### Project Links

- **GitHub Repository**: [github.com/shortwavlabs/swv-guitar-collection](https://github.com/shortwavlabs/swv-guitar-collection)
- **Issue Tracker**: [GitHub Issues](https://github.com/shortwavlabs/swv-guitar-collection/issues)
- **Releases**: [GitHub Releases](https://github.com/shortwavlabs/swv-guitar-collection/releases)
- **Support Development**: [Ko-fi](https://ko-fi.com/shortwavlabs)

---

## 💬 Feedback

We're constantly improving the documentation. Your feedback helps!

**How to provide feedback:**
- Open a [GitHub Issue](https://github.com/shortwavlabs/swv-guitar-collection/issues) for corrections
- Start a [Discussion](https://github.com/shortwavlabs/swv-guitar-collection/discussions) for suggestions
- Email [contact@shortwavlabs.com](mailto:contact@shortwavlabs.com) for private feedback

**What we're looking for:**
- Unclear explanations
- Missing information
- Broken links
- Suggestions for new topics
- Examples you'd like to see

---

## 📜 License

Documentation is licensed under [GPL-3.0-or-later](../LICENSE.md), same as the plugin.

You're free to:
- Share and adapt the documentation
- Use it in your own projects
- Translate it to other languages

Just maintain the license and give credit!

---

**Happy patching! 🎸**

*Explore, experiment, and create amazing guitar tones with Guitar Tools.*

---

**Navigation:** [↑ Back to Top](#guitar-tools-documentation) | [Main README](../README.md) | [Quickstart →](quickstart.md)
