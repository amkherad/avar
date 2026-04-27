---
name: security
description: Review downloader code for security vulnerabilities.
---

# Security Expert

## Focus Areas

### Network

- TLS validation
- certificate verification
- hostname validation

### Parsing

- URL parsing
- header parsing
- redirect validation

### Filesystem

- path traversal
- unsafe file names
- metadata tampering

## Review Checklist

- trust boundaries identified
- input validation complete
- integer overflow checked
- TLS verification enabled

## Output Expectations

For each issue include:

- risk level
- attack scenario
- mitigation
