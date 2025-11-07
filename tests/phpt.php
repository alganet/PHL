<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * PHP test runner and TAP producer, compatible with PHP and PHL
 */

// Valid section types
$valid_sections = array('test', 'description', 'credits', 'skipif', 'file', 'expect', 'expectf', 'expectregex', 'clean', 'post', 'post_raw', 'get', 'cookie', 'stdin', 'ini', 'args', 'env');

// Unimplemented section types
$not_implemented = array('post', 'post_raw', 'get', 'cookie', 'stdin', 'ini', 'args', 'env', 'expectf', 'expectregex');

// Default values
$target_executable = "";
$target_timeout = 1;
$target_dir = dirname(__FILE__);
$file_extension = "phpt";
$filter = "";
$output_format = "tap";

// Parse arguments
if (count($argv) > 0 && strpos($argv[0], '--') !== 0) {
    $args = array_slice($argv, 1);
    $script_name = $argv[0];
} else {
    $args = $argv;
    $script_name = 'phpt.php';
}
while (!empty($args)) {
    $arg = array_shift($args);
    switch ($arg) {
        case '--target-executable':
            $target_executable = array_shift($args);
            if ($target_executable === null) {
                echo "Error: --target-executable requires a value\n";
                exit(1);
            }
            break;
        case '--target-timeout':
            $target_timeout = array_shift($args);
            if ($target_timeout === null) {
                echo "Error: --target-timeout requires a value\n";
                exit(1);
            }
            break;
        case '--target-dir':
            $target_dir = array_shift($args);
            if ($target_dir === null) {
                echo "Error: --target-dir requires a value\n";
                exit(1);
            }
            break;
        case '--file-extension':
            $file_extension = array_shift($args);
            if ($file_extension === null) {
                echo "Error: --file-extension requires a value\n";
                exit(1);
            }
            break;
        case '--filter':
            $filter = array_shift($args);
            if ($filter === null) {
                echo "Error: --filter requires a value\n";
                exit(1);
            }
            break;
        case '--output-format':
            $output_format = array_shift($args);
            if ($output_format === null) {
                echo "Error: --output-format requires a value\n";
                exit(1);
            }
            break;
        case '--help':
            echo "Usage: " . $script_name . " [options]\n";
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
            echo "Unknown option: $arg\n";
            exit(1);
    }
}

// Argument parsing complete

// Validate arguments
if (!is_numeric($target_timeout) || $target_timeout != (int)$target_timeout) {
    echo "1..0 # Target timeout '$target_timeout' is not a valid integer.\n";
    exit(1);
}
if (!is_dir($target_dir)) {
    echo "1..0 # Target directory '$target_dir' not found.\n";
    exit(1);
}
if ($output_format != "tap" && $output_format != "dot") {
    echo "1..0 # Invalid output format '$output_format'. Must be 'tap' or 'dot'.\n";
    exit(1);
}

// TAP header
if ($output_format == "tap") {
    echo "Tap Version 13\n";
}

function parse_phpt_sections($path, $valid_sections) {
    // Read file and normalize line endings to LF
    $content = file_get_contents($path);
    $content = str_replace("\r\n", "\n", $content);
    $content = str_replace("\r", "\n", $content);
    $lines = explode("\n", $content);
    
    $sections = array();
    $current_section = null;
    $content_lines = array();
    
    foreach ($lines as $line) {
        if (substr($line, 0, 2) === '--' && substr($line, -2) === '--') {
            // Save previous section
            if ($current_section !== null) {
                $sections[$current_section] = implode("\n", $content_lines);
            }
            $section_name = strtolower(substr($line, 2, -2));
            if (in_array($section_name, $valid_sections)) {
                $current_section = $section_name;
            } else {
                $current_section = null;
            }
            $content_lines = array();
        } else {
            if ($current_section !== null) {
                $content_lines[] = $line;
            }
        }
    }
    
    // Save last section
    if ($current_section !== null) {
        $sections[$current_section] = implode("\n", $content_lines);
    }
    
    return $sections;
}

function find_files($dir, $extension, $filter = '') {
    $files = array();
    if (!is_dir($dir)) {
        return $files;
    }
    $handle = opendir($dir);
    while (($entry = readdir($handle)) !== false) {
        if ($entry == '.' || $entry == '..') continue;
        $path = $dir . '/' . $entry;
        if (is_dir($path)) {
            $files = array_merge($files, find_files($path, $extension, $filter));
        } elseif (is_file($path) && pathinfo($path, PATHINFO_EXTENSION) === $extension) {
            $basename = pathinfo($path, PATHINFO_FILENAME);
            if (empty($filter) || strpos($basename, $filter) === 0) {
                $files[] = $path;
            }
        }
    }
    closedir($handle);
    return $files;
}

// Find test files
$files = find_files($target_dir, $file_extension, $filter);
sort($files);

$total = count($files);
if ($output_format == "tap") {
    echo "1..$total\n";
}

$passed = 0;
$failed = 0;
$skipped = 0;
$nimp = 0;
$count = 1;

foreach ($files as $file) {
    $sections = parse_phpt_sections($file, $valid_sections);
    
    // Write sections to disk
    if (isset($sections['file'])) {
        $file_path = $file . '.file';
        file_put_contents($file_path, $sections['file']);
    }
    if (isset($sections['skipif'])) {
        $skipif_path = $file . '.skipif';
        file_put_contents($skipif_path, $sections['skipif']);
    }
    if (isset($sections['clean'])) {
        $clean_path = $file . '.clean';
        file_put_contents($clean_path, $sections['clean']);
    }

    $test_name = $file;
    
    // SKIPIF check
    $skip = false;
    if (isset($sections['skipif'])) {
        $skipif_path = $file . '.skipif';
        ob_start();
        @include($skipif_path);
        $skip_output = ob_get_clean();
        $skip_output = trim($skip_output);
        if (!empty($skip_output)) {
            $skip = true;
        }
    }
    
    if ($skip) {
        if ($output_format == "tap") {
            echo "ok $count - $test_name # skip\n";
        } else {
            echo "S";
        }
        $skipped++;
    } else {
        // Check for unimplemented sections
        $has_unimplemented = false;
        foreach ($not_implemented as $section) {
            if (isset($sections[$section]) && !empty(trim($sections[$section]))) {
                $has_unimplemented = true;
                break;
            }
        }
        
        if ($has_unimplemented) {
            if ($output_format == "tap") {
                echo "not ok $count - $test_name # TODO no runner support\n";
            } else {
                echo "F";
            }
            $nimp++;
        } else {
            // Test execution
            $file_path = $file . '.file';
            ob_start();
            @include($file_path);
            $output = ob_get_clean();
            $output = trim($output);
            
            $expected = isset($sections['expect']) ? trim($sections['expect']) : '';
            
            if ($output === $expected) {
                if ($output_format == "tap") {
                    echo "ok $count - $test_name\n";
                } else {
                    echo ".";
                }
                $passed++;
            } else {
                if ($output_format == "tap") {
                    echo "not ok $count - $test_name\n";
                } else {
                    echo "F";
                }
                $failed++;
            }
        }
    }
    
    // CLEAN execution
    if (isset($sections['clean'])) {
        $clean_path = $file . '.clean';
        ob_start();
        @include($clean_path);
        ob_end_clean();
    }
    
    $count++;
    
    // Clean up temp files
    @unlink($file . '.file');
    @unlink($file . '.skipif');
    @unlink($file . '.clean');
    @unlink($file . '.output');
    @unlink($file . '.expect');
}

if ($output_format == "tap") {
    echo "# Incomplete Tests\n";
    echo "# ----------------\n";
    echo "#     ok: $skipped\t(# skip)\n";
    echo "# not ok: $nimp\t(# TODO)\n";
    echo "# ----------------\n";
    echo "#  Total: " . ($skipped + $nimp) . " incomplete\n";
    echo "\n";
    echo "# Test Summary\n";
    echo "# ----------------\n";
    echo "#     ok: " . ($passed + $skipped) . "\n";
    echo "# not ok: " . ($failed + $nimp) . "\n";
    echo "# ----------------\n";
    echo "#  Total: $total tests\n";
} else {
    echo " (" . ($passed + $skipped) . "/$total)\n";
}

if ($total != ($passed + $failed + $skipped + $nimp)) {
    echo "# WARNING: Test count mismatch in directory '$target_dir'.\n";
    exit(1);
}

if ($failed > 0) {
    exit(1);
}
