# Contributing to HAKC

Thank you for your interest in contributing to HAKC (Hardware-Assisted Kernel Compartmentalization)! This document provides guidelines for contributing to the project.

## Table of Contents

- [Getting Started](#getting-started)
- [Ways to Contribute](#ways-to-contribute)
- [Development Workflow](#development-workflow)
- [Legal Requirements](#legal-requirements)
- [Pull Request Process](#pull-request-process)
- [Code Review](#code-review)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Communication](#communication)
- [Code of Conduct](#code-of-conduct)
- [Acknowledgments](#acknowledgments)

## Getting Started

HAKC is a compartmentalization system for the Linux kernel using LLVM-based compiler transformations. Before contributing, please:

1. Read the [README.md](README.md) for build instructions
2. Review the documentation in [docs/](docs/)
3. Set up your development environment:
   ```bash
   git submodule update --init --recursive
   source .envrc
   cmake --preset debug
   cmake --build $HAKC_BUILD_ROOT --target clang
   ```

## Ways to Contribute

### Reporting Bugs

- Use GitHub Issues to report bugs
- Include: environment details, reproduction steps, expected vs actual behavior
- Check existing issues first to avoid duplicates

### Feature Requests

- Open a GitHub Issue describing the proposed feature
- Explain the use case and potential benefits
- Discuss design before implementing large features

### Code Contributions

- Bug fixes
- New features
- Performance improvements
- Code refactoring

### Documentation

- Improve existing documentation
- Add examples and tutorials
- Fix typos and clarify instructions

### Testing

- Add test cases
- Test on different platforms/configurations
- Report test results

## Development Workflow

### 1. Fork and Clone

```bash
git clone https://github.com/YOUR-USERNAME/HAKC.git
cd HAKC
git remote add upstream https://github.com/HAKC-MSV/HAKC.git
```

### 2. Create a Branch

```bash
git checkout -b feature/my-new-feature
# or
git checkout -b bugfix/issue-123
```

Branch naming conventions:
- `feature/` - New features
- `bugfix/` - Bug fixes
- `docs/` - Documentation changes
- `test/` - Test additions/improvements

### 3. Make Changes

Follow project coding standards:

**C/C++ Code:**
- Follow the LLVM coding style
- Use the `.clang-format` configuration in the repository
- Format code: `clang-format -i <file>`

**Python Code:**
- Follow PEP 8 style guide
- Use meaningful variable names
- Add docstrings to functions

**Commit Messages:**
- Clear, descriptive first line (50 characters or less)
- Blank line followed by detailed explanation if needed
- Reference issues: "Fixes #123" or "Related to #456"

### 4. Test Your Changes

Run the test suite:

```bash
# Run HAKC tests
cmake --build $HAKC_BUILD_ROOT --target check-hakc

# Run LLVM tests
cmake --build $HAKC_BUILD_ROOT --target check-llvm

# Run unit tests
cmake --build $HAKC_BUILD_ROOT --target UnitTests
```

Ensure:
- All existing tests pass
- New features include tests
- Bug fixes include regression tests

### 5. Update Documentation

- Update relevant documentation in `docs/`
- Update README.md if needed
- Add code comments for complex logic

## Legal Requirements

All contributions to HAKC require compliance with the following:

### Developer Certificate of Origin (DCO)

All commits must be signed off, indicating you agree to the Developer Certificate of Origin (DCO). The full DCO text is available at https://developercertificate.org/

To sign off commits, use:

```bash
git commit -s -m "Your commit message"
```

This adds a `Signed-off-by` line to your commit message:

```
Signed-off-by: Your Name <your.email@example.com>
```

The sign-off certifies that you wrote the code and have the right to submit it under the project's open source license.

### Contributor License Agreement (CLA)

First-time contributors will be asked to sign a Contributor License Agreement (CLA). This ensures the project can use and distribute your contributions.

**Note:** The CLA process is currently being established. You will be contacted with instructions when you submit your first pull request.

## Pull Request Process

### 1. Push Your Branch

```bash
git push origin feature/my-new-feature
```

### 2. Open a Pull Request

- Go to the HAKC repository on GitHub
- Click "New Pull Request"
- Select your branch
- Fill out the PR description

### 3. PR Description Should Include:

- Summary of changes
- Motivation and context
- Related issues (e.g., "Fixes #123")
- Testing performed
- Any breaking changes

### 4. Checklist:

- [ ] All tests pass
- [ ] Code follows project style guidelines
- [ ] Documentation updated
- [ ] Commits include DCO sign-off
- [ ] New tests added for new functionality

### 5. Review Process:

- A maintainer will review your PR
- Address feedback and comments
- Update PR as needed
- Once approved, a maintainer will merge

## Code Review

### For Contributors:

- Be responsive to feedback
- Be open to suggestions
- Ask questions if feedback is unclear
- Update your PR based on review comments

### Review Focus Areas:

- Correctness and functionality
- Security implications
- Performance considerations
- Code quality and maintainability
- Test coverage
- Documentation completeness

## Project Structure

Key directories in the HAKC repository:

- **`llvm-project/llvm/lib/Transforms/Compartmentalization/`** - LLVM compartmentalization passes
- **`llvm-project/llvm/include/llvm/Transforms/Compartmentalization/`** - Pass headers
- **`llvm-project/llvm/utils/hakc/`** - Python policy server and analysis tools
- **`linux/kernel/hakc/`** - Linux kernel HAKC integration
- **`linux/include/linux/hakc/`** - Kernel headers for HAKC
- **`python/`** - Python utilities
- **`scripts/`** - Build and utility scripts
- **`configs/`** - Configuration file examples
- **`docs/`** - Project documentation

**Note:** The `llvm-project` and `linux` directories are large upstream submodules. Focus on the HAKC-specific directories listed above.

## Testing

### Unit Tests

Located in `llvm-project/llvm/unittests/Transforms/Compartmentalization/hakc/`

Run with:
```bash
cmake --build $HAKC_BUILD_ROOT --target UnitTests
llvm-project/llvm/unittests/Transforms/Compartmentalization/hakc/HAKC_UNIT_TESTS
```

### Integration Tests

LLVM lit tests are located in `llvm-project/llvm/test/Transforms/Compartmentalization/hakc/`

Run with:
```bash
cmake --build $HAKC_BUILD_ROOT --target check-hakc
```

### Kernel Tests

Test the full kernel build and QEMU execution:
```bash
cmake --build $HAKC_BUILD_ROOT --target run-x86_64-hakc-kernel
```

### Adding New Tests

- Add unit tests for new C++ functionality
- Add lit tests for compiler pass transformations
- Include test cases that cover edge cases and error conditions
- Ensure tests are deterministic and reproducible

## Communication

### GitHub Issues

- **Bug Reports**: Use the issue tracker to report bugs
- **Feature Requests**: Propose new features via issues
- **Questions**: Ask questions about usage or development

### Security Issues

**Do NOT report security vulnerabilities via public GitHub issues.**

See [SECURITY.md](SECURITY.md) for reporting security issues privately.

### Email

For questions about maintainership or project governance, see [MAINTAINERS.md](MAINTAINERS.md).

## Code of Conduct

All contributors are expected to follow our [Code of Conduct](CODE_OF_CONDUCT.md). Please read it before participating in the community.

Key principles:
- Be respectful and inclusive
- Focus on constructive feedback
- Collaborate openly and professionally

Violations can be reported to derrick.mckee@ll.mit.edu

## Acknowledgments

HAKC was developed at MIT Lincoln Laboratory.

Thank you to all contributors who help improve HAKC!

## Questions?

If you have questions about contributing:

1. Check existing documentation in [docs/](docs/)
2. Search existing GitHub issues
3. Open a new GitHub issue with your question
4. Contact the maintainers (see [MAINTAINERS.md](MAINTAINERS.md))

We appreciate your contributions and look forward to collaborating with you!
