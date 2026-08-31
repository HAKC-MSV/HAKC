# HAKC Project Roadmap

**Last Updated**: July 23, 2026  
**Current Version**: Beta (LLVM-based)  
**Target Release**: v1.0 (Transpiler-based) - January 2027 (6 months)

---

## Executive Summary

### Vision

Enable kernel-level compartmentalization without requiring custom compiler builds, making HAKC accessible to enterprise Linux distributions and suitable for upstream kernel inclusion.

### Strategic Goal

Achieve adoption by enterprise Linux distributions (RedHat, SUSE, Canonical) by transitioning from a custom LLVM compiler pass to a **compiler-agnostic C-to-C transpiler architecture**. This architectural shift eliminates the primary barrier to upstream adoption while maintaining the security guarantees and performance characteristics of the current system.

### Current Phase

**Beta → Transpiler Architecture Transition → v1.0 Production Release**

We are pivoting from an LLVM IR-based transformation pass to a source-to-source transpiler that:
- Uses **Clang AST** for parsing and type information extraction
- Generates **modified C source code** that can be compiled with any standard compiler (GCC, Clang, ICC)
- Preserves the existing analysis infrastructure (Kuzu database, NetworkX policy generation)
- Integrates seamlessly with the kernel build system (Kbuild)

### Timeline

**6-month initial release target** with 4 major phases:
1. **Foundation** (Months 1-2): Architecture design + prototype
2. **Core Implementation** (Months 3-4): Feature-complete transpiler
3. **Validation & Hardening** (Month 5): Performance + compatibility testing
4. **Release Preparation** (Month 6): Documentation + upstream engagement

---

## Current Status

**Version**: Beta (LLVM-based)  
**As of**: July 2026

### What Works Today

- ✅ **Full x86_64 compartmentalization** with analysis + enforcement pipeline
- ✅ **ARM64 support** with hardware features (MTE memory tagging + PAC pointer authentication)
- ✅ **ROS2 demonstration** running in QEMU with compartment isolation
- ✅ **Graph-based policy analysis** using Kuzu database + NetworkX algorithms
- ✅ **Runtime kernel support**:
  - Pointer signing and verification
  - Memory tagging (16-color scheme)
  - Transfer functions for cross-compartment calls
  - Per-CPU variable handling
- ✅ **Multiple compartmentalization algorithms** (Greedy, Filesystem-based, Size-balanced)
- ✅ **CVE-based vulnerability analysis** for demonstrating security benefits
- ✅ **Comprehensive test suite** (LLVM pass tests + kernel runtime tests)

### Known Limitations

- ⚠️ **Requires custom LLVM build** - blocks distribution adoption
- ⚠️ **Version-locked to specific LLVM release** - difficult to track upstream
- ⚠️ **IR-level transformations** - complicates debugging and code inspection
- ⚠️ **Long build times** - rebuilding LLVM for development iteration is slow
- ⚠️ **Limited to Clang/LLVM toolchain** - cannot use GCC or vendor compilers

These limitations prevent upstream kernel adoption and distribution acceptance, motivating the transpiler transition.

---

## Architecture Transition: LLVM → Transpiler

### Problem Statement

Enterprise Linux distributions require using their **validated compiler toolchains** and will not adopt solutions requiring custom compiler builds. Key concerns:

1. **Maintenance burden**: Tracking LLVM upstream changes and maintaining patches
2. **Security validation**: Distros must re-certify any modified compiler
3. **Toolchain flexibility**: Need to support GCC (primary kernel compiler) and vendor toolchains
4. **Debugging complexity**: IR-level changes obscure source code debugging
5. **Build infrastructure**: Existing kernel build systems expect standard compilers

### Solution: C-to-C Transpiler Architecture

**Three-stage build process**:

```
┌─────────────────────────────────────────────────────────────────┐
│ Stage 1: Analysis                                               │
│ ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐ │
│ │ Clang AST   │───▶│ Type/Symbol  │───▶│ Kuzu Graph Database │ │
│ │ Parser      │    │ Extractor    │    │ (DAG + Edges)       │ │
│ └─────────────┘    └──────────────┘    └─────────────────────┘ │
│                                                     │            │
│                                         ┌───────────▼─────────┐ │
│                                         │ Policy Generator    │ │
│                                         │ (NetworkX Analysis) │ │
│                                         └───────────┬─────────┘ │
│                                                     │            │
│                                         ┌───────────▼─────────┐ │
│                                         │ Compartment Policy  │ │
│                                         └─────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ Stage 2: Transpilation                                          │
│ ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐ │
│ │ Original    │───▶│ C-to-C       │───▶│ Transformed         │ │
│ │ Kernel      │    │ Transpiler   │    │ Kernel Source       │ │
│ │ Source      │    │ (AST-based)  │    │ (with HAKC)         │ │
│ └─────────────┘    └──────────────┘    └─────────────────────┘ │
│                            ▲                                    │
│                            │                                    │
│                    ┌───────┴────────┐                          │
│                    │ Policy Server  │                          │
│                    │ (queries DB)   │                          │
│                    └────────────────┘                          │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ Stage 3: Compilation                                            │
│ ┌─────────────┐    ┌──────────────┐    ┌─────────────────────┐ │
│ │ Transformed │───▶│ Standard     │───▶│ HAKC-Protected      │ │
│ │ Source      │    │ Compiler     │    │ Kernel Binary       │ │
│ │             │    │ (GCC/Clang)  │    │                     │ │
│ └─────────────┘    └──────────────┘    └─────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

### Key Technical Decisions

| Component | Current (LLVM) | New (Transpiler) | Rationale |
|-----------|---------------|------------------|-----------|
| **Input Format** | C source → LLVM IR | C source → Clang AST | AST preserves source structure |
| **Type Information** | DWARF debug info | Clang AST traversal | Direct access to type system |
| **Transformation** | IR instruction manipulation | Source code generation | Any compiler can build output |
| **Output Format** | Modified IR → Object files | Modified C source | Human-readable, debuggable |
| **Compiler Support** | Clang only | GCC, Clang, ICC, etc. | Distro flexibility |
| **Build Integration** | LLVM pass via `-fpass-plugin` | Kbuild preprocessing step | Standard kernel workflow |

### Key Benefits

- ✅ **Compiler independence**: Works with any C compiler (GCC, Clang, vendor toolchains)
- ✅ **Faster development**: No LLVM rebuild required for iteration
- ✅ **LLVM version independence**: Only need Clang libtooling for analysis, not full build
- ✅ **Better debuggability**: Source-level transformations preserve line numbers and structure
- ✅ **Upstream acceptance**: Acceptable for mainline kernel (similar to pahole, objtool)
- ✅ **Transparent operation**: Generated code is human-readable and inspectable

### Performance Target

**< 5% runtime overhead** compared to baseline kernel (same as current LLVM approach)

---

## Roadmap Timeline

### Phase 1: Foundation (Months 1-2)

**Target**: [Aug 2026 - Sep 2026]

#### P0 - Critical Path

- [ ] **Design C-to-C transpiler architecture**
  - Document AST traversal strategy for type extraction
  - Define source rewriting approach (Clang `Rewriter` vs custom codegen)
  - Specify Kbuild integration points
  - Design file-by-file vs whole-program transformation strategy
  - Plan for handling kernel-specific constructs (inline asm, attributes, macros)
  
- [ ] **Implement Clang AST-based type extractor**
  - Replace DWARF-based `HAKCTypeIdentifier` with AST traversal using `libtooling`
  - Extract struct layouts, sizes, and member offsets
  - Extract function signatures, parameter types, and return types
  - Extract pointer types and indirection levels
  - Store in existing Kuzu database schema (minimal schema changes)
  - Validate against DWARF-extracted types for correctness
  
- [ ] **Prototype minimal transpiler for single-file transformation**
  - Parse single C source file → Build AST → Transform → Output modified C
  - Focus on **transfer function generation only** (simplest transformation)
  - Target a simple kernel file (e.g., `kernel/printk.c`) as test case
  - Validate output compiles cleanly with both GCC and Clang
  - Ensure output produces functionally equivalent object code
  - Location: `llvm-project/llvm/utils/hakc/hakc-transpiler/`

#### P1 - High Priority

- [ ] **Performance baseline establishment**
  - Measure current LLVM approach overhead on:
    - Kernel boot time
    - System call latency (getpid, read, write)
    - Network throughput (TCP, UDP)
    - File I/O throughput
    - Full LMBench/UnixBench suite
  - Document baseline: HAKC vs vanilla kernel
  - Set acceptance criteria: transpiler must stay within 5% of LLVM performance
  
- [ ] **Compiler compatibility matrix**
  - Document target compilers:
    - GCC 11.x, 12.x, 13.x (primary distro versions)
    - Clang 15.x, 16.x, 17.x
    - ICC (if required by customers)
  - Identify kernel subsystems with compiler-specific code:
    - GCC plugins
    - Clang-specific attributes
    - Different optimization behaviors
  - Plan for testing across all targets
  
- [ ] **Migration strategy document**
  - Define incremental vs big-bang transition plan
  - Backward compatibility considerations (preserve LLVM pass during transition?)
  - Testing strategy for feature parity (same compartmentalization results)
  - Rollout plan for existing users
  - Timeline for deprecating LLVM pass

#### P2 - Medium Priority

- [ ] **Development environment setup**
  - Docker containers for multi-compiler testing
  - CI/CD pipeline scaffolding
  - Automated performance regression testing framework

#### Deliverables

- ✅ Architecture design document (`docs/transpiler-architecture.md`)
- ✅ Working prototype: single-file transpilation with transfer functions
- ✅ Performance baseline report
- ✅ Migration strategy document

---

### Phase 2: Core Implementation (Months 3-4)

**Target**: [Oct 2026 - Nov 2026]

#### P0 - Critical Path

- [ ] **Full transpiler implementation**
  - **Transfer function generation**:
    - Generate wrapper functions for cross-compartment calls
    - Insert `hakc_transfer_to_clique()` calls with proper parameters
    - Handle return value transfers back to caller compartment
    - Support for complex types (structs, unions, arrays)
  - **Pointer signing/checking instrumentation**:
    - Insert `hakc_sign_pointer()` at pointer creation sites
    - Insert `check_hakc_data_access()` at pointer dereferences
    - Insert `check_hakc_code_access()` at indirect function calls
    - Optimize away redundant checks (static analysis)
  - **Memory coloring for data structures**:
    - Tag `kmalloc()` allocations with compartment color
    - Color global variables based on policy
    - Handle per-CPU variables with `hakc_transfer_percpu()`
  - **Compartment metadata generation**:
    - Generate ELF section attributes for compartment boundaries
    - Emit `.hakc_metadata` section with entry/exit points
    - Generate symbol maps for runtime loader
  - **Handle complex kernel patterns**:
    - Function pointers and callbacks
    - Variadic functions
    - Inline assembly (pass through unchanged)
    - Preprocessor conditionals
    - Kernel-specific macros (`EXPORT_SYMBOL`, `__init`, etc.)
  
- [ ] **Integration with analysis pipeline**
  - Connect AST-based type extraction to existing policy server
  - Preserve graph database workflow (minimal changes to Python code)
  - Transpiler queries policy server via socket for compartment assignments
  - Reuse existing `HAKCCompartmentalizationPolicy` data structures
  - Maintain compatibility with existing analysis algorithms
  - Support incremental transpilation (file-by-file)
  
- [ ] **Kbuild integration**
  - Create Kconfig option: `CONFIG_HAKC_COMPARTMENTALIZATION`
  - Add transpilation step before compilation:
    - Hook into Kbuild before `$(CC)` invocation
    - Transpile `.c` → `.hakc.c` (transformed source)
    - Compile `.hakc.c` → `.o` with standard compiler
  - Handle parallel builds correctly (per-file locking if needed)
  - Preserve compilation database generation for analysis phase
  - Update `scripts/` to invoke transpiler
  - Support `make clean`, `make mrproper` for generated files
  - Location: `linux/scripts/hakc/` for build integration scripts
  
- [ ] **Initial validation suite**
  - Port existing LLVM pass tests to transpiler:
    - `llvm-project/llvm/test/Transforms/Compartmentalization/hakc/`
    - Convert from IR-based tests to source-based tests
  - Functional correctness tests:
    - Same input policy → same compartmentalization result
    - Transfer functions behave identically
    - Pointer authentication works correctly
  - Cross-compiler testing:
    - Build same kernel with GCC and Clang
    - Binary comparison (functional equivalence)
    - Boot both kernels and run identical test suite
  - Add new tests in `linux/tools/testing/selftests/hakc/`

#### P1 - High Priority

- [ ] **Optimization passes**
  - **Minimize code duplication**:
    - Share transfer functions across similar call sites
    - De-duplicate identical pointer checks
    - Inline small transfer functions
  - **Reduce metadata size**:
    - Compress compartment tables
    - Use efficient encoding for policy data
  - **Efficient pointer authentication**:
    - Batch signing operations where possible
    - Cache PAC contexts for hot paths
    - Lazy signing for cold paths
  
- [ ] **Debug info preservation**
  - Emit `#line` directives to map transformed code to original source
  - Ensure GDB shows original source files and line numbers
  - Support kernel debuggers (GDB, crash utility)
  - Preserve stack traces with original function names
  - Test with `CONFIG_DEBUG_INFO_BTF` enabled
  
- [ ] **Documentation**
  - **Developer guide**: `docs/transpiler-developer-guide.md`
    - Transpiler architecture and design decisions
    - Code organization and key modules
    - How to add new transformations
    - Debugging tips for transpiler development
  - **Build system integration guide**: `docs/transpiler-build-integration.md`
    - How transpiler integrates with Kbuild
    - Configuration options
    - Troubleshooting build failures
  - **Troubleshooting guide**: `docs/transpiler-troubleshooting.md`
    - Common errors and solutions
    - Performance debugging
    - Compatibility issues

#### P2 - Medium Priority

- [ ] **Tooling improvements**
  - Better error messages from transpiler (point to original source lines)
  - Visualization of transformations:
    - Before/after diff viewer
    - Transfer function call graph
    - Compartment boundary visualization
  - Intermediate file inspection utilities:
    - `hakc-inspect-transpiled` tool
    - Show what transformations were applied to a file
  
- [ ] **Code quality**
  - Static analysis with clang-tidy
  - Memory safety with AddressSanitizer/MemorySanitizer
  - Code review and refactoring

#### Deliverables

- ✅ Feature-complete C-to-C transpiler
- ✅ Integrated with kernel build system
- ✅ Passes basic validation suite (functional equivalence with LLVM pass)
- ✅ Developer documentation

---

### Phase 3: Validation & Hardening (Month 5)

**Target**: [Dec 2026]

#### P0 - Critical Path

- [ ] **Performance validation**
  - **Full kernel build performance**:
    - Measure transpilation overhead (time added to build)
    - Target: < 10% increase in total build time
    - Optimize hot paths in transpiler if needed
  - **Runtime performance**:
    - Run full benchmark suite (LMBench, UnixBench, kernel selftests)
    - Compare transpiler-based kernel vs LLVM-based kernel vs vanilla
    - Meet < 5% overhead target
    - Profile performance issues with `perf`
  - **Identify and fix regressions**:
    - Optimize transfer function overhead
    - Reduce pointer check frequency
    - Improve compartment switching latency
  
- [ ] **Compiler compatibility testing**
  - **GCC compatibility**:
    - Build full kernel with GCC 11.x, 12.x, 13.x
    - Test on x86_64 and ARM64 (aarch64-linux-gnu-gcc)
    - Fix GCC-specific warnings and errors
    - Validate runtime behavior matches Clang builds
  - **Clang compatibility**:
    - Build with Clang 15.x, 16.x, 17.x
    - Ensure no regressions from LLVM-based approach
  - **Compiler-specific issues**:
    - Handle different inline heuristics
    - Handle different optimization behaviors
    - Test with various optimization levels (-O0, -O2, -O3, -Os)
  - **Automated testing**:
    - CI pipeline tests all compilers on every commit
    - Matrix builds: {GCC, Clang} × {x86_64, ARM64} × {defconfig, allmodconfig}
  
- [ ] **Functional equivalence testing**
  - **Compare LLVM-based vs transpiler-based compartmentalization**:
    - Same policy input → same compartment assignments
    - Same transfer functions generated (semantically equivalent)
    - Same runtime behavior on test workloads
  - **ROS2 demo equivalence**:
    - Demo works identically with transpiler-based kernel
    - Same security boundaries enforced
    - Same performance characteristics
  - **Regression testing**:
    - All existing HAKC tests pass
    - No functional regressions introduced
  
- [ ] **Security validation**
  - **Pointer authentication correctness**:
    - Valid pointers pass checks
    - Invalid pointers are caught (test with deliberate errors)
    - No false positives or false negatives
  - **Compartment boundary enforcement**:
    - Cross-compartment calls go through transfer functions
    - Direct calls blocked or detected
    - Memory tagging prevents unauthorized access
  - **Penetration testing**:
    - Run existing exploit demos (`scripts/ros2-demo/hakc-demo-exploit.c`)
    - Attempt compartment boundary violations
    - Verify all attacks are mitigated
  - **CVE case studies**:
    - Demonstrate mitigation of specific historical kernel CVEs
    - Measure reduction in attack surface

#### P1 - High Priority

- [ ] **Edge case handling**
  - **Complex kernel patterns**:
    - Inline assembly (ensure pass-through works)
    - `__attribute__` usage (gcc/clang differences)
    - Preprocessor-heavy code (conditional compilation)
    - Variadic functions and macros
    - Self-modifying code patterns
  - **Architecture-specific code**:
    - x86_64-specific paths
    - ARM64-specific paths
    - Ensure transpiler handles arch-specific ifdefs correctly
  - **Stress testing**:
    - Large source files (> 10k lines)
    - Complex dependency chains
    - Full `allmodconfig` build
  
- [ ] **Regression test suite**
  - **Automated testing**:
    - CI/CD pipeline for all supported architectures
    - Nightly builds with full test suite
    - Performance regression detection
  - **Test coverage**:
    - Aim for > 80% code coverage in transpiler
    - Cover all transformation types
    - Cover error handling paths
  - **Integration with kernel CI**:
    - Compatible with kernel's `0day` testing infrastructure
    - Automated bisection for failures
  
- [ ] **Documentation updates**
  - Update `README.md` with transpiler build instructions
  - Migration guide from LLVM to transpiler: `docs/migration-llvm-to-transpiler.md`
  - FAQ for common issues: `docs/FAQ.md`
  - Update all references to LLVM pass in existing docs

#### P2 - Medium Priority

- [ ] **Code quality**
  - Static analysis (clang-tidy, cppcheck, coverity)
  - Code review and refactoring for maintainability
  - Memory leak detection (valgrind, ASan)
  - Resource leak detection (open files, sockets)
  - Documentation coverage (all public APIs documented)

#### Deliverables

- ✅ Performance meets targets (< 5% overhead, < 10% build time increase)
- ✅ Multi-compiler compatibility validated (GCC 11+, Clang 15+)
- ✅ Security properties verified with penetration testing
- ✅ Comprehensive regression test suite
- ✅ All documentation updated

---

### Phase 4: Release Preparation (Month 6)

**Target**: [Jan 2027]

#### P0 - Critical Path

- [ ] **Production readiness checklist**
  - ✅ All P0 and P1 items from previous phases completed
  - ✅ No known critical bugs (P0/P1 issues resolved)
  - ✅ Performance targets met (verified in CI)
  - ✅ Documentation complete and reviewed
  - ✅ Security validation passed
  - ✅ Multi-compiler testing passed
  - ✅ Legal review (licenses, attributions)
  
- [ ] **Packaging and distribution**
  - **Clean, installable transpiler toolchain**:
    - Standalone build of transpiler tools
    - Does not require full LLVM build
    - Only needs Clang headers/libraries for libtooling
  - **Integration scripts for kernel builds**:
    - `scripts/hakc/setup-hakc.sh` - one-time setup
    - `scripts/hakc/run-analysis.sh` - compartmentalization analysis
    - `scripts/hakc/build-hakc-kernel.sh` - full build wrapper
  - **Package for common distros**:
    - RPM spec file for Fedora/RHEL (`.spec` file)
    - DEB package for Debian/Ubuntu (`debian/` directory)
    - Build instructions for other distros
  - **Version management**:
    - Tag v1.0 release in git
    - Semantic versioning for future releases
  
- [ ] **Upstream engagement**
  - **Prepare RFC patches for LKML**:
    - Split into logical patch series (< 15 patches)
    - Follow kernel coding style (checkpatch.pl clean)
    - Write detailed commit messages
    - Include cover letter explaining HAKC benefits
    - Include performance numbers and security case studies
  - **Engage with distro engineers**:
    - RedHat security team
    - SUSE kernel team
    - Canonical kernel team
    - Present at distro developer conferences
  - **Address initial feedback**:
    - Respond to LKML reviews promptly
    - Iterate on patch series based on feedback
    - Prepare for multi-round review process
  
- [ ] **Release artifacts**
  - **Versioned release (v1.0)**:
    - Git tag: `v1.0`
    - GitHub release with binaries
    - Source tarball
  - **Change log**:
    - `CHANGELOG.md` with full history
    - Highlight LLVM → transpiler migration
    - Breaking changes (if any)
    - Upgrade instructions
  - **Release notes**:
    - `docs/release-notes-v1.0.md`
    - New features
    - Known limitations
    - Supported platforms
    - System requirements

#### P1 - High Priority

- [ ] **User documentation**
  - **Quick start guide**: `docs/quickstart.md`
    - Build and install HAKC in 5 minutes
    - Target audience: kernel developers
  - **Integration guide for distributions**: `docs/distro-integration.md`
    - How to integrate HAKC into distro kernel builds
    - Configuration recommendations
    - Testing procedures
  - **Integration guide for kernel maintainers**: `docs/kernel-maintainer-guide.md`
    - How to use HAKC for subsystem compartmentalization
    - Policy customization
    - Debugging compartmentalization issues
  - **Performance tuning guide**: `docs/performance-tuning.md`
    - Optimization knobs
    - Profiling techniques
    - Trade-offs between security and performance
  - **Security benefits whitepaper**: `docs/security-whitepaper.md`
    - Threat model
    - CVE case studies (specific vulnerabilities mitigated)
    - Attack surface reduction quantification
    - Comparison with other kernel hardening approaches
  
- [ ] **Community building**
  - **Announcement blog post**:
    - Technical blog post on project website
    - Cross-post to LWN.net, Phoronix
    - Hacker News, /r/linux, /r/kernel submissions
  - **Academic paper** (if applicable):
    - Submit to security conference (USENIX Security, Oakland, CCS)
    - Or systems conference (OSDI, SOSP, EuroSys)
  - **Conference presentations**:
    - Linux Security Summit (LSS)
    - Linux Plumbers Conference
    - FOSDEM
  - **Demo videos and tutorials**:
    - YouTube walkthrough of HAKC setup
    - Asciinema recordings of build process
    - Demo of exploit mitigation
  
- [ ] **Support infrastructure**
  - **Issue tracker cleanup**:
    - Close resolved issues
    - Triage open issues
    - Label issues by priority and component
  - **Support documentation**:
    - Template for bug reports
    - Template for feature requests
    - Contribution guidelines updated
  - **Developer onboarding guide**: `docs/DEVELOPER_ONBOARDING.md`
    - How to set up development environment
    - Architecture overview
    - Code walkthrough
    - How to contribute

#### P2 - Medium Priority

- [ ] **Future work planning**
  - Create GitHub milestones for v1.1, v1.2
  - Gather community feedback for prioritization
  - Plan next-generation features (see "Beyond v1.0" section)
  
- [ ] **Website and branding**
  - Project website with documentation
  - Logo and visual identity
  - Social media presence

#### Deliverables

- ✅ v1.0 production release tagged and published
- ✅ Upstream RFC submitted to LKML
- ✅ Complete user and developer documentation
- ✅ Distro packages (RPM, DEB)
- ✅ Community engagement initiated (blog posts, conferences)
- ✅ Support infrastructure operational

---

## Beyond v1.0: Future Work

### Research & Experimental (Post-v1.0)

**Advanced Compartmentalization Policies**
- [ ] **Machine learning-based policy generation**
  - Learn optimal compartment boundaries from runtime traces
  - Minimize performance overhead while maximizing security
  - Adapt to workload-specific patterns
- [ ] **CVE-aware compartmentalization**
  - Automatically analyze CVE patterns to inform compartment boundaries
  - Prioritize isolating historically vulnerable subsystems
  - Dynamic policy updates based on new CVE disclosures
- [ ] **Dynamic policy adjustment**
  - Runtime policy reconfiguration without reboot
  - Adaptive compartmentalization based on threat level
  - Hot-patching of compartment boundaries

**Extended Platform Support**
- [ ] **RISC-V architecture**
  - RISC-V pointer masking extension (Zpm)
  - RISC-V capability-based protection (CHERI-RISC-V)
- [ ] **Additional ARM variants**
  - ARMv8.5-A MTE refinements
  - ARMv9 enhancements
- [ ] **x86_64 hardware features**
  - Intel LAM (Linear Address Masking)
  - Intel CET (Control-flow Enforcement Technology)
  - AMD memory tagging extensions

**Performance Optimizations**
- [ ] **Hardware-accelerated pointer authentication**
  - Offload PAC operations to crypto accelerators
  - Batch authentication operations
- [ ] **Lazy compartment switching**
  - Delay expensive context switches until necessary
  - Speculative compartment entry/exit
- [ ] **Zero-copy transfer functions**
  - Eliminate memory copies for read-only transfers
  - Shared memory regions with access control

**Developer Experience**
- [ ] **IDE integration**
  - VSCode extension for HAKC visualization
  - CLion plugin for policy editing
  - Real-time compartmentalization feedback in editor
- [ ] **Visual policy editor**
  - GUI for designing compartment policies
  - Drag-and-drop compartment boundary adjustment
  - Visual impact analysis (performance/security trade-offs)
- [ ] **Static analysis warnings**
  - Warn about suboptimal compartmentalization
  - Suggest policy improvements
  - Detect potential security issues

**Upstream Integration**
- [ ] **Mainline kernel inclusion**
  - Merge HAKC runtime into mainline kernel
  - Upstream Kconfig option: `CONFIG_HAKC`
  - Long-term maintenance and stability
- [ ] **Distribution adoption tracking**
  - Monitor which distros adopt HAKC
  - Gather feedback from distro maintainers
  - Support distro-specific requirements
- [ ] **Production deployment case studies**
  - Document real-world deployments
  - Performance data from production systems
  - Security incident reports (prevented attacks)

### Long-term Vision (3-5 years)

- **Standard kernel hardening mechanism** accepted by upstream Linux kernel
- **Adopted by major distributions** (Fedora, RHEL, Ubuntu, SUSE) by default
- **Extended to userspace compartmentalization** for system services and applications
- **Industry standard** for privilege separation in operating systems
- **Academic adoption** as a platform for compartmentalization research

---

## Non-Goals (v1.0 Scope)

Explicitly **out of scope** for v1.0 to manage expectations and maintain focus:

- ❌ **Full GCC plugin** - We require Clang libtooling for AST parsing during analysis phase, but GCC (or any compiler) can build the transpiler output. This is acceptable for distros.
- ❌ **Userspace application compartmentalization** - v1.0 is kernel-only. Userspace compartmentalization is a future research direction.
- ❌ **Automatic CVE remediation** - Policy generation is manual or semi-automated. Fully automatic CVE-aware policies are research work.
- ❌ **Real-time kernel support** (`PREEMPT_RT`) - Focus on general-purpose kernels first. RT kernel support requires additional latency analysis.
- ❌ **Android kernel integration** - Mainline Linux only. Android has different build system and security model.
- ❌ **Windows/macOS support** - HAKC is Linux-specific, leveraging kernel-specific features.
- ❌ **Microkernel architectures** - Currently targeting monolithic Linux kernel. Microkernel compartmentalization is a different problem.
- ❌ **LLVM pass maintenance** - Once transpiler is production-ready, the LLVM pass will be deprecated and no longer maintained (though source will remain for reference).

---

## Success Metrics

### Technical Metrics (Quantifiable Goals)

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| **Runtime Performance** | < 5% overhead vs vanilla kernel | LMBench, UnixBench, kernel selftests |
| **Build Time** | < 10% increase vs vanilla kernel build | Time `make -j$(nproc)` on reference hardware |
| **Compiler Compatibility** | 100% pass rate with GCC 11-13, Clang 15-17 | CI matrix builds, all tests pass |
| **Test Coverage** | > 80% code coverage in transpiler | gcov/lcov on transpiler codebase |
| **Boot Success** | 100% boot rate on x86_64, ARM64 | Automated QEMU tests, daily runs |
| **Functional Equivalence** | 100% of LLVM pass tests pass with transpiler | Port existing test suite, no regressions |

### Adoption Metrics (Post-v1.0, Tracked Over Time)

| Metric | 6 Months Post-Release | 12 Months Post-Release |
|--------|----------------------|------------------------|
| **Upstream RFC Status** | Initial submission, review in progress | Accepted or in final revision rounds |
| **Distribution Pilot Programs** | 1+ distro pilot (target: RedHat) | 2+ distros in pilot or production |
| **Community Contributions** | 5+ external contributors | 15+ external contributors |
| **GitHub Stars** | 100+ stars | 500+ stars |
| **Conference Presentations** | 1+ accepted talk | 3+ talks or papers |
| **Production Deployments** | 1+ production deployment | 5+ production deployments |
| **CVE Mitigations** | Document 5+ historical CVEs mitigated | Document 10+ CVEs + real-world incident |

### Community Engagement Indicators

- **Issue tracker activity**: 10+ issues filed, 80%+ resolved within 2 weeks
- **Mailing list/discussion forum**: Active discussions, responsive maintainers
- **Documentation quality**: No major gaps reported, < 5% of issues are "how do I...?"
- **Academic citations**: 3+ academic papers cite HAKC within 1 year

---

## Dependencies & Risks

### Technical Dependencies

| Dependency | Status | Risk Level | Notes |
|------------|--------|-----------|-------|
| **Clang libtooling** (AST parsing) | ✅ Stable, well-supported | Low | Mature API, widely used in Clang-Tidy, Clang-Format |
| **Kuzu graph database** | ✅ Current analysis infrastructure | Low | Already in use, stable API |
| **Kernel build system (Kbuild)** | ✅ Stable API | Low | Well-documented, stable across kernel versions |
| **ARM MTE/PAC hardware** | ⚠️ Limited availability | Medium | Emulated in QEMU, ARMv9 hardware slowly rolling out |
| **Python 3.9+** | ✅ Stable | Low | Required for analysis scripts |
| **NetworkX** | ✅ Stable | Low | Graph analysis library |

### Risks & Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|--------|-----------|------------|
| **Transpiler performance doesn't meet target** | High | Medium | Early prototyping in Phase 1, continuous benchmarking, iterative optimization |
| **GCC compatibility issues with kernel code** | High | Medium | Test against multiple kernel versions (5.15 LTS, 6.1 LTS, 6.6 LTS), close engagement with kernel team |
| **Type information incomplete from AST** | Medium | Low | Fall back to BTF (BPF Type Format) if needed, validate early in Phase 1 |
| **Upstream resistance to complexity** | High | Medium | Demonstrate value with CVE case studies, minimize kernel changes, show distro demand |
| **Timeline slippage** | Medium | Medium | Prioritize ruthlessly (P0 only for v1.0), defer non-critical features to v1.1 |
| **Transpiler bugs introduce security vulnerabilities** | High | Low | Extensive testing, fuzzing, security review, formal verification (future work) |
| **RedHat/distro requirements change** | Medium | Low | Maintain close communication with distro stakeholders, adapt quickly |
| **Compiler behavior differences cause functional bugs** | Medium | Medium | Comprehensive cross-compiler testing, CI matrix, conservative code generation |

### Risk Monitoring

- **Weekly**: Review progress against timeline, adjust priorities as needed
- **Monthly**: Stakeholder check-ins (distro contacts, academic collaborators)
- **Per-Phase**: Formal risk assessment and mitigation plan update

---

## Team & Resources

### Core Team (if applicable - adjust or remove this section as needed)

- **Lead Architect**: [Name] - Overall design, architectural decisions
- **Transpiler Core Team**: [Names] - AST parsing, code generation, compiler compatibility
- **Kernel Integration Team**: [Names] - Kbuild integration, runtime support, kernel debugging
- **Analysis & Policy Team**: [Names] - Graph algorithms, policy generation, CVE analysis
- **Testing & Validation Team**: [Names] - CI/CD, performance benchmarking, security testing
- **Documentation Team**: [Names] - User guides, developer docs, whitepapers

### Collaboration & Communication

- **Weekly meetings**: Architecture and technical sync
- **Bi-weekly meetings**: Stakeholder updates (distros, advisors)
- **Mailing list**: hakc-dev@[domain] for development discussions
- **Slack/Discord**: Real-time collaboration (invite-only during beta)
- **GitHub**: Issue tracking, pull requests, code review

### Resource Requirements

- **Hardware**: x86_64 and ARM64 development/test machines, QEMU environments
- **Cloud CI**: GitHub Actions or equivalent for multi-architecture matrix testing
- **Performance lab**: Dedicated hardware for reproducible benchmarking
- **Distro partnerships**: Access to distro build infrastructure (RedHat Beaker, etc.)

---

## How to Contribute

We welcome contributions from the community! Here's how to get involved:

### For Users

- **Try HAKC**: Follow the [ROS2 Demo directions in the README](README.md) to build a HAKC-protected kernel
- **Report bugs**: File issues on [GitHub Issues](https://github.com/[org]/HAKC/issues)
- **Provide feedback**: Share your experience on the mailing list or discussions
- **Spread the word**: Star the repo, share on social media, write blog posts

### For Developers

- **Read the docs**: Start with [CONTRIBUTING.md](CONTRIBUTING.md)
- **Find an issue**: Check [open issues](https://github.com/[org]/HAKC/issues) for tasks labeled `good-first-issue` or `help-wanted`
- **Submit patches**: Follow the kernel patch submission workflow (or GitHub PR workflow, depending on project policy)
- **Review code**: Help review pull requests and provide constructive feedback
- **Write tests**: Expand test coverage, add edge cases, improve CI

### For Researchers

- **Collaborate**: Reach out to discuss research ideas leveraging HAKC
- **Cite HAKC**: If you use HAKC in your research, please cite using the reference in README.md
- **Contribute algorithms**: New compartmentalization policies, analysis techniques, optimization strategies

### For Distributors

- **Pilot program**: Contact us to participate in early distro integration testing
- **Provide feedback**: Share requirements, constraints, and compatibility issues
- **Sponsor development**: Support specific features or platforms needed for your distro

---

## Roadmap Governance

This roadmap is a **living document** and will evolve based on:

- **Community feedback**: Priorities may shift based on user needs
- **Technical discoveries**: New challenges or opportunities may arise during implementation
- **External factors**: Upstream kernel changes, new hardware, security threats

### Update Process

- **Minor updates** (typos, clarifications): Direct commits by maintainers
- **Major changes** (timeline shifts, scope changes): Propose via GitHub issue, discuss, then PR
- **Quarterly reviews**: Formal roadmap review and update each quarter
- **Transparency**: All changes documented in git history and CHANGELOG.md

### Proposing Changes

To propose a roadmap change:

1. Open a GitHub issue with title: `[Roadmap] Proposal: <summary>`
2. Describe the proposed change and rationale
3. Tag with `roadmap` label
4. Discuss in comments
5. Maintainers will make a decision within 2 weeks
6. If approved, submit a PR updating ROADMAP.md

---

## Questions or Feedback?

- **Email**: hakc@ll.mit.edu
- **Issue tracker**: https://github.com/HAKC-MSV/HAKC/issues

---

**Last Updated**: July 23, 2026  
**Roadmap Version**: 1.0  
**Next Review**: October 2026
