#include <algorithm>  // For std::sort, std::find_if, std::replace
#include <cstdlib>	  // For system() and getenv()
#include <filesystem> // For all path and directory operations (Requires C++17)
#include <fstream>	  // For std::ifstream (reading files)
#include <iostream>	  // For std::cout, std::cerr, std::endl
#include <map>		  // For std::map (config storage)
#include <sstream>	  // For std::stringstream
#include <stdexcept>  // For std::exception
#include <string>	  // For std::string
#include <vector>	  // For std::vector

// POSIX headers for checking stdout (I/O loop detection)
#include <sys/stat.h> // For struct stat, S_ISREG
#include <unistd.h>	  // For isatty, STDOUT_FILENO, fstat

// For Windows, this would require #include <io.h> and _isatty, _fstat, etc.

// Define the namespace for easy access
namespace fs = std::filesystem;

// --- Configuration Structs ---

/**
 * @brief Holds the configuration for which external commands to use.
 */
struct Config
{
	std::string tree_command;
	std::string file_command;

	// Set defaults
	Config() : tree_command("tree"), file_command("bat") {}
};

/**
 * @brief Holds the include/exclude filters for listing and printing.
 */
struct Filters
{
	std::vector<std::string> print_includes;
	std::vector<std::string> print_excludes;
	std::vector<std::string> list_includes;
	std::vector<std::string> list_excludes;
};

// --- Cross-Platform & Utility Functions ---

/**
 * @brief Trims whitespace from the beginning and end of a string.
 */
std::string trim(const std::string &s)
{
	auto start = s.begin();
	while (start != s.end() && std::isspace(*start))
	{
		start++;
	}
	auto end = s.end();
	do
	{
		end--;
	} while (std::distance(start, end) > 0 && std::isspace(*end));
	return std::string(start, end + 1);
}

/**
 * @brief Gets the path to the user's home directory (cross-platform).
 */
fs::path get_home_path()
{
#ifdef _WIN32
	const char *home_env = getenv("USERPROFILE");
#else
	const char *home_env = getenv("HOME");
#endif
	if (home_env == nullptr)
	{
		return fs::path(); // Return empty path
	}
	return fs::path(home_env);
}

/**
 * @brief Checks if a command-line tool is available in the system's PATH.
 */
bool command_exists(const std::string &command)
{
	if (command.empty())
	{
		return false;
	}
	// Split command from args (e.g., "lsd --tree" -> check "lsd")
	std::string main_command = command.substr(0, command.find(' '));
#ifdef _WIN32
	std::string check_cmd = "where " + main_command + " > NUL 2>&1";
#else
	std::string check_cmd = "command -v " + main_command + " > /dev/null 2>&1";
#endif
	return system(check_cmd.c_str()) == 0;
}

/**
 * @brief Parses the config file from ~/.config/catlr/catlr.conf.
 */
Config parse_config()
{
	Config config; // Start with defaults
	fs::path home = get_home_path();
	if (home.empty())
	{
		return config; // No home dir, return defaults
	}
	fs::path config_path = home / ".config" / "catlr" / "catlr.conf";
	std::ifstream config_file(config_path);
	if (!config_file.is_open())
	{
		return config; // No config file, return defaults
	}
	std::string line;
	while (std::getline(config_file, line))
	{
		if (line.empty() || line[0] == '#')
			continue;
		auto equals_pos = line.find('=');
		if (equals_pos == std::string::npos)
			continue;
		std::string key = trim(line.substr(0, equals_pos));
		std::string value = trim(line.substr(equals_pos + 1));
		if (key == "treePrintCommand")
			config.tree_command = value;
		else if (key == "filePrintCommand")
			config.file_command = value;
	}
	return config;
}

/**
 * @brief Implements the new pattern matching logic from README/TODO.
 * @param rel_path_str The path relative to the root, using '/' separators.
 * @param filename_str The final component (filename) of the path.
 * @param pattern The user-provided filter pattern.
 * @return true if the path matches the pattern.
 */
bool pattern_matches(const std::string &rel_path_str, const std::string &filename_str, std::string pattern)
{
	// Normalize pattern to use forward slashes, just like rel_path_str
	std::replace(pattern.begin(), pattern.end(), '\\', '/');

	// 1. Wildcard matching
	if (pattern.find('*') != std::string::npos)
	{
		if (pattern.front() == '*' && pattern.back() == '*')
		{ // *modules*
			return rel_path_str.find(pattern.substr(1, pattern.length() - 2)) != std::string::npos;
		}
		if (pattern.front() == '*')
		{ // *.cpp
			std::string suffix = pattern.substr(1);
			if (rel_path_str.length() < suffix.length())
				return false;
			return rel_path_str.compare(rel_path_str.length() - suffix.length(), suffix.length(), suffix) == 0;
		}
		if (pattern.back() == '*')
		{ // build*
			return rel_path_str.rfind(pattern.substr(0, pattern.length() - 1), 0) == 0;
		}
		// Fallback for other wildcards (e.g. *build.log*) -> treat as contains
		std::string processed_pattern;
		for (char c : pattern)
			if (c != '*')
				processed_pattern += c;
		if (processed_pattern.empty())
			return true; // Match "*"
		return rel_path_str.find(processed_pattern) != std::string::npos;
	}

	// 2. Direct Matching
	if (pattern.back() == '/')
	{
		// This is the bug fix.
		// Pattern is "build/"
		std::string dir_pattern = pattern;								// "build/"
		std::string dir_name = pattern.substr(0, pattern.length() - 1); // "build"

		// Match if rel_path is the directory itself ("build")
		// OR if rel_path starts with the directory pattern ("build/main.cpp")
		return rel_path_str == dir_name || rel_path_str.rfind(dir_pattern, 0) == 0;
	}

	if (pattern.find('/') == std::string::npos)
	{ // modules (no slash)
		return filename_str == pattern;
	}

	// Full path match: src/models/user.js
	return rel_path_str == pattern;
}

/**
 * @brief Checks if a path matches include/exclude filters.
 * @param path The file or directory path to check.
 * @param base_path The root directory the scan started from (for relative paths).
 * @param includes Vector of include patterns.
 * @param excludes Vector of exclude patterns.
 * @return true if the path should be shown, false if hidden.
 */
bool matches_filters(const fs::path &path, const fs::path &base_path, const std::vector<std::string> &includes, const std::vector<std::string> &excludes)
{
	std::string rel_path_str;
	std::string filename_str;
	try
	{
		// Use relative path for matching, as specified in README examples
		rel_path_str = fs::relative(path, base_path).string();
		filename_str = path.filename().string();
		// Normalize path separators for consistent matching
		std::replace(rel_path_str.begin(), rel_path_str.end(), '\\', '/');
	}
	catch (const std::exception &e)
	{
		return false; // Handle invalid path encoding or comparison
	}

	// 1. Check Includes (Priority 1)
	for (const auto &pattern : includes)
	{
		if (pattern_matches(rel_path_str, filename_str, pattern))
		{
			return true;
		}
	}

	// 2. Check Excludes (Priority 2)
	for (const auto &pattern : excludes)
	{
		if (pattern_matches(rel_path_str, filename_str, pattern))
		{
			return false;
		}
	}

	// 3. If 'includes' was not empty, we are in "include-only" mode.
	if (!includes.empty())
	{
		return false;
	}

	// 4. If 'includes' was empty, we are in "show-all-except-excludes" mode.
	return true;
}

// --- Native (Built-in) Implementations ---

/**
 * @brief NATIVE FALLBACK: Prints file contents using C++ streams.
 */
void print_file_native(const fs::path &path)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		std::cerr << "[Could not open file: " << path.string() << "]" << std::endl;
		return;
	}
	std::cout << file.rdbuf();
	file.close();
}

/**
 * @brief Helper for print_tree_native to recursively draw the tree.
 */
void print_tree_recursive(const fs::path &path, const fs::path &base_path, const std::string &prefix, const Filters &filters)
{
	try
	{
		std::vector<fs::directory_entry> entries;
		for (const auto &entry : fs::directory_iterator(path))
		{
			// Apply list filters *before* adding to the vector
			if (matches_filters(entry.path(), base_path, filters.list_includes, filters.list_excludes))
			{
				entries.push_back(entry);
			}
		}
		std::sort(entries.begin(), entries.end(),
				  [](const auto &a, const auto &b)
				  {
					  return a.path().filename() < b.path().filename();
				  });

		for (size_t i = 0; i < entries.size(); ++i)
		{
			const auto &entry = entries[i];
			bool is_last = (i == entries.size() - 1);

			std::cout << prefix;
			std::cout << (is_last ? "└── " : "├── ");
			std::cout << entry.path().filename().string();

			if (entry.is_directory())
			{
				std::cout << "/" << std::endl;
				std::string new_prefix = prefix + (is_last ? "    " : "│   ");
				print_tree_recursive(entry.path(), base_path, new_prefix, filters);
			}
			else
			{
				std::cout << std::endl;
			}
		}
	}
	catch (const std::exception &e)
	{
		// Silently ignore directories we can't read
	}
}

/**
 * @brief NATIVE FALLBACK: Prints a directory tree using C++, respecting filters.
 */
void print_tree_native(const fs::path &path, const Filters &filters)
{
	std::cout << path.filename().string() << "/" << std::endl;
	// The base_path for filtering is the path itself
	print_tree_recursive(path, path, "", filters);
}

// --- Main Program Logic ---

/**
 * @brief Displays the new usage information.
 */
void show_usage(const char *prog_name)
{
	std::cerr << "Usage: " << prog_name << " [directory_path...] [filter_rules...]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "Arguments:" << std::endl;
	std::cerr << "  directory_path...: One or more target directories (defaults to current)." << std::endl;
	std::cerr << std::endl;
	std::cerr << "Filtering Options (patterns can use wildcards like *.cpp or *build*):" << std::endl;
	std::cerr << "  -e,  --exclude <p...>: Exclude from BOTH list and print (e.g., -e node_modules/ build/)." << std::endl;
	std::cerr << "  -i,  --include <p...>: Include in BOTH list and print. Overrides excludes." << std::endl;
	std::cerr << "  -li, -il, --list-include <p...>: Only LIST paths matching pattern." << std::endl;
	std::cerr << "  -le, -el, --list-exclude <p...>: Exclude from LIST (tree view) only (e.g., -le .git/)." << std::endl;
	std::cerr << "  -pi, -ip, --print-include <p...>: Only PRINT files matching pattern (e.g., -pi *.cpp *.h)." << std::endl;
	std::cerr << "  -pe, -ep, --print-exclude <p...>: Exclude from PRINT only (e.g., -pe *.min.js)." << std::endl;
	std::cerr << "  -h,  --help            : Show this help message." << std::endl;
	std::cerr << std::endl;
	std::cerr << "Examples:" << std::endl;
	std::cerr << "  " << prog_name << "                        # List and print all in current dir" << std::endl;
	std::cerr << "  " << prog_name << " /src/backend /src/frontend -e node_modules/ -ip *.java *.cpp" << std::endl;
	std::cerr << "  " << prog_name << " -e build/ -i build/main.js # Exclude 'build' dir, but still show 'build/main.js'" << std::endl;
	std::cerr << "  " << prog_name << " -le .git/ -pe README.md  # Hide .git from tree, skip printing README" << std::endl;
}

int main(int argc, char *argv[])
{
	// --- 0. I/O Loop Detection Setup ---
	ino_t stdout_inode = 0;
	dev_t stdout_dev = 0;

#ifndef _WIN32 // isatty/fstat are POSIX
	if (!isatty(STDOUT_FILENO))
	{
		struct stat stdout_stat;
		if (fstat(STDOUT_FILENO, &stdout_stat) == 0)
		{
			if (S_ISREG(stdout_stat.st_mode))
			{
				stdout_inode = stdout_stat.st_ino;
				stdout_dev = stdout_stat.st_dev;
			}
		}
	}
#endif

	// --- 1. Argument Parsing ---
	std::vector<fs::path> target_paths;
	Filters filters;
	int first_flag_idx = argc;

	// Find first flag
	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "-h" || arg == "--help")
		{
			show_usage(argv[0]);
			return 0;
		}
		if (arg[0] == '-' && arg.length() > 1)
		{ // Found a flag
			first_flag_idx = i;
			break;
		}
	}

	// Collect paths (all args before the first flag)
	for (int i = 1; i < first_flag_idx; ++i)
	{
		target_paths.push_back(argv[i]);
	}
	if (target_paths.empty())
	{
		target_paths.push_back(".");
	}

	// Parse flags and patterns (from first flag index onwards)
	for (int i = first_flag_idx; i < argc; ++i)
	{
		std::string arg = argv[i];

		// --- Filter Flags (Multi-Pattern) ---
		if (arg == "-e" || arg == "--exclude")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				filters.list_excludes.push_back(argv[i]);
				filters.print_excludes.push_back(argv[i]);
			}
		}
		else if (arg == "-i" || arg == "--include")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				filters.list_includes.push_back(argv[i]);
				filters.print_includes.push_back(argv[i]);
			}
		}
		else if (arg == "-li" || arg == "--list-include" || arg == "-il")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				filters.list_includes.push_back(argv[i]);
			}
		}
		else if (arg == "-le" || arg == "--list-exclude" || arg == "-el")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				filters.list_excludes.push_back(argv[i]);
			}
		}
		else if (arg == "-pi" || arg == "--print-include" || arg == "-ip")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				filters.print_includes.push_back(argv[i]);
			}
		}
		else if (arg == "-pe" || arg == "--print-exclude" || arg == "-ep")
		{
			while (i + 1 < argc && argv[i + 1][0] != '-')
			{
				i++;
				filters.print_excludes.push_back(argv[i]);
			}
		}
		else if (arg[0] == '.')
		{
			// Backwards compatibility for: catlr . .txt .md
			filters.print_includes.push_back(arg);
		}
		else if (arg[0] == '-')
		{
			std::cerr << "Warning: Unknown flag '" << arg << "'. Ignoring." << std::endl;
		}
	}

	// --- 3. Load Config and Validate Tools ---
	Config config = parse_config();
	bool use_external_tree = command_exists(config.tree_command);
	bool use_configured_file_cmd = command_exists(config.file_command);
	bool use_cat = !use_configured_file_cmd && command_exists("cat");

	// --- 4. Loop through each target path ---
	for (const auto &path_entry : target_paths)
	{
		fs::path target_path;
		try
		{
			target_path = fs::canonical(path_entry);
		}
		catch (const fs::filesystem_error &e)
		{
			std::cerr << "Error: Could not resolve path '" << path_entry.string() << "'. " << e.what() << std::endl;
			continue; // Skip to next path
		}

		// --- 4a. Directory Tree Listing ---
		std::cout << "--- Directory Tree for: " << target_path.filename().string() << " ---" << std::endl;
		std::cout << "Located at: " << target_path.string() << std::endl
				  << std::endl;

		if (use_external_tree)
		{
			if (!filters.list_includes.empty() || !filters.list_excludes.empty())
			{
				std::cout << "Info: External 'tree' command does not support filters. Using built-in tree." << std::endl;
				print_tree_native(target_path, filters);
			}
			else
			{
				std::string tree_cmd = config.tree_command + " \"" + target_path.string() + "\"";
				system(tree_cmd.c_str());
			}
		}
		else
		{
			std::cout << "Info: '" << config.tree_command << "' not found. Using built-in tree implementation." << std::endl;
			print_tree_native(target_path, filters);
		}
		std::cout << std::endl;

		// --- 4b. Recursive File Content Listing ---
		std::cout << "--- File Contents (Recursive) for: " << target_path.filename().string() << " ---" << std::endl;

		try
		{
			auto it = fs::recursive_directory_iterator(target_path, fs::directory_options::skip_permission_denied);
			for (const auto &entry : it)
			{
				try
				{
					// 1. Check LIST exclusion (to skip recursion)
					if (entry.is_directory() && !matches_filters(entry.path(), target_path, filters.list_includes, filters.list_excludes))
					{
						it.disable_recursion_pending(); // C++17: Don't recurse
						continue;
					}

					if (!entry.is_regular_file())
					{
						continue;
					}

					fs::path current_path = entry.path();

					// 2. Check PRINT filtering
					if (matches_filters(current_path, target_path, filters.print_includes, filters.print_excludes))
					{
						fs::path relative_path = fs::relative(current_path, target_path);

// --- IO LOOP CHECK ---
#ifndef _WIN32
						if (stdout_inode != 0)
						{
							struct stat file_stat;
							if (stat(current_path.string().c_str(), &file_stat) == 0)
							{
								if (file_stat.st_ino == stdout_inode && file_stat.st_dev == stdout_dev)
								{
									std::cerr << "--- " << relative_path.string() << " ---" << std::endl;
									std::cerr << "[Warning: Skipping file to avoid I/O loop (file is program output)]" << std::endl;
									std::cout << std::endl;
									continue;
								}
							}
						}
#endif
						// --- END CHECK ---

						std::cout << "--- " << relative_path.string() << " ---" << std::endl;

						std::string cmd;
						if (use_configured_file_cmd)
						{
							cmd = config.file_command;
							if (config.file_command == "bat")
								cmd += " --paging=never --style=full";
							cmd += " \"" + current_path.string() + "\"";
						}
						else if (use_cat)
						{
							cmd = "cat \"" + current_path.string() + "\"";
						}

						if (use_configured_file_cmd || use_cat)
						{
							system(cmd.c_str());
						}
						else
						{
							print_file_native(current_path);
						}

						std::cout << std::endl; // Separator
					}
				}
				catch (const fs::filesystem_error &e)
				{
					// Suppress errors for single file
				}
			}
		}
		catch (const std::exception &e)
		{
			std::cerr << "Error during file traversal: " << e.what() << std::endl;
		}
	} // End loop over target_paths

	std::cout << "--- End of Listing ---" << std::endl;
	return 0;
}