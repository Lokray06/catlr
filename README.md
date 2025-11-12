# catlr
**catlr** is a powerful, cross-platform command-line utility designed to list directory structures and print file contents recursively, specifically tailored for code auditing and project reviews. It intelligently falls back to native C++ implementations if preferred external tools (bat, tree, etc.) are unavailable, ensuring functionality on any system (Linux, macOS, Windows).

## Features

  * **Intelligent Tool Fallback:** Prioritizes system commands (bat, tree) if installed, but seamlessly switches to built-in C++ methods for file reading and tree generation if external tools are missing or configured otherwise.

  * **Granular Filtering:** Offers fine-grained control over which files and directories are **listed** (in the tree) and which are **printed** (content display), using powerful include/exclude patterns with **wildcard support** and **case-sensitive direct matching**.

  * **Cross-Platform Configuration:** Use a simple configuration file (`~/.config/catlr/catlr.conf`) to specify preferred external tools.

  * **I/O Loop Protection:** Automatically detects and skips printing any file that is also the destination of the program's output (`catlr ... > file.txt`), preventing infinite recursion errors.

-----

## 🚀 Installation (From Source)

catlr is written in C++ and requires a C++17 compliant compiler (like g++ or clang++) due to its use of the $std::filesystem$ library.

Save the source code as `catlr.cpp` and compile using one of the following commands:

| Platform | Command | Notes |
| :--- | :--- | :--- |
| **Linux (g++)** | `g++ -o catlr catlr.cpp -std=c++17 -lstdc++fs` | Uses the GNU standard filesystem library. |
| **macOS (clang++)** | `clang++ -o catlr catlr.cpp -std=c++17 -lc++fs` | Uses the LLVM standard filesystem library. |
| **Windows (MSVC)** | `cl.exe /std:c++17 /EHsc catlr.cpp` | Compiles using the Visual Studio compiler. |

After compilation, place the resulting executable (`catlr` or `catlr.exe`) in a directory listed in your system's `$PATH`.

-----

## Configuration
You can change the default tools this command uses (`bat` or `cat` and `tree`) for the output, but you are free to override them.
catlr looks for a configuration file to determine which external tools to use for directory listing and file printing.

**Configuration File Path:** `~/.config/catlr/catlr.conf`

### Setup Steps:

1.  `mkdir -p ~/.config/catlr`

2.  `nano ~/.config/catlr/catlr.conf`

3.  ```conf
    # catlr Configuration File
    # If a command is not found or this file is empty, catlr uses its native C++ implementation.
    # Use 'lsd --tree' for directory listing (or 'tree')
    treePrintCommand=lsd --tree
    # Use 'bat' for file printing (or 'cat')
    filePrintCommand=bat
    ```

-----

## 📖 Usage and Filtering

The general syntax is: `catlr [directory_path...] [filter_rules...]`

This structure allows you to specify **one or more starting directories** followed by any combination of filtering flags and their associated patterns. The filters are applied globally across all specified directories.

### Core Filtering Flags

Filtering is divided into **List** (tree output) and **Print** (file content output). The fundamental logic is that **Include always overrides Exclude**.

| Base Flag | Modifier (Scope) | Permutation (Description) |
| :--- | :--- | :--- |
| **`-e`** (Exclude) | **(None)** | Exclude the **following patterns** from **both** listing and printing. |
| | **`-l`** (List) | Exclude the **following patterns** from **listing only** (`-el` or `-le`). |
| | **`-p`** (Print) | Exclude the **following patterns** from **printing only** (`-ep` or `-pe`). |
| **`-i`** (Include) | **(None)** | Include the **following patterns** in **both** listing and printing (overrides any Exclude). |
| | **`-l`** (List) | Include the **following patterns** in **listing only** (`-il` or `-li`). |
| | **`-p`** (Print) | Include the **following patterns** in **printing only** (`-ip` or `-pi`). |

> **Note:** The order of the modifier letters (`-el` vs `-le`, `-ip` vs `-pi`) does not matter. Multiple patterns can be listed after a single flag, or the flag can be repeated for each pattern.

### Examples

#### 1. Multi-Directory Scan and Filtering

Audit code across two separate repositories (`/src/backend` and `/src/frontend`), excluding common build artifacts but ensuring all Java and C++ files are printed.

```bash
catlr /src/backend /src/frontend -e node_modules/ build/ -ip *.java *.cpp
```

#### 2. Global Exclusion (Ignoring Development Artifacts)

Completely ignore the `node_modules` directory and any files containing `build.log` in their name (using wildcards) in both the directory tree and the printed content.

```bash
catlr . -e node_modules/ *build.log*
```

#### 3. Selective Content Audit (Print Includes)

List the entire project structure, but only print the contents of C++ source files (`.cpp` and `.h`) using wildcards.

```bash
catlr . -ip *.cpp *.h
```

#### 2. Exception to Exclusion (The "Exclude all but one" pattern)

Exclude the entire `build/` directory from being processed, but explicitly **include** and print the critical file located at `build/main.js`.

```bash
catlr . -e build/ -i build/main.js
```

#### 3. Selective Content Audit (Print Includes)

List the entire project structure (`-l` scope is default *included*), but only print the contents of C++ source files (`.cpp` and `.h`) using wildcards.

```bash
catlr . -ip *.cpp -ip *.h
```

#### 4. Listing Cleanup (List Excludes)

Hide large configuration directories (`.git`, `.vscode`) from the directory tree, but still print the contents of any file that passes the default print filters.

```bash
catlr . -le .git/ -le .vscode/
```

#### 5. Mixed Inclusion Scope

Exclude from printing and listing the whole `node_modules` directory (`-e node_modules/`), but explicitly **include** the `node_modules/lucide_icons/` directory **into the tree listing only** (`-il`). The files inside `lucide_icons` will still not be printed because the overall printing scope is excluded.

```bash
catlr . -e node_modules/ -il node_modules/lucide_icons/
```

#### 6. Avoiding Redirection Errors (Automatic Safety)

If you redirect the output of catlr to a file, the program will automatically detect if it tries to read that same file during traversal, and skip it, preventing I/O loops and external tool errors (like the `bat` warning).

```bash
# catlr automatically skips writing to 'audit.txt' if it finds it.
catlr . > audit.txt
```

-----

## ⚠️ Important Note on Filtering

The filtering is performed via **case-sensitive matching** against the full path string.

  * **Direct Path Matching:** A pattern without wildcards (`*`) must match the path segment exactly. A trailing slash (`/`) is highly recommended to target directories recursively.
      * `-e modules` will **only** exclude the file named `modules` in the current directory.
      * `-e modules/` will exclude (recursively) the directory `modules/` and all its contents.
      * `-e src/models/user.js` targets that specific file path.
  * **Wildcard Matching:** Use the asterisk (`*`) for general substring or suffix/prefix matching.
      * `-e *modules*` will exclude every file or directory that contains `modules` in its name (e.g., `node_modules/`, `my_modules.txt`, `modules.cpp`).
      * `-ip *.log` targets all files **ending with** `.log` for printing.