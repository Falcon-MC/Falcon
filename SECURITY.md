# Security Policy

Falcon is a Minecraft: Bedrock Edition server exposed to the internet. A vulnerability can mean a server that
anyone can take offline, corrupted worlds, or stolen player identities. Reports are taken seriously, and we
are grateful to everyone who takes the time to send one.

---

## Supported Versions

| Version                          | Supported                          |
|----------------------------------|------------------------------------|
| Latest release and `main`        | ✅ Fixes land here                  |
| Older releases                   | ❌ Not supported - update first     |
| Forks and modified builds        | ❌ Report to the fork's maintainer  |

**Before reporting, reproduce the issue on the latest release or on `main`.** Fixes are not backported.

---

## Reporting a Vulnerability

**Do not open a public issue, pull request or discussion for a security vulnerability.** Public disclosure
before a fix exists puts every server running Falcon at risk.

Report it privately through **GitHub Private Vulnerability Reporting**:

👉 **[Report a vulnerability](https://github.com/Falcon-MC/Falcon/security/advisories/new)**

This is also reachable from the repository's **Security** tab → **Report a vulnerability**. It creates a
private advisory visible only to you and the maintainers until it is published.

### What to include

- The Falcon version and commit (`/about`)
- Operating system
- Bedrock client version, if the attack comes from a client
- **Impact**: what an attacker gains - a crash, memory corruption, an authentication bypass, item duplication
- **Reproduction steps**, ideally a minimal proof of concept such as a packet capture, a script or a world file
- **Preconditions**: does the attacker need to be logged in, operator, or need a specific setting or pack?
- A suggested fix, if you have one

---

## Scope

### In scope

- **Remote code execution** or **memory corruption** triggered by network input
- **Denial of service from client input**: a malformed packet, NBT payload, item, skin, form response or
  command that crashes the server, stalls the tick loop, or exhausts memory or disk
- **Authentication and permission bypass**: Xbox Live authentication or encryption bypass, identity spoofing,
  operator escalation, allowlist or ban bypass
- **Arbitrary file read, write or deletion** through world files, resource packs or behavior packs
- **Script sandbox escapes**: a behavior pack script reaching the host beyond the scripting API
- **Duplication or world corruption** that an unprivileged player can trigger

### Out of scope

- **Anything requiring operator or console access.** Operators are trusted by design.
- **Anything requiring access to the host** or to the server's configuration files.
- **Vanilla parity bugs.** File them as normal issues.
- **Vulnerabilities in the Bedrock client itself.** Report those to Mojang.
- **Misconfiguration**, such as running with `online-mode=false` on a public server.
- **Dependency CVEs with no demonstrated impact on Falcon.** Open a normal issue instead.

Not sure whether something is in scope? Report it privately anyway.

---

## What Happens Next

1. **Acknowledgement.** We aim to confirm receipt within a few days.
2. **Triage.** We reproduce the issue and share our assessment of its severity.
3. **Fix.** We work on a patch and keep you updated.
4. **Coordinated disclosure.** We agree on a disclosure date with you.
5. **Publication.** We publish a GitHub Security Advisory and credit you by name or handle, or keep you
   anonymous if you prefer.

---

## Disclosure Expectations

- Give us a chance to fix the issue before making it public.
- Only test against servers you own. Do not attack public servers or access other people's data.
- Stop at a proof of concept.

There is no paid bug bounty. What we can offer is credit in the advisory and in the release notes, and our
genuine thanks.

---

## For Server Operators

- Update to the [latest release](https://github.com/Falcon-MC/Falcon/releases/latest) regularly and verify its
  `.sha256` checksum.
- Watch this repository (**Watch** → **Custom** → **Security alerts**) to be notified of advisories.
- Keep `online-mode=true` on any public server.
- Run the server as an unprivileged user, never as root or Administrator.
- Only install resource packs and behavior packs from sources you trust.

Thank you for helping keep Falcon and its server operators safe.
