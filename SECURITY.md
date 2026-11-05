# Security Policy

## Reporting a vulnerability

Please do not open a public issue for security vulnerabilities.

Use GitHub's private vulnerability reporting:
https://github.com/garymike/lineveil/security/advisories/new

I will acknowledge receipt within 48 hours and aim to resolve critical issues within 14 days.

## Verifying a release

Every release ships `SHA256SUMS.txt` and a Sigstore build-provenance attestation.
`gh attestation verify <file> --repo garymike/lineveil` proves a binary was built by
this repository's release workflow from the tagged commit.
