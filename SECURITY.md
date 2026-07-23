# Security Policy

## Supported Versions

HAKC is currently in active development. Security updates and support are provided for:

- **LLVM**: Based on a custom fork tracking LLVM development
- **Linux Kernel**: Tracks Long Term Support (LTS) kernel versions
- **HAKC**: Current development branch and recent tagged releases

| Component | Version/Branch | Supported |
|-----------|----------------|-----------|
| HAKC | Current `main` branch | ✅ Yes |
| HAKC | Tagged releases | ✅ Yes |
| LLVM | Custom fork | ✅ Yes |
| Linux Kernel | LTS-based | ✅ Yes |

Security patches are provided for the current development state and will be applied to stable releases as they are established.

## Reporting a Vulnerability

**IMPORTANT: Do NOT report security vulnerabilities through public GitHub issues.**

If you discover a security vulnerability in HAKC, please report it privately to ensure responsible disclosure and to protect users while a fix is being developed.

### How to Report

**Email:** derrick.mckee@ll.mit.edu

**Subject Line:** `[HAKC SECURITY] <brief description>`

**Encryption:** PGP encryption is available upon request for sensitive reports.

### What to Include in Your Report

Please provide as much information as possible to help us understand and reproduce the issue:

1. **Description**: Detailed description of the vulnerability
2. **Impact**: What can an attacker do? What is at risk? What data or systems are compromised?
3. **Reproduction Steps**: Step-by-step instructions to reproduce the vulnerability
4. **Environment Details**:
   - Linux kernel version
   - Architecture (x86_64, aarch64, etc.)
   - HAKC version or git commit hash
   - LLVM version
   - Any relevant configuration details
5. **Proof of Concept**: Code, commands, or other artifacts demonstrating the issue (if available)
6. **Suggested Fix**: If you have ideas for remediation, please share them
7. **Credit**: How you would like to be credited when the vulnerability is disclosed (or if you prefer to remain anonymous)

### What to Expect

#### Response Timeline

1. **Acknowledgment**: Within **5 business days** of your report
   - We'll confirm receipt and provide initial assessment
   
2. **Initial Assessment**: Within **10 business days**
   - Confirm whether the issue is a vulnerability
   - Assess severity using CVSS or similar framework
   - Provide initial timeline estimate

3. **Regular Updates**: At least every **14 days**
   - Progress updates on fix development and testing
   
4. **Resolution Timeline** (depends on severity):
   - **Critical**: Expedited fix (target **30 days**)
   - **High**: Target **60 days**
   - **Medium**: Target **90 days**
   - **Low**: Target **90 days**

These are target timelines. Complex issues may require more time, and we'll keep you informed of any changes.

#### Coordinated Disclosure

We follow responsible disclosure practices:

- Vulnerabilities and fixes are developed **privately** until ready for release
- **Public disclosure** occurs after a fix is available and users have time to update (typically **90 days** from report)
- We coordinate disclosure timing with you, the reporter
- **Credit** is given to reporters in security advisories (unless anonymity is requested)
- We may request a **CVE ID** for significant vulnerabilities

## Vulnerability Handling Process

When you report a security issue, here's what happens:

### 1. Triage
- Confirm the vulnerability
- Assess severity and impact using CVSS or similar metrics
- Determine affected versions and components

### 2. Development
- Create a fix in a private repository branch
- Review the fix thoroughly
- Consider backwards compatibility and deployment concerns

### 3. Testing
- Test the fix across supported configurations
- Verify the fix resolves the issue without introducing regressions
- Test on multiple architectures and kernel versions

### 4. CVE Assignment
- Request a CVE identifier for significant vulnerabilities
- Prepare CVE description and metadata

### 5. Release Preparation
- Prepare security advisory
- Create patch or updated release
- Plan coordinated disclosure timeline

### 6. Disclosure
- Publish GitHub Security Advisory
- Release patch/update
- Notify users through available channels
- Coordinate public disclosure with reporter

## Security Update Distribution

Security fixes and advisories will be distributed through:

- **GitHub Security Advisories**: Primary mechanism for security notifications
- **Git Tags**: Security releases tagged appropriately (e.g., `v1.2.3-security`)
- **Release Notes**: Security fixes highlighted in release notes
- **Mailing List**: (To be established for project updates and security notifications)

## Scope

### In Scope

Security vulnerabilities in HAKC-specific code:
- LLVM compartmentalization passes (`llvm-project/llvm/lib/Transforms/Compartmentalization/`)
- Linux kernel HAKC integration (`linux/kernel/hakc/`, `linux/include/linux/hakc/`)
- Python policy server and analysis tools (`llvm-project/llvm/utils/hakc/`, `python/`)
- Build system vulnerabilities that could lead to exploitation
- Documentation that could lead to insecure usage

### Out of Scope

- **Upstream Vulnerabilities**: Issues in upstream LLVM or Linux kernel (report to those projects)
- **Third-Party Dependencies**: Issues in Kuzu, Python libraries, etc. (report to dependency maintainers)
- **Theoretical Issues**: Issues without practical exploit scenarios
- **Denial of Service**: In build/analysis tools only (DoS in runtime is in scope)
- **Social Engineering**: Phishing, impersonation, or other social attacks

If you're unsure whether an issue is in scope, please report it and we'll make the determination.

## Security Best Practices

### For Users

When using HAKC in your projects:

1. **Keep Updated**: Use the latest HAKC code and LTS kernel versions
2. **Test Thoroughly**: Validate compartmentalization policies in test environments before production
3. **Monitor Advisories**: Subscribe to security advisories for HAKC, LLVM, and Linux kernel
4. **Review Policies**: Regularly review and audit your compartmentalization policies
5. **Report Issues**: If you discover potential vulnerabilities, report them promptly

### For Contributors

When contributing code:

1. **Secure Coding**: Follow secure coding practices
2. **Avoid Vulnerabilities**: Be mindful of common vulnerabilities (buffer overflows, injection attacks, etc.)
3. **Security Impact**: Consider security implications of compartmentalization policy changes
4. **Review Carefully**: Security-sensitive changes receive extra scrutiny during review
5. **Test Security**: Include security-focused test cases where appropriate

### Compartmentalization Security

HAKC itself is a security mechanism. When designing and implementing compartmentalization:

- **Principle of Least Privilege**: Compartments should have minimal necessary permissions
- **Defense in Depth**: Compartmentalization is one layer; use in conjunction with other security measures
- **Test Attack Scenarios**: Test that compartmentalization boundaries hold under attack
- **Monitor Runtime**: Consider runtime monitoring and anomaly detection

## Security Considerations

### HAKC Architecture

HAKC operates at multiple levels:

1. **Compile-Time**: LLVM passes transform code
2. **Build-Time**: Policy server makes compartmentalization decisions  
3. **Runtime**: Linux kernel enforces compartment boundaries

Security issues can arise at any of these levels, so reports concerning any component are welcome.

### Trust Model

- **Policy Server**: Trusted component that defines compartmentalization
- **LLVM Passes**: Trusted components running during compilation
- **Kernel Integration**: Trusted component enforcing runtime security
- **User Code**: Untrusted code being compartmentalized

Vulnerabilities that break this trust model are especially critical.

## Acknowledgments

We deeply appreciate security researchers and contributors who help keep HAKC secure. Responsible disclosure helps protect all HAKC users.

Security researchers who report valid vulnerabilities will be:
- Credited in security advisories (unless anonymity is requested)
- Acknowledged in project documentation
- Thanked publicly in release notes

## Contact

**For security concerns:** derrick.mckee@ll.mit.edu  
**Subject line:** `[HAKC SECURITY] <brief description>`

**For general questions:**
- Contributing: See [CONTRIBUTING.md](CONTRIBUTING.md)
- Maintainers: See [MAINTAINERS.md](MAINTAINERS.md)
- Code of Conduct: See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

## Additional Resources

- **CVE Database**: https://cve.mitre.org/
- **CVSS Calculator**: https://www.first.org/cvss/calculator/3.1
- **Linux Kernel Security**: https://www.kernel.org/category/security.html
- **LLVM Security**: https://llvm.org/
- **Responsible Disclosure**: https://en.wikipedia.org/wiki/Responsible_disclosure

Thank you for helping keep HAKC and its users safe!
