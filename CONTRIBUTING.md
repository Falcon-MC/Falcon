# Contributing to Falcon

Thanks for taking the time to contribute. Bug fixes, gameplay features, performance work and documentation
improvements are all welcome.

**Before you open a pull request, read [Pull Requests](#-pull-requests) and [AI Tool Usage](#-ai-tool-usage).**
Those two sections describe what gets pull requests closed.

**Found a security vulnerability?** Do **not** open a public issue. See [SECURITY.md](SECURITY.md).

---

## 🌱 New to Open Source?

- [Finding ways to contribute on GitHub](https://docs.github.com/en/get-started/exploring-projects-on-github/finding-ways-to-contribute-to-open-source-on-github)
- [Setting up Git](https://docs.github.com/en/get-started/getting-started-with-git/set-up-git)
- [Understanding GitHub flow](https://docs.github.com/en/get-started/using-github/github-flow)
- [Collaborating with pull requests](https://docs.github.com/en/github/collaborating-with-pull-requests)

Read the [README](README.md) first for an overview of the project.

---

## 📋 Issues

### Reporting a bug

[Search existing issues](https://github.com/Falcon-MC/Falcon/issues) first. Yours may already be tracked or
fixed. If not, [open a new issue](https://github.com/Falcon-MC/Falcon/issues/new) and include:

- The Falcon version and commit, as printed by `/about`
- Your operating system
- The Bedrock client version
- Exact steps to reproduce
- Expected vs. actual behaviour, compared with a vanilla world when it is a gameplay bug
- The server log, including the full crash output if there is one

Reproduce on a **fresh world without behavior packs** if you can. If the bug only happens with a pack
loaded, say so explicitly.

### Working on an issue

Issues are not pre-assigned. Pick one and open a pull request. For anything large or architectural, comment
on the issue first so the approach can be agreed on before you write the code.

---

## 🔧 Making Changes

1. **Fork** the repository and clone your fork.
2. Install the toolchain:

   | Platform | Requirements                                                                           |
   |----------|----------------------------------------------------------------------------------------|
   | Windows  | [MSYS2](https://www.msys2.org) UCRT64 with `gcc`, `cmake`, `ninja`, `openssl`, `zlib` |
   | Linux    | `g++` with C++17 support, `cmake` 3.16+, `ninja`, OpenSSL and zlib development headers |
   | macOS    | Xcode command line tools, `cmake`, `ninja`, `openssl@3`                                |

3. Branch off `main`. Work that targets an upcoming protocol version goes on the branch named after that
   version.
4. Build:

   | Command       | Purpose                                                |
   |---------------|--------------------------------------------------------|
   | `build.bat`   | Configure and build on Windows                         |
   | `./build.sh`  | Configure and build on Linux and macOS                 |

   The first configure downloads the dependencies with CMake `FetchContent`, so it needs network access
   and takes a while. The executable is written to `build/Falcon.Server/`.

5. **Start the server and test your change in-game** before opening a pull request.

### Repository layout

| Path                                   | Contents                                                            |
|----------------------------------------|---------------------------------------------------------------------|
| `Falcon.Server/src/Network/Handler`    | Network session, login, movement, inventory and gameplay handlers  |
| `Falcon.Server/src/Level`              | Worlds, chunks, storage, world generation                           |
| `Falcon.Server/src/Block`              | Blocks, block actors and block systems (redstone, fluids, fire...)  |
| `Falcon.Server/src/Item`               | Items and item behaviours                                           |
| `Falcon.Server/src/Actor`              | Players, mobs, projectiles and AI goals                             |
| `Falcon.Server/src/Command`            | Commands                                                            |
| `Falcon.Server/src/Scripting`          | JavaScript scripting API for behavior packs                         |

The network layer, the protocol, NBT and the Bedrock data files live in their own repositories under
[Falcon-MC](https://github.com/Falcon-MC). Changes to them go there.

---

## 🎨 Code Quality & Style

- Match the **existing style** of the file you are editing. When in doubt, copy the surrounding code.
- **One logical change per pull request.** Split unrelated fixes into separate pull requests.
- **No unrelated churn**: no drive-by reformatting, include reordering or refactors outside your scope.
- **Vanilla is the reference.** Gameplay changes must match what Minecraft: Bedrock Edition actually does.
  Explain in the pull request how you checked it.
- **Reuse before you write.** Look for an existing helper, system or base class before adding a new one.
  Duplicated logic will be sent back.
- **Behaviour belongs to classes.** A block, item or actor with behaviour gets its own class, registered in
  its registry, and overrides virtual methods. Systems must not decide what something is by comparing
  identifier strings.
- **Thread ownership.** Game state belongs to the main thread. Chunk workers and the network thread only
  exchange data through queues.
- **No dead code** and no leftover debug logging.
- **New dependencies need justification.** Open an issue before adding one.
- If the change is performance-sensitive, include **profiler numbers** from `/profiler` in the pull request.
- **Commit messages** follow [Conventional Commits](https://www.conventionalcommits.org):
  `fix: stop coral from dying underwater`, `feat: add sniffer egg hatching`, `refactor: split the network handler`.

---

## 📬 Pull Requests

- [Link the issue](https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue)
  with `Closes #123` or `Fixes #123`.
- Enable **"Allow maintainer edits"**.
- Open a **draft pull request** if the work is not finished.
- **CI must pass** on Linux, Windows and macOS.
- **Every pull request must be tested on a running server.** Describe exactly what you tested and how.
  "Tested" alone is not a test report.
- Reply to review feedback and resolve conversations once addressed. Push follow-up commits rather than
  force-pushing while a review is in progress.

### Pull requests closed without review

- Code that does not compile, or that fails CI with no follow-up.
- Untested changes, or a testing section that is empty, generic or made up.
- Calls to functions, classes or packets that do not exist in this codebase.
- Repository-wide reformats, blanket "optimisations" or refactors nobody asked for.
- Undisclosed AI usage.
- Duplicate or spam pull requests.

You are welcome to fix the underlying problems and open a new one.

---

## 🤖 AI Tool Usage

AI tools are allowed. Hiding them, or submitting their output unread, is not.

> If you use a coding agent, point it at [AGENTS.md](AGENTS.md) first.

- **Disclose it.** State which model you used and which part of the work it did: code, research, commit
  messages. Name the model and version, not just the vendor.
- **Write your own prose.** Pull request descriptions, issues and review replies must be in your own words.
  Translators and spell checkers are fine.
- **Own the output.** You must have read every line you submit and be able to explain it.
- **No AI-only bug reports.** Reproduce the bug on a real server before opening an issue.

---

## ⚖️ Licensing

- Contributions are licensed under the **[LGPL-3.0](LICENSE)**, like the rest of Falcon.
- Do not copy code from projects with an incompatible license.
- If your contribution is derived from another open-source project, name the source and its license in the
  pull request.

---

## 🤝 Conduct

- Be kind, patient and constructive, especially with newcomers.
- Critique code, not people.
- Maintainers have the final say on what fits the project. A closed pull request is not a personal rejection.

Happy contributing! 🚀
