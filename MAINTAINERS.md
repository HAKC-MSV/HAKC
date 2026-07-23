# HAKC Maintainers

This document lists the maintainers of the HAKC (Hardware-Assisted Kernel Compartmentalization) project and describes the maintainer responsibilities and governance structure.

## Current Maintainers

### Project Lead & Maintainer

**Derrick McKee**  
Email: derrick.mckee@ll.mit.edu  
Role: Project Lead & Maintainer  

**Responsibilities:**
- Overall project direction and technical leadership
- Review and approval of all contributions
- Security issue coordination and response
- Community guidance and governance
- Final decision-making authority on technical matters

## Areas of Responsibility

As the project grows, maintainership may be divided by area. Currently, all areas are maintained by Derrick McKee:

### 1. LLVM Compartmentalization Pass
- **Paths:** 
  - `llvm-project/llvm/lib/Transforms/Compartmentalization/`
  - `llvm-project/llvm/include/llvm/Transforms/Compartmentalization/`
- **Current Maintainer:** Derrick McKee
- **Scope:** LLVM-based compiler transformations, pass implementation, optimization

### 2. Linux Kernel Integration
- **Paths:** 
  - `linux/kernel/hakc/`
  - `linux/include/linux/hakc/`
- **Current Maintainer:** Derrick McKee
- **Scope:** Kernel-side compartmentalization support, runtime enforcement

### 3. Python Tooling & Analysis
- **Paths:** 
  - `python/`
  - `llvm-project/llvm/utils/hakc/`
- **Current Maintainer:** Derrick McKee
- **Scope:** Policy server, static analysis tools, DAG generation

### 4. Build System & Infrastructure
- **Paths:** 
  - `CMakeLists.txt`
  - `CMakePresets.json`
  - `scripts/`
  - `configs/`
- **Current Maintainer:** Derrick McKee
- **Scope:** Build configuration, CI/CD, Docker setup

### 5. Documentation
- **Paths:** 
  - `docs/`
  - `README.md`
  - Governance documents
- **Current Maintainer:** Derrick McKee
- **Scope:** Technical documentation, guides, project governance

## Maintainer Responsibilities

Maintainers are expected to:

- **Review Code**: Review pull requests thoroughly for correctness, security, performance, and maintainability
- **Respond Promptly**: Acknowledge issues and PRs in a timely manner
- **Guide Development**: Provide technical direction and architectural guidance
- **Ensure Quality**: Maintain high standards for code quality, testing, and documentation
- **Security**: Handle security reports responsibly and coordinate fixes
- **Community**: Foster a welcoming, inclusive, and professional community
- **Enforce Standards**: Uphold the Code of Conduct and contribution guidelines
- **Mentor Contributors**: Help new contributors learn the codebase and development process
- **Make Decisions**: Make final decisions on technical disputes or design choices

## Review & Approval Process

### Current Process

- All pull requests require approval from **Derrick McKee** before merging
- Reviews focus on:
  - **Correctness**: Does the code work as intended?
  - **Security**: Are there any security implications?
  - **Performance**: Does this impact performance positively or negatively?
  - **Maintainability**: Is the code clear, well-documented, and maintainable?
  - **Testing**: Are tests adequate and comprehensive?
  - **Style**: Does the code follow project conventions?

- All automated tests must pass before merge
- At least one approval is required before merge

### Future Process

As the project grows and additional maintainers join:

- Complex changes may require multiple reviewer approvals
- Component-specific reviewers may be designated for their areas of expertise
- Emergency/security fixes may use an expedited process

## Becoming a Maintainer

As HAKC grows, we welcome new maintainers who demonstrate:

### Criteria

1. **Technical Expertise**: Deep understanding of relevant areas (LLVM, Linux kernel, compartmentalization, security)
2. **Sustained Contribution**: Consistent, high-quality contributions over an extended period
3. **Community Engagement**: Active participation in code reviews, issue discussions, and helping other contributors
4. **Sound Judgment**: Demonstrated good judgment in technical decisions and community interactions
5. **Alignment**: Understanding of and alignment with project goals and values
6. **Trustworthiness**: Earned trust of existing maintainers and the community

### Process

1. **Nomination**: An existing maintainer nominates a candidate
2. **Discussion**: Maintainers discuss the nomination (currently Derrick McKee makes the decision)
3. **Agreement**: Agreement on specific areas of responsibility and expectations
4. **Announcement**: Public announcement to the community
5. **Documentation**: Update this MAINTAINERS.md file with new maintainer information
6. **Access**: Grant appropriate repository and infrastructure access

### Expectations for New Maintainers

- Commit to ongoing engagement with the project
- Uphold project values and Code of Conduct
- Collaborate constructively with other maintainers
- Continue learning and growing with the project

## Maintainer Changes

### Stepping Down

- Maintainers may step down at any time for any reason
- Provide advance notice when possible to ensure smooth transition
- Work with remaining maintainers to transfer responsibilities

### Emeritus Status

- Maintainers who step down or become inactive may be granted emeritus status
- Emeritus maintainers are honored for their contributions
- They may be listed in a separate section acknowledging their past service

### Removal

In rare cases where a maintainer:
- Violates the Code of Conduct
- Is consistently unresponsive or inactive
- Acts against the interests of the project

The project lead may remove maintainer status after discussion and attempted resolution.

## Decision Making

### Consensus Model

- **Preference**: Strive for consensus among maintainers on major decisions
- **Discussion**: Technical decisions should be discussed openly (in issues, PRs, or other forums)
- **Documentation**: Major architectural or policy decisions should be documented

### Escalation

- If consensus cannot be reached, the Project Lead (Derrick McKee) makes the final decision
- Final decisions should consider all perspectives and be explained clearly

### Types of Decisions

- **Minor**: Bug fixes, small improvements - single maintainer approval
- **Moderate**: New features, refactoring - maintainer review and discussion
- **Major**: Architectural changes, API changes, governance - require broader discussion and Project Lead approval

## Communication

### For Contributors

- Pull Request reviews and feedback
- Issue triage and responses
- Technical guidance in discussions

### For Maintainers

- Currently: Direct communication with Derrick McKee
- Future: Maintainer mailing list or private communication channel may be established

### Public Communication

- Major decisions and project direction communicated via:
  - GitHub Issues and Discussions
  - Project documentation updates
  - Announcements in README or docs

## Acknowledgments

HAKC was developed at MIT Lincoln Laboratory.

We are grateful for the contributions of all community members, and we look forward to growing the maintainer team as the project evolves.

## Questions?

If you have questions about:
- **Contributing**: See [CONTRIBUTING.md](CONTRIBUTING.md)
- **Security**: See [SECURITY.md](SECURITY.md)
- **Code of Conduct**: See [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- **Maintainership**: Contact Derrick McKee at derrick.mckee@ll.mit.edu
