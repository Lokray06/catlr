#include <algorithm>  // For std::sort, std::find_if
#include <cstdlib>    // For system() and getenv()
#include <filesystem> // For all path and directory operations (Requires C++17)
#include <fstream>    // For std::ifstream (reading files)
#include <iostream>   // For std::cout, std::cerr, std::endl
#include <map>        // For std::map (config storage)
#include <sstream>    // For std::stringstream
#include <stdexcept>  // For std::exception
#include <string>     // For std::string
#include <vector>     // For std::vector

// POSIX headers for checking stdout (I/O loop detection)
#include <sys/stat.h> // For struct stat, S_ISREG
#include <unistd.h>   // For isatty, STDOUT_FILENO, fstat

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
	while(start != s.end() && std::isspace(*start))
	{
		start++;
	}
	auto end = s.end();
	do
	{
		end--;
	} while(std::distance(start, end) > 0 && std::isspace(*end));
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
	if(home_env == nullptr)
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
	if(command.empty())
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
	if(home.empty())
	{
		return config; // No home dir, return defaults
	}
	fs::path config_path = home / ".config" / "catlr" / "catlr.conf";
	std::ifstream config_file(config_path);
	if(!config_file.is_open())
	{
		return config; // No config file, return defaults
	}
	std::string line;
	while(std::getline(config_file, line))
	{
		if(line.empty() || line[0] == '#')
			continue;
		auto equals_pos = line.find('=');
		if(equals_pos == std::string::npos)
			continue;
		std::string key = trim(line.substr(0, equals_pos));
		std::string value = trim(line.substr(equals_pos + 1));
		if(key == "treePrintCommand")
			config.tree_command = value;
		else if(key == "filePrintCommand")
			config.file_command = value;
	}
	return config;
}

/**
 * @brief Checks if a path matches include/exclude filters.
 * @param path The file or directory path to check.
 * @param includes Vector of include patterns.
 * @param excludes Vector of exclude patterns.
 * @return true if the path should be shown, false if hidden.
 */
bool matches_filters(const fs::path &path, const std::vector<std::string> &includes, const std::vector<std::string> &excludes)
{
	std::string path_str;
	try
	{
		path_str = path.string(); // Use string for simple substring matching
	}
	catch(const std::exception &e)
	{
		return false; // Handle invalid path encoding
	}

	// 1. Check Includes (Priority 1)
	// If any include pattern matches, we *must* show this path.
	for(const auto &pattern : includes)
	{
		if(path_str.find(pattern) != std::string::npos)
		{
			return true;
		}
	}

	// 2. Check Excludes (Priority 2)
	// If no includes matched, check excludes. If any exclude matches, hide it.
	for(const auto &pattern : excludes)
	{
		if(path_str.find(pattern) != std::string::npos)
		{
			return false;
		}
	}

	// 3. No includes or excludes matched.
	// If the 'includes' list was not empty, it means we are in "include-only"
	// mode, and since this path didn't match an include, we hide it.
	if(!includes.empty())
	{
		return false;
	}

	// 4. If 'includes' was empty, we are in "show-all-except-excludes" mode.
	// Since it didn't match an exclude, we show it.
	return true;
}

// --- Native (Built-in) Implementations ---

/**
 * @brief NATIVE FALLBACK: Prints file contents using C++ streams.
 */
void print_file_native(const fs::path &path)
{
	std::ifstream file(path);
	if(!file.is_open())
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
void print_tree_recursive(const fs::path &path, const std::string &prefix, const Filters &filters)
{
	try
	{
		std::vector<fs::directory_entry> entries;
		for(const auto &entry : fs::directory_iterator(path))
		{
			// Apply list filters *before* adding to the vector
			if(matches_filters(entry.path(), filters.list_includes, filters.list_excludes))
			{
				entries.push_back(entry);
			}
		}
		std::sort(entries.begin(), entries.end(),
		          [](const auto &a, const auto &b)
		          {
			          return a.path().filename() < b.path().filename();
		          });

		for(size_t i = 0; i < entries.size(); ++i)
		{
			const auto &entry = entries[i];
			bool is_last = (i == entries.size() - 1);

			std::cout << prefix;
			std::cout << (is_last ? "└── " : "├── ");
			std::cout << entry.path().filename().string();

			if(entry.is_directory())
			{
				std::cout << "/" << std::endl;
				std::string new_prefix = prefix + (is_last ? "    " : "│   ");
				print_tree_recursive(entry.path(), new_prefix, filters);
			}
			else
			{
				std::cout << std::endl;
			}
		}
	}
	catch(const std::exception &e)
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
	print_tree_recursive(path, "", filters);
}

// --- Main Program Logic ---

/**
 * @brief Displays the new usage information.
 */
void show_usage(const char *prog_name)
{
	std::cerr << "Usage: " << prog_name << " [directory] [options] [patterns...]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "Arguments:" << std::endl;
	std::cerr << "  directory        : The target directory (defaults to current)." << std::endl;
	std::cerr << "  patterns...      : Legacy file extensions (e.g., .txt .md)." << std::endl;
	std::cerr << "                     Treated as print-include filters." << std::endl;
	std::cerr << std::endl;
	std::cerr << "Filtering Options:" << std::endl;
	std::cerr << "  -e,  --exclude <p>   : Exclude from BOTH list and print (e.g., -e node_modules)." << std::endl;
	std::cerr << "  -i,  --include <p>   : Include in BOTH list and print. Overrides excludes." << std::endl;
	std::cerr << "  -li, --list-include <p>: Only LIST paths matching pattern." << std::endl;
	std::cerr << "  -le, --list-exclude <p>: Exclude from LIST (tree view) only." << std::endl;
	std::cerr << "  -pi, --print-include <p>: Only PRINT files matching pattern (e.g., -pi .cpp)." << std::endl;
	std::cerr << "  -pe, --print-exclude <p>: Exclude from PRINT only (e.g., -pe .min.js)." << std::endl;
	std::cerr << "  -h,  --help            : Show this help message." << std::endl;
	std::cerr << std::endl;
	std::cerr << "Examples:" << std::endl;
	std::cerr << "  " << prog_name << "                        # List and print all" << std::endl;
	std::cerr << "  " << prog_name << " . .txt .md              # List all, print only .txt and .md" << std::endl;
	std::cerr << "  " << prog_name << " -e .git -e node_modules # Ignore .git and node_modules entirely" << std::endl;
	std::cerr << "  " << prog_name << " -e build -i build/main.js # Exclude 'build' dir, but still show 'build/main.js'" << std::endl;
	std::cerr << "  " << prog_name << " -pi .cpp -pi .h         # List all, print only .cpp and .h files" << std::endl;
	std::cerr << "  " << prog_name << " -le .git -pe README.md  # Hide .git from tree, skip printing README" << std::endl;
}

int main(int argc, char *argv[])
{
	// --- 0. I/O Loop Detection Setup ---
	// Check if stdout is a redirected file to prevent I/O loops
	ino_t stdout_inode = 0;
	dev_t stdout_dev = 0;

// isatty() checks if stdout is a terminal.
// If it's NOT a terminal, it's redirected (to a file or pipe).
#ifndef _WIN32 // isatty/fstat are POSIX
	if(!isatty(STDOUT_FILENO))
	{
		struct stat stdout_stat;
		// fstat() gets info about the open file (stdout).
		if(fstat(STDOUT_FILENO, &stdout_stat) == 0)
		{
			// Check if it's a "regular file" (e.g., not a pipe)
			if(S_ISREG(stdout_stat.st_mode))
			{
				stdout_inode = stdout_stat.st_ino;
				stdout_dev = stdout_stat.st_dev;
			}
		}
	}
#endif

	// --- 1. Argument Parsing ---
	fs::path target_path = ".";
	Filters filters;
	int arg_index = 1;

	// Check if first arg is a directory
	if(argc > 1 && argv[1][0] != '-' && fs::is_directory(argv[1]))
	{
		target_path = argv[1];
		arg_index = 2;
	}
	else
	{
		target_path = ".";
		arg_index = 1;
	}

	// Parse all remaining arguments
	for(int i = arg_index; i < argc; ++i)
	{
		std::string arg = argv[i];
		if(arg == "-h" || arg == "--help")
		{
			show_usage(argv[0]);
			return 0;
		}
		// --- Filter Flags ---
		// These flags expect another argument
		if(i + 1 >= argc)
		{
			// Handle legacy patterns at the end
			if(arg[0] == '.')
			{
				filters.print_includes.push_back(arg);
			}
			continue; // Skip if a flag is at the very end
		}

		if(arg == "-e" || arg == "--exclude")
		{
			filters.list_excludes.push_back(argv[i + 1]);
			filters.print_excludes.push_back(argv[i + 1]);
			i++; // Consume pattern
		}
		else if(arg == "-i" || arg == "--include")
		{
			filters.list_includes.push_back(argv[i + 1]);
			filters.print_includes.push_back(argv[i + 1]);
			i++; // Consume pattern
		}
		else if(arg == "-li" || arg == "--list-include")
		{
			filters.list_includes.push_back(argv[i + 1]);
			i++;
		}
		else if(arg == "-le" || arg == "--list-exclude")
		{
			filters.list_excludes.push_back(argv[i + 1]);
			i++;
		}
		else if(arg == "-pi" || arg == "--print-include")
		{
			filters.print_includes.push_back(argv[i + 1]);
			i++;
		}
		else if(arg == "-pe" || arg == "--print-exclude")
		{
			filters.print_excludes.push_back(argv[i + 1]);
			i++;
		}
		else if(arg[0] == '.')
		{
			// Backwards compatibility for: catlr . .txt .md
			filters.print_includes.push_back(arg);
		}
		else if(arg[0] == '-')
		{
			std::cerr << "Warning: Unknown flag '" << arg << "'. Ignoring." << std::endl;
		}
	}

	// --- 2. Path Resolution ---
	try
	{
		target_path = fs::canonical(target_path);
	}
	catch(const fs::filesystem_error &e)
	{
		std::cerr << "Error: Could not resolve path. " << e.what() << std::endl;
		return 1;
	}

	// --- 3. Load Config and Validate Tools ---
	Config config = parse_config();
	bool use_external_tree = command_exists(config.tree_command);
	bool use_configured_file_cmd = command_exists(config.file_command);
	bool use_cat = !use_configured_file_cmd && command_exists("cat");

	// --- 4. Directory Tree Listing ---
	std::cout << "--- Directory Tree for: " << target_path.filename().string() << " ---" << std::endl;
	std::cout << "Located at: " << target_path.string() << std::endl
	          << std::endl;

	if(use_external_tree)
	{
		// We can't easily filter the external 'tree' command, so we add a note
		if(!filters.list_includes.empty() || !filters.list_excludes.empty())
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

	// --- 5. Recursive File Content Listing ---
	std::cout << "--- File Contents (Recursive) ---" << std::endl;

	try
	{
		// Manually control iterator to skip excluded directories
		auto it = fs::recursive_directory_iterator(target_path, fs::directory_options::skip_permission_denied);
		for(const auto &entry : it)
		{
			try
			{
				// 1. Check LIST exclusion (to skip recursion)
				if(entry.is_directory() && !matches_filters(entry.path(), filters.list_includes, filters.list_excludes))
				{
					it.disable_recursion_pending(); // C++17: Don't recurse into this directory
					continue;
				}

				if(!entry.is_regular_file())
				{
					continue;
				}

				fs::path current_path = entry.path();

				// 2. Check PRINT filtering
				if(matches_filters(current_path, filters.print_includes, filters.print_excludes))
				{
					fs::path relative_path = fs::relative(current_path, target_path);

// --- NEW IO LOOP CHECK ---
// Check if stdout_inode was set (meaning stdout is a file)
#ifndef _WIN32
					if(stdout_inode != 0)
					{
						struct stat file_stat;
						// Get stats for the file we're *about* to print
						if(stat(current_path.string().c_str(), &file_stat) == 0)
						{
							// If its inode and device match stdout, it's the same file.
							if(file_stat.st_ino == stdout_inode && file_stat.st_dev == stdout_dev)
							{
								std::cerr << "--- " << relative_path.string() << " ---" << std::endl;
								std::cerr << "[Warning: Skipping file to avoid I/O loop (file is program output)]" << std::endl;
								std::cout << std::endl; // Add separator
								continue;               // Skip to next file
							}
						}
					}
#endif
					// --- END NEW CHECK ---

					std::cout << "--- " << relative_path.string() << " ---" << std::endl;

					std::string cmd;
					if(use_configured_file_cmd)
					{
						cmd = config.file_command;
						if(config.file_command == "bat")
							cmd += " --paging=never --style=full";
						cmd += " \"" + current_path.string() + "\"";
					}
					else if(use_cat)
					{
						cmd = "cat \"" + current_path.string() + "\"";
					}

					if(use_configured_file_cmd || use_cat)
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
			catch(const fs::filesystem_error &e)
			{
				// Suppress errors for single file (e.g., broken symlink, permissions)
			}
		}
	}
	catch(const std::exception &e)
	{
		std::cerr << "Error during file traversal: " << e.what() << std::endl;
	}

	std::cout << "--- End of Listing ---" << std::endl;
	return 0;
}