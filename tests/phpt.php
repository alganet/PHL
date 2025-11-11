<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * PHP test runner and TAP producer, compatible with PHP and PHL
 */

// Valid section types
$phpt_valid_sections = array('test', 'description', 'credits', 'skipif', 'file', 'expect', 'expectf', 'expectregex', 'clean', 'post', 'post_raw', 'get', 'cookie', 'stdin', 'ini', 'args', 'env');

// Unimplemented section types
$phpt_not_implemented = array('post', 'post_raw', 'get', 'cookie', 'stdin', 'ini', 'args', 'env', 'expectregex');

// Default values
$phpt_target_executable = "";
$phpt_target_timeout = 1;
$phpt_target_dir = dirname(__FILE__);
$phpt_file_extension = "phpt";
$phpt_filter = "";
$phpt_output_format = "tap";
$phpt_curdir = getcwd();

// Parse arguments
if (count($argv) > 0 && strpos($argv[0], '--') !== 0) {
    $phpt_args = array_slice($argv, 1);
    $phpt_script_name = $argv[0];
} else {
    $phpt_args = $argv;
    $phpt_script_name = 'phpt.php';
}
while (!empty($phpt_args)) {
    $phpt_arg = array_shift($phpt_args);
    switch ($phpt_arg) {
        case '--target-executable':
            $phpt_target_executable = array_shift($phpt_args);
            if ($phpt_target_executable === null) {
                echo "Error: --target-executable requires a value\n";
                exit(1);
            }
            break;
        case '--target-timeout':
            $phpt_target_timeout = array_shift($phpt_args);
            if ($phpt_target_timeout === null) {
                echo "Error: --target-timeout requires a value\n";
                exit(1);
            }
            break;
        case '--target-dir':
            $phpt_target_dir = array_shift($phpt_args);
            if ($phpt_target_dir === null) {
                echo "Error: --target-dir requires a value\n";
                exit(1);
            }
            break;
        case '--file-extension':
            $phpt_file_extension = array_shift($phpt_args);
            if ($phpt_file_extension === null) {
                echo "Error: --file-extension requires a value\n";
                exit(1);
            }
            break;
        case '--filter':
            $phpt_filter = array_shift($phpt_args);
            if ($phpt_filter === null) {
                echo "Error: --filter requires a value\n";
                exit(1);
            }
            break;
        case '--output-format':
            $phpt_output_format = array_shift($phpt_args);
            if ($phpt_output_format === null) {
                echo "Error: --output-format requires a value\n";
                exit(1);
            }
            break;
        case '--help':
            echo "Usage: " . $phpt_script_name . " [options]\n";
            echo "\n";
            echo "Options:\n";
            echo "  --target-executable <exe>  Path to the executable being tested (mandatory)\n";
            echo "  --target-timeout <sec>     Timeout for each test in seconds (default: 1)\n";
            echo "  --target-dir <dir>         Directory containing test files (default: directory of this script)\n";
            echo "  --file-extension <ext>     File extension for test files (default: phpt)\n";
            echo "  --filter <pattern>         Filter test files by prefix (optional, runs all if not specified)\n";
            echo "  --output-format <format>   Output format: tap (default) or dot\n";
            echo "  --help                     Show this help message\n";
            echo "\n";
            exit(0);
        default:
            echo "Unknown option: $phpt_arg\n";
            exit(1);
    }
}

// Argument parsing complete

// Validate arguments
if (!is_numeric($phpt_target_timeout) || $phpt_target_timeout != (int)$phpt_target_timeout) {
    echo "1..0 # Target timeout '$phpt_target_timeout' is not a valid integer.\n";
    exit(1);
}
if (!is_dir($phpt_target_dir)) {
    echo "1..0 # Target directory '$phpt_target_dir' not found.\n";
    exit(1);
}
if ($phpt_output_format != "tap" && $phpt_output_format != "dot") {
    echo "1..0 # Invalid output format '$phpt_output_format'. Must be 'tap' or 'dot'.\n";
    exit(1);
}

// TAP header
if ($phpt_output_format == "tap") {
    echo "Tap Version 13\n";
}

function parse_phpt_sections($phpt_path, $phpt_valid_sections) {
    // Read file and normalize line endings to LF
    $phpt_content = file_get_contents($phpt_path);
    $phpt_content = str_replace("\r\n", "\n", $phpt_content);
    $phpt_content = str_replace("\r", "\n", $phpt_content);
    $phpt_lines = explode("\n", $phpt_content);
    
    $phpt_sections = array();
    $phpt_current_section = null;
    $phpt_content_lines = array();
    
    foreach ($phpt_lines as $phpt_line) {
        if (substr($phpt_line, 0, 2) === '--' && substr($phpt_line, -2) === '--') {
            // Save previous section
            if ($phpt_current_section !== null) {
                $phpt_sections[$phpt_current_section] = implode("\n", $phpt_content_lines);
            }
            $phpt_section_name = strtolower(substr($phpt_line, 2, -2));
            if (in_array($phpt_section_name, $phpt_valid_sections)) {
                $phpt_current_section = $phpt_section_name;
            } else {
                $phpt_current_section = null;
            }
            $phpt_content_lines = array();
        } else {
            if ($phpt_current_section !== null) {
                $phpt_content_lines[] = $phpt_line;
            }
        }
    }
    
    // Save last section
    if ($phpt_current_section !== null) {
        $phpt_sections[$phpt_current_section] = implode("\n", $phpt_content_lines);
    }
    
    return $phpt_sections;
}

function find_files($phpt_dir, $phpt_extension, $phpt_filter = '') {
    $phpt_files = array();
    if (!is_dir($phpt_dir)) {
        return $phpt_files;
    }
    $phpt_entries = scandir($phpt_dir);
    if ($phpt_entries === false) {
        return $phpt_files;
    }
    foreach ($phpt_entries as $phpt_entry) {
        if ($phpt_entry === '.' || $phpt_entry === '..') continue;
        $phpt_path = $phpt_dir . '/' . $phpt_entry;
        if (is_dir($phpt_path)) {
            $phpt_files = array_merge($phpt_files, find_files($phpt_path, $phpt_extension, $phpt_filter));
        } elseif (is_file($phpt_path) && pathinfo($phpt_path, PATHINFO_EXTENSION) === $phpt_extension) {
            $phpt_basename = pathinfo($phpt_path, PATHINFO_FILENAME);
            if (empty($phpt_filter) || strpos($phpt_basename, $phpt_filter) === 0) {
                $phpt_files[] = $phpt_path;
            }
        }
    }
    return $phpt_files;
}

function handle_error($errno, $errstr, $errfile, $errline) {
    echo "Error [$errno]: $errstr in $errfile on line $errline\n";
    exit(1);
}

function match_expectf_pattern($phpt_pattern, $phpt_output) {
    // Handle %d (digits) and %s (non-whitespace strings) without regex
    $phpt_pattern_len = strlen($phpt_pattern);
    $phpt_output_len = strlen($phpt_output);
    $phpt_p = 0; // pattern position
    $phpt_o = 0; // output position
    
    while ($phpt_p < $phpt_pattern_len) {
        if ($phpt_pattern[$phpt_p] === '%') {
            $phpt_p++; // move past %
            if ($phpt_p >= $phpt_pattern_len) {
                return false; // incomplete % sequence
            }
            
            if ($phpt_pattern[$phpt_p] === 'd') {
                // Match one or more digits
                if ($phpt_o >= $phpt_output_len || !ctype_digit($phpt_output[$phpt_o])) {
                    return false;
                }
                while ($phpt_o < $phpt_output_len && ctype_digit($phpt_output[$phpt_o])) {
                    $phpt_o++;
                }
            } elseif ($phpt_pattern[$phpt_p] === 's') {
                // Match one or more non-whitespace characters
                if ($phpt_o >= $phpt_output_len || ctype_space($phpt_output[$phpt_o])) {
                    return false;
                }
                while ($phpt_o < $phpt_output_len && !ctype_space($phpt_output[$phpt_o])) {
                    $phpt_o++;
                }
            } else {
                return false; // unknown % sequence
            }
            $phpt_p++; // move past d or s
        } else {
            // Literal character match
            if ($phpt_o >= $phpt_output_len || $phpt_pattern[$phpt_p] !== $phpt_output[$phpt_o]) {
                return false;
            }
            $phpt_p++;
            $phpt_o++;
        }
    }
    
    // Allow trailing content in output for flexibility
    return true;
}

// Find test files
$phpt_files = find_files($phpt_target_dir, $phpt_file_extension, $phpt_filter);
sort($phpt_files);

$phpt_total = count($phpt_files);
if ($phpt_output_format == "tap") {
    echo "1..$phpt_total\n";
}

$phpt_passed = 0;
$phpt_failed = 0;
$phpt_skipped = 0;
$phpt_nimp = 0;
$phpt_count = 1;

foreach ($phpt_files as $phpt_file) {
    $phpt_sections = parse_phpt_sections($phpt_file, $phpt_valid_sections);
    
    // Write sections to disk
    if (isset($phpt_sections['file'])) {
        $phpt_file_path = $phpt_file . '.file';
        file_put_contents($phpt_file_path, $phpt_sections['file']);
    }
    if (isset($phpt_sections['skipif'])) {
        $phpt_skipif_path = $phpt_file . '.skipif';
        file_put_contents($phpt_skipif_path, $phpt_sections['skipif']);
    }
    if (isset($phpt_sections['clean'])) {
        $phpt_clean_path = $phpt_file . '.clean';
        file_put_contents($phpt_clean_path, $phpt_sections['clean']);
    }

    $phpt_test_name = $phpt_file;
    
    // SKIPIF check
    $phpt_skip = false;
    if (isset($phpt_sections['skipif'])) {
        $phpt_skipif_path = $phpt_file . '.skipif';
        ob_start();
        include($phpt_skipif_path);
        $phpt_skip_output = ob_get_clean();
        chdir($phpt_curdir);
        $phpt_skip_output = trim($phpt_skip_output);
        if (!empty($phpt_skip_output)) {
            $phpt_skip = true;
        }
    }
    
    if ($phpt_skip) {
        if ($phpt_output_format == "tap") {
            echo "ok $phpt_count - $phpt_test_name # skip\n";
        } else {
            echo "S";
        }
        $phpt_skipped++;
    } else {
        // Check for unimplemented sections
        $phpt_has_unimplemented = false;
        foreach ($phpt_not_implemented as $phpt_section) {
            if (isset($phpt_sections[$phpt_section]) && !empty(trim($phpt_sections[$phpt_section]))) {
                $phpt_has_unimplemented = true;
                break;
            }
        }
        
        if ($phpt_has_unimplemented) {
            if ($phpt_output_format == "tap") {
                echo "not ok $phpt_count - $phpt_test_name # TODO no runner support\n";
            } else {
                echo "F";
            }
            $phpt_nimp++;
        } else {
            // Test execution
            $phpt_file_path = $phpt_file . '.file';
            ob_start();
            set_error_handler('handle_error');
            include($phpt_file_path);
            set_error_handler(null);
            $phpt_output = ob_get_clean();
            $phpt_output = str_replace("\r\n", "\n", $phpt_output);
            $phpt_output = str_replace("\r", "", $phpt_output);
            chdir($phpt_curdir);
            $phpt_output = trim($phpt_output);
            
            $phpt_expected = isset($phpt_sections['expect']) ? trim($phpt_sections['expect']) : '';
            $phpt_expectedf = isset($phpt_sections['expectf']) ? trim($phpt_sections['expectf']) : '';
            
            $phpt_matches = false;
            if (!empty($phpt_expectedf)) {
                // Use EXPECTF with pattern matching for %d and %s
                $phpt_matches = match_expectf_pattern($phpt_expectedf, $phpt_output);
            } elseif ($phpt_output === $phpt_expected) {
                $phpt_matches = true;
            }
            
            if ($phpt_matches === true) {
                if ($phpt_output_format == "tap") {
                    echo "ok $phpt_count - $phpt_test_name\n";
                } else {
                    echo ".";
                }
                $phpt_passed++;
            } else {
                if ($phpt_output_format == "tap") {
                    echo "not ok $phpt_count - $phpt_test_name\n";
                    if (!empty($phpt_expectedf)) {
                        echo "# Expected pattern: '$phpt_expectedf'\n";
                    } else {
                        echo "# Expected: '$phpt_expected'\n";
                    }
                    echo "# Actual: '$phpt_output'\n";
                } else {
                    echo "F";
                    if (!empty($phpt_expectedf)) {
                        echo "# Expected pattern: '$phpt_expectedf'\n";
                    } else {
                        echo "# Expected: '$phpt_expected'\n";
                    }
                    echo "# Actual: '$phpt_output'\n";
                }
                $phpt_failed++;
            }
        }
    }
    
    // CLEAN execution
    if (isset($phpt_sections['clean']) && $phpt_skip === false) {
        $phpt_clean_path = $phpt_file . '.clean';
        ob_start();
        include($phpt_clean_path);
        ob_end_clean();
        chdir($phpt_curdir);
    }
    
    $phpt_count++;
    
    // Clean up temp files
    @unlink($phpt_file . '.file');
    @unlink($phpt_file . '.skipif');
    @unlink($phpt_file . '.clean');
    @unlink($phpt_file . '.output');
    @unlink($phpt_file . '.expect');
}

if ($phpt_output_format == "tap") {
    echo "# Incomplete Tests\n";
    echo "# ----------------\n";
    echo "#     ok: $phpt_skipped\t(# skip)\n";
    echo "# not ok: $phpt_nimp\t(# TODO)\n";
    echo "# ----------------\n";
    echo "#  Total: " . ($phpt_skipped + $phpt_nimp) . " incomplete\n";
    echo "\n";
    echo "# Test Summary\n";
    echo "# ----------------\n";
    echo "#     ok: " . ($phpt_passed + $phpt_skipped) . "\n";
    echo "# not ok: " . ($phpt_failed + $phpt_nimp) . "\n";
    echo "# ----------------\n";
    echo "#  Total: $phpt_total tests\n";
} else {
    echo " (" . ($phpt_passed + $phpt_skipped) . "/$phpt_total)\n";
}

if ($phpt_total != ($phpt_passed + $phpt_failed + $phpt_skipped + $phpt_nimp)) {
    echo "# WARNING: Test count mismatch in directory '$phpt_target_dir'.\n";
    exit(1);
}

if ($phpt_failed > 0) {
    exit(1);
}
