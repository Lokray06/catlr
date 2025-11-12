catlr (Catalogue, List, and Review)
===================================

**catlr** is a powerful, cross-platform command-line utility designed to list directory structures and print file contents recursively, specifically tailored for code auditing and project reviews. It intelligently falls back to native C++ implementations if preferred external tools (bat, tree, etc.) are unavailable, ensuring functionality on any system (Linux, macOS, Windows).

✨ Features
----------

*   **Intelligent Tool Fallback:** Prioritizes system commands (bat, tree) if installed, but seamlessly switches to built-in C++ methods for file reading and tree generation if external tools are missing or configured otherwise.
    
*   **Granular Filtering:** Offers fine-grained control over which files and directories are **listed** (in the tree) and which are **printed** (content display), using powerful include/exclude patterns.
    
*   **Cross-Platform Configuration:** Use a simple configuration file (~/.config/catlr/catlr.conf) to specify preferred external tools.
    
*   **I/O Loop Protection:** Automatically detects and skips printing any file that is also the destination of the program's output (catlr ... > file.txt), preventing infinite recursion errors.
    

🚀 Installation (From Source)
-----------------------------

catlr is written in C++ and requires a C++17 compliant compiler (like g++ or clang++) due to its use of the library.

Save the source code as catlr.cpp and compile using one of the following commands:

Platform

Command

Notes

**Linux (g++)**

g++ -o catlr catlr.cpp -std=c++17 -lstdc++fs

Uses the GNU standard filesystem library.

**macOS (clang++)**

clang++ -o catlr catlr.cpp -std=c++17 -lc++fs

Uses the LLVM standard filesystem library.

**Windows (MSVC)**

cl.exe /std:c++17 /EHsc catlr.cpp

Compiles using the Visual Studio compiler.

After compilation, place the resulting executable (catlr or catlr.exe) in a directory listed in your system's $PATH.

⚙️ Configuration
----------------

catlr looks for a configuration file to determine which external tools to use for directory listing and file printing.

**Configuration File Path:** ~/.config/catlr/catlr.conf

### Setup Steps:

1.  mkdir -p ~/.config/catlr
    
2.  nano ~/.config/catlr/catlr.conf
    
3.  \# catlr Configuration File# If a command is not found or this file is empty, catlr uses its native C++ implementation.# Use 'lsd --tree' for directory listing (or 'tree')treePrintCommand=lsd --tree# Use 'bat' for file printing (or 'cat')filePrintCommand=bat
    

📖 Usage and Filtering
----------------------

The general syntax is: catlr \[directory\] \[options\] \[legacy\_patterns...\]

### Core Filtering Flags

Filtering is divided into **List** (tree output) and **Print** (file content output). The fundamental logic is that **Include always overrides Exclude**.

Flag

Scope

Description

**\-e**

**Global Exclude**

Excludes paths matching from **both** the tree listing and file printing.

**\-i**

**Global Include**

Includes paths matching for **both** listing and printing. **This overrides any -e exclusions.**

**\-le**

List Exclude

Excludes paths from the **tree listing only**.

**\-li**

List Include

Includes paths in the **tree listing only**.

**\-pe**

Print Exclude

Excludes files from **content printing only**.

**\-pi**

Print Include

Includes files for **content printing only**.

### Examples

#### 1\. Global Exclusion (Ignoring Development Artifacts)

Completely ignore the node\_modules directory and any files named build.log in both the directory tree and the printed content.

Plain textANTLR4BashCC#CSSCoffeeScriptCMakeDartDjangoDockerEJSErlangGitGoGraphQLGroovyHTMLJavaJavaScriptJSONJSXKotlinLaTeXLessLuaMakefileMarkdownMATLABMarkupObjective-CPerlPHPPowerShell.propertiesProtocol BuffersPythonRRubySass (Sass)Sass (Scss)SchemeSQLShellSwiftSVGTSXTypeScriptWebAssemblyYAMLXML`   catlr . -e node_modules -e build.log   `

#### 2\. Exception to Exclusion (The "Exclude all but one" pattern)

Exclude the entire build directory, but explicitly include and print the critical file located at build/main.js.

Plain textANTLR4BashCC#CSSCoffeeScriptCMakeDartDjangoDockerEJSErlangGitGoGraphQLGroovyHTMLJavaJavaScriptJSONJSXKotlinLaTeXLessLuaMakefileMarkdownMATLABMarkupObjective-CPerlPHPPowerShell.propertiesProtocol BuffersPythonRRubySass (Sass)Sass (Scss)SchemeSQLShellSwiftSVGTSXTypeScriptWebAssemblyYAMLXML`   catlr . -e build -i build/main.js   `

#### 3\. Content Audit (Print Includes)

List the entire project structure, but only print the contents of C++ source files (.cpp and .h).

Plain textANTLR4BashCC#CSSCoffeeScriptCMakeDartDjangoDockerEJSErlangGitGoGraphQLGroovyHTMLJavaJavaScriptJSONJSXKotlinLaTeXLessLuaMakefileMarkdownMATLABMarkupObjective-CPerlPHPPowerShell.propertiesProtocol BuffersPythonRRubySass (Sass)Sass (Scss)SchemeSQLShellSwiftSVGTSXTypeScriptWebAssemblyYAMLXML`   catlr . -pi .cpp -pi .h  # Note: You can also use the legacy format: catlr . .cpp .h   `

#### 4\. Listing Cleanup (List Excludes)

Hide large configuration directories (.git, .vscode) from the directory tree, but still print the contents of any file that passes the default filters.

Plain textANTLR4BashCC#CSSCoffeeScriptCMakeDartDjangoDockerEJSErlangGitGoGraphQLGroovyHTMLJavaJavaScriptJSONJSXKotlinLaTeXLessLuaMakefileMarkdownMATLABMarkupObjective-CPerlPHPPowerShell.propertiesProtocol BuffersPythonRRubySass (Sass)Sass (Scss)SchemeSQLShellSwiftSVGTSXTypeScriptWebAssemblyYAMLXML`   catlr . -le .git -le .vscode   `

#### 5\. Avoiding Redirection Errors (Automatic Safety)

If you redirect the output of catlr to a file, the program will automatically detect if it tries to read that same file during traversal, and skip it, preventing I/O loops and external tool errors (like the bat warning).

Plain textANTLR4BashCC#CSSCoffeeScriptCMakeDartDjangoDockerEJSErlangGitGoGraphQLGroovyHTMLJavaJavaScriptJSONJSXKotlinLaTeXLessLuaMakefileMarkdownMATLABMarkupObjective-CPerlPHPPowerShell.propertiesProtocol BuffersPythonRRubySass (Sass)Sass (Scss)SchemeSQLShellSwiftSVGTSXTypeScriptWebAssemblyYAMLXML`# catlr automatically skips writing to 'audit.txt' if it finds it.  catlr . > audit.txt` 

⚠️ Important Note on Filtering
------------------------------

The filtering is performed via **simple substring matching** against the full path string. This means:

*   `-e modules` will exclude `node_modules`, `my_modules`, and any file containing the word "modules".
    
*   For best results when targeting directories, use patterns that include the path separator (e.g., -e /node\_modules/ or -e node\_modules).