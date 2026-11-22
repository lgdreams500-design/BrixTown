# Development environment setup (Windows)

Status legend: ☐ not done · ☑ done

## 1. Check the machine first
| Item | Guidance |
|---|---|
| Free disk space | Plan for 150 GB+ on an SSD: engine, debug symbols, Visual Studio, project cache. |
| GPU | A DirectX 12 card with recent drivers; Lumen and Nanite want a modern mid-range GPU or better. |
| RAM | 32 GB is comfortable for editor + compiling; 64 GB helps with large worlds. |
| OS | Windows 10/11 64-bit, fully updated. |

Check Epic's hardware and software specification page for UE 5.8 for exact figures before installing.

### This machine (checked 2026-11-21)
| Item | Found | Verdict |
|---|---|---|
| Model | ASUS ExpertBook P5405CAA, Windows 11 Home | OK |
| CPU | Intel Core Ultra 7 356H, 16 cores | Good for compiling |
| RAM | 32 GB | Good |
| Disk C: | 951 GB total, 820 GB free | Good |
| GPU | Intel integrated graphics only, no discrete card | **Weak point.** The editor will run, but Lumen, Nanite and large scenes will be slow. Use scalability settings at Medium or lower, and treat this laptop as below the game's eventual minimum spec. |
| App control | System Information reports "App Control for Business policy: Enforced" | Watch for Windows blocking locally compiled, unsigned game binaries once C++ work starts. |

### Install status (2026-11-21)
| Tool | Status |
|---|---|
| Blender | Installed from the Microsoft Store: version 5.3 (the Store has no 5.2 LTS listing; it auto-updates). |
| Epic Games Launcher | Installed from the Microsoft Store; waiting for Epic account sign-in. |
| Unreal Engine 5.8 | Not yet installed (needs the sign-in above). |
| Visual Studio, Git, Git LFS | Not yet installed. |

## 2. Install order
| # | Step | Notes | Done |
|---|---|---|---|
| 1 | Git and Git LFS | git-scm.com; then run `git lfs install` once. | ☐ |
| 2 | Visual Studio with the **Game development with C++** workload | Use the Visual Studio version and MSVC toolchain listed in the UE 5.8 release notes. Required for C++ projects and dedicated-server builds. | ☐ |
| 3 | Epic Games Launcher | From unrealengine.com/download. Needs an Epic account (sign in yourself). | ☐ |
| 4 | Unreal Engine 5.8 | Launcher → Unreal Engine → Library → add engine version 5.8. In install options, tick **Editor symbols for debugging** only if disk allows. | ☐ |
| 5 | Blender 5.2 LTS | From blender.org/download (pick the LTS build). | ☐ |
| 6 | Create the Unreal project | New project → Games → Third Person → **C++**, location `C:\Projects\Wasted\Game`, name `BrixTown` (the game is Brix Town, ADR-0025). | ☐ |
| 7 | First commit | `git init`, `git lfs install`, commit the skeleton. `.gitignore` and `.gitattributes` are already in the repo root. | ☐ |

## 3. Later (PH0 completion)
- Dedicated server target requires a **source build** of Unreal Engine from Epic's GitHub (the Launcher build cannot compile server targets). Link your Epic and GitHub accounts to get access.
- Editor plugins to enable: Data Validation, Functional Testing Editor, Gauntlet.
- CI runner setup.

## 4. Version pinning
| Tool | Version | Pinned on |
|---|---|---|
| Unreal Engine | 5.8.x | 2026-11-21 |
| Blender | 5.2 LTS | 2026-11-21 |

Record the exact patch versions here once installed.
