<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/**
 * PHP test runner and TAP producer, compatible with PHP and PHL
 */

// Valid section types
$phpt_valid_sections = array('test', 'description', 'credits', 'skipif', 'file', 'expect', 'expectf', 'expectregex', 'expect_stderr', 'clean', 'post', 'post_raw', 'get', 'cookie', 'stdin', 'ini', 'args', 'env');

// Unimplemented section types. The remaining four all describe a CGI request;
// PHL is CLI + `-S` only (scope policy), so they stay unimplemented by design
// rather than pending — a test carrying one is reported, never silently passed.
$phpt_not_implemented = array('post', 'post_raw', 'get', 'cookie');

// Default values
$phpt_target_executable = "";
$phpt_target_timeout = 1;
$phpt_target_dir = dirname(__FILE__);
$phpt_file_extension = "phpt";
$phpt_filter = "";
$phpt_output_format = "tap";
$phpt_shard_index = 0; // 1-based shard to run; 0 = no sharding (run all)
$phpt_shard_total = 0; // total number of shards
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
        case '--shard':
            // Run only shard k of n (e.g. --shard 2/4). Deterministically
            // partitions the sorted test list so CI can fan a slow run across
            // parallel workers; results are merged downstream.
            $phpt_shard_spec = array_shift($phpt_args);
            if ($phpt_shard_spec === null
                || !preg_match('#^([0-9]+)/([0-9]+)$#', $phpt_shard_spec, $phpt_shard_m)) {
                echo "Error: --shard requires a value of the form k/n (e.g. 2/4)\n";
                exit(1);
            }
            $phpt_shard_index = (int)$phpt_shard_m[1];
            $phpt_shard_total = (int)$phpt_shard_m[2];
            if ($phpt_shard_total < 1 || $phpt_shard_index < 1 || $phpt_shard_index > $phpt_shard_total) {
                echo "Error: --shard k/n requires 1 <= k <= n\n";
                exit(1);
            }
            break;
        case '--help':
            echo "Usage: " . $phpt_script_name . " [options]\n";
            echo "\n";
            echo "Options:\n";
            echo "  --target-executable <exe>  Path to the executable being tested. Omit to run\n";
            echo "                             tests in-process (fast); the in-process corpus must\n";
            echo "                             not call exit/die or pollute the interpreter\n";
            echo "                             (see tests/phptrunner/README.md).\n";
            echo "  --target-timeout <sec>     Timeout for each test in seconds (default: 1)\n";
            echo "  --target-dir <dir>         Directory containing test files (default: directory of this script)\n";
            echo "  --file-extension <ext>     File extension for test files (default: phpt)\n";
            echo "  --filter <pattern>         Filter test files by prefix (optional, runs all if not specified)\n";
            echo "  --output-format <format>   Output format: tap (default) or dot\n";
            echo "  --shard <k/n>              Run only shard k of n (e.g. 2/4); partitions the\n";
            echo "                             sorted test list for parallel CI runs (default: all)\n";
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

// Abort guard. Tests run in-process by default (fast; the smoke corpus is
// curated to never exit/die or pollute the interpreter). If a test does kill
// the interpreter, this shutdown function turns the otherwise-silent
// truncation into a loud TAP "Bail out!" with a nonzero exit status.
$phpt_run_complete = false;
$phpt_current_test = '';
function phpt_abort_guard() {
    global $phpt_run_complete, $phpt_current_test, $phpt_count;
    if (!$phpt_run_complete) {
        // The aborted test left its ob_start() buffer open; discard it so the
        // Bail out! line reaches stdout instead of the dangling buffer.
        while (ob_get_level() > 0) {
            ob_end_clean();
        }
        echo "\nBail out! Test run aborted at test #$phpt_count ($phpt_current_test).\n";
        echo "# A test terminated the interpreter (exit/die?). In-process tests must not\n";
        echo "# do that; move it to tests/ph7/002-integration and run with --target-executable.\n";
        exit(1);
    }
}
register_shutdown_function('phpt_abort_guard');

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
    // Both engines call a user handler for EVERY diagnostic, including ones the
    // '@' operator or error_reporting() masks out -- the mask gates only the
    // engine's own printed copy, and a handler is expected to consult
    // error_reporting() itself. Without this the in-process runner printed
    // warnings that the very same test suppressed correctly when run through
    // --target-executable, so '@' appeared to work or not depending purely on
    // how the test was executed.
    if (!(error_reporting() & $errno)) {
        return true;
    }
    echo "Error [$errno]: $errstr in $errfile on line $errline\n";
    return true;
}

function match_expectf_pattern($phpt_pattern, $phpt_output) {
    // Handle %d (digits), %s (non-whitespace), %f (floats), %A (any chars
    // including empty, lazy) and %% (literal percent) without regex.
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

            if ($phpt_pattern[$phpt_p] === 'A') {
                // Match any characters (possibly empty) lazily: try the
                // shortest advance of the output cursor for which the rest
                // of the pattern matches the rest of the output.
                $phpt_p++; // move past 'A'
                $phpt_rest_pattern = substr($phpt_pattern, $phpt_p);
                for ($phpt_i = $phpt_o; $phpt_i <= $phpt_output_len; $phpt_i++) {
                    if (match_expectf_pattern($phpt_rest_pattern, substr($phpt_output, $phpt_i))) {
                        return true;
                    }
                }
                return false;
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
                // Match one or more non-whitespace characters, LAZILY: try the
                // shortest run for which the rest of the pattern also matches.
                // Consuming greedily (what this did before) made a literal that
                // follows %s unmatchable whenever the literal itself is
                // non-whitespace -- '%s:2' could never match 'some/path:2'
                // because %s swallowed ':2' as well and never gave it back.
                // That is why so many expectations had to fall back to %A and
                // assert almost nothing; keep the character class as-is and
                // only add backtracking.
                $phpt_p++; // move past 's'
                $phpt_rest_pattern = substr($phpt_pattern, $phpt_p);
                for ($phpt_i = $phpt_o + 1; $phpt_i <= $phpt_output_len; $phpt_i++) {
                    if (ctype_space($phpt_output[$phpt_i - 1])) {
                        break; // the run may not cross whitespace
                    }
                    if (match_expectf_pattern($phpt_rest_pattern, substr($phpt_output, $phpt_i))) {
                        return true;
                    }
                }
                return false;
            } elseif ($phpt_pattern[$phpt_p] === 'f') {
                // Match a float: optional sign, digits, optional decimal, optional exponent
                if ($phpt_o >= $phpt_output_len) {
                    return false;
                }
                // Optional sign
                if ($phpt_output[$phpt_o] === '+' || $phpt_output[$phpt_o] === '-') {
                    $phpt_o++;
                }
                // Digits before decimal
                if ($phpt_o >= $phpt_output_len || !ctype_digit($phpt_output[$phpt_o])) {
                    return false;
                }
                while ($phpt_o < $phpt_output_len && ctype_digit($phpt_output[$phpt_o])) {
                    $phpt_o++;
                }
                // Optional decimal part
                if ($phpt_o < $phpt_output_len && $phpt_output[$phpt_o] === '.') {
                    $phpt_o++;
                    while ($phpt_o < $phpt_output_len && ctype_digit($phpt_output[$phpt_o])) {
                        $phpt_o++;
                    }
                }
                // Optional exponent
                if ($phpt_o < $phpt_output_len && ($phpt_output[$phpt_o] === 'e' || $phpt_output[$phpt_o] === 'E')) {
                    $phpt_o++;
                    if ($phpt_o < $phpt_output_len && ($phpt_output[$phpt_o] === '+' || $phpt_output[$phpt_o] === '-')) {
                        $phpt_o++;
                    }
                    if ($phpt_o >= $phpt_output_len || !ctype_digit($phpt_output[$phpt_o])) {
                        return false;
                    }
                    while ($phpt_o < $phpt_output_len && ctype_digit($phpt_output[$phpt_o])) {
                        $phpt_o++;
                    }
                }
            } elseif ($phpt_pattern[$phpt_p] === '%') {
                // Literal % match (escaped as %%)
                if ($phpt_o >= $phpt_output_len || $phpt_output[$phpt_o] !== '%') {
                    return false;
                }
                $phpt_o++;
            } else {
                return false; // unknown % sequence
            }
            $phpt_p++; // move past d, s, f, or %
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

// Parse an --ENV-- section into an ordered map of KEY => VALUE. Each non-empty
// line is "KEY=VALUE" (VALUE may be empty); blank lines are ignored.
function parse_env_section($phpt_env_text) {
    $phpt_env = array();
    foreach (explode("\n", $phpt_env_text) as $phpt_env_line) {
        $phpt_env_line = trim($phpt_env_line);
        if ($phpt_env_line === '' || strpos($phpt_env_line, '=') === false) {
            continue;
        }
        list($phpt_env_key, $phpt_env_val) = explode('=', $phpt_env_line, 2);
        // Trim both sides: `KEY = 32` is a common way to write the section, and a
        // stray space would otherwise reach the child as a value like " 32".
        $phpt_env[trim($phpt_env_key)] = trim($phpt_env_val);
    }
    return $phpt_env;
}

// Build the platform-appropriate command prefix that exports $env for a child
// process (used for --ENV-- support). Values are quoted; keys are assumed sane.
function build_env_prefix($phpt_env) {
    // The runner is self-hosted under phl, so avoid escapeshellarg() (not a phl
    // builtin) and quote POSIX values by hand: wrap in single quotes, closing +
    // escaping any embedded quote.
    $phpt_prefix = '';
    foreach ($phpt_env as $phpt_env_key => $phpt_env_val) {
        if (PHP_OS === 'WINNT') {
            // set "KEY=VAL"&& — the quotes bound the value so a trailing space
            // before && is NOT captured into it (cmd.exe would otherwise keep it).
            $phpt_prefix .= 'set "' . $phpt_env_key . '=' . $phpt_env_val . '"&& ';
        } else {
            $phpt_quoted = "'" . str_replace("'", "'\\''", $phpt_env_val) . "'";
            $phpt_prefix .= $phpt_env_key . '=' . $phpt_quoted . ' ';
        }
    }
    return $phpt_prefix;
}

// Run a PHPT section file through an external target executable
// Returns the combined stdout/stderr output as string, or false if popen() failed
function run_file_with_target($phpt_target_executable, $phpt_file, $phpt_env = array(), $phpt_ini = array(), $phpt_argv_tail = '', $phpt_stdin = null, $phpt_split_stderr = false, &$phpt_stderr_out = null) {
    // Export PHPT_TARGET_EXECUTABLE through the same per-OS, value-quoting path as
    // the --ENV-- vars (build_env_prefix) so a target path containing a space is
    // quoted too. The '+' keeps PHPT_TARGET_EXECUTABLE ahead of any --ENV-- vars.
    $phpt_full_env = array('PHPT_TARGET_EXECUTABLE' => $phpt_target_executable) + $phpt_env;
    $cmd = build_env_prefix($phpt_full_env);
    $cmd .= '"' . $phpt_target_executable . '"';
    // --INI-- directives: pass each as a `-d name=value` CLI flag to the fresh
    // child (mirrors php run-tests). Quote the whole token per-OS like the env.
    foreach ($phpt_ini as $phpt_ini_key => $phpt_ini_val) {
        $phpt_ini_tok = $phpt_ini_key . '=' . $phpt_ini_val;
        if (PHP_OS === 'WINNT') {
            $cmd .= ' -d "' . str_replace('"', '\\"', $phpt_ini_tok) . '"';
        } else {
            $cmd .= ' -d ' . "'" . str_replace("'", "'\\''", $phpt_ini_tok) . "'";
        }
    }
    $cmd .= ' "' . $phpt_file . '"';
    // --ARGS--: appended after the script path, so the child sees them as
    // $argv[1..] with $argv[0] still the script (php run-tests does the same).
    // Passed through verbatim -- the section is shell-level text by design.
    if ($phpt_argv_tail !== '') {
        $cmd .= ' ' . $phpt_argv_tail;
    }
    // --STDIN--: fed by redirecting a temp file into the child. popen() only
    // gives us one pipe direction, and a redirect is portable across cmd.exe
    // and POSIX shells, so this avoids depending on proc_open().
    $phpt_stdin_path = null;
    if ($phpt_stdin !== null) {
        $phpt_stdin_path = $phpt_file . '.stdin';
        file_put_contents($phpt_stdin_path, $phpt_stdin);
        $cmd .= ' < "' . $phpt_stdin_path . '"';
    }
    // --EXPECT_STDERR-- opts a test into stream SEPARATION: redirect the child's
    // stderr to a temp file (portable across cmd.exe/POSIX, same as the .stdin
    // temp above) so stdout and stderr can be asserted independently. Without it,
    // keep the default 2>&1 merge so the existing corpus is unaffected.
    $phpt_stderr_path = null;
    if ($phpt_split_stderr) {
        $phpt_stderr_path = $phpt_file . '.stderr';
        $cmd .= ' 2>"' . $phpt_stderr_path . '"';
    } else {
        $cmd .= ' 2>&1';
    }
    $fp = popen($cmd, 'r');
    if ($fp === false) {
        // piped execution failed
        if ($phpt_stdin_path !== null) {
            @unlink($phpt_stdin_path);
        }
        if ($phpt_stderr_path !== null) {
            @unlink($phpt_stderr_path);
        }
        return false;
    }
    $output = '';
    while (!feof($fp)) {
        $chunk = fgets($fp);
        if ($chunk === false) break;
        $output .= $chunk;
    }
    pclose($fp);
    if ($phpt_stderr_path !== null) {
        $phpt_stderr_out = file_exists($phpt_stderr_path) ? file_get_contents($phpt_stderr_path) : '';
        @unlink($phpt_stderr_path);
    }
    if ($phpt_stdin_path !== null) {
        @unlink($phpt_stdin_path);
    }
    return $output;
}

// Find test files
$phpt_files = find_files($phpt_target_dir, $phpt_file_extension, $phpt_filter);
sort($phpt_files);

// Keep only this shard's slice (deterministic: by position in the sorted list).
if ($phpt_shard_total > 0) {
    $phpt_sharded = array();
    foreach ($phpt_files as $phpt_i => $phpt_f) {
        if (($phpt_i % $phpt_shard_total) === ($phpt_shard_index - 1)) {
            $phpt_sharded[] = $phpt_f;
        }
    }
    $phpt_files = $phpt_sharded;
}

$phpt_total = count($phpt_files);
if ($phpt_output_format == "tap") {
    echo "1..$phpt_total\n";
}

$phpt_passed = 0;
$phpt_failed = 0;
$phpt_skipped = 0;
$phpt_nimp = 0;
$phpt_count = 1;
$phpt_failures = array();
$phpt_skip_reasons = array();

foreach ($phpt_files as $phpt_file) {
    $phpt_sections = parse_phpt_sections($phpt_file, $phpt_valid_sections);

    // Optional --ENV-- section: extra environment for the target CHILD process.
    // Only meaningful with --target-executable: it is exported via the command
    // prefix so a fresh child reads it at startup. In-process (@include) runs
    // share the runner's own already-started VM, so env cannot be applied there
    // (phl's putenv can't unset for a clean restore, and startup-read knobs like
    // PHL_MAX_* would not re-read) — such tests are skipped below rather than
    // run with the env silently dropped.
    $phpt_env = isset($phpt_sections['env']) ? parse_env_section($phpt_sections['env']) : array();
    $phpt_env_unsupported = (!empty($phpt_env) && empty($phpt_target_executable));

    // Optional --INI-- section: php.ini directives (name=value lines) passed to the
    // target CHILD as `-d name=value`. Like --ENV--, only meaningful with a fresh
    // child process; the in-process (@include) runner shares the runner's own VM
    // (started without those -d flags), so such tests are skipped there.
    $phpt_ini = isset($phpt_sections['ini']) ? parse_env_section($phpt_sections['ini']) : array();
    $phpt_ini_unsupported = (!empty($phpt_ini) && empty($phpt_target_executable));

    // Optional --ARGS-- / --STDIN--: both shape the CHILD's invocation (argv tail,
    // redirected stdin), so like --ENV--/--INI-- they need a fresh process. The
    // in-process runner shares the runner's own argv and stdin, so those tests are
    // skipped there rather than run with the section silently dropped.
    $phpt_argv_tail = isset($phpt_sections['args']) ? trim($phpt_sections['args']) : '';
    $phpt_args_unsupported = ($phpt_argv_tail !== '' && empty($phpt_target_executable));
    $phpt_stdin = isset($phpt_sections['stdin']) ? $phpt_sections['stdin'] : null;
    $phpt_stdin_unsupported = ($phpt_stdin !== null && empty($phpt_target_executable));

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
    $phpt_current_test = $phpt_file;

    // SKIPIF check
    $phpt_skip = false;
    $phpt_skip_reason = '';
    if ($phpt_env_unsupported) {
        // --ENV-- is only honored for a fresh child process (see the parse note);
        // skip rather than run in-process with the env silently dropped.
        $phpt_skip = true;
        $phpt_skip_reason = '--ENV-- requires --target-executable';
    } elseif ($phpt_ini_unsupported) {
        // --INI-- is applied as -d flags to a fresh child; skip in-process runs.
        $phpt_skip = true;
        $phpt_skip_reason = '--INI-- requires --target-executable';
    } elseif ($phpt_args_unsupported) {
        // --ARGS-- becomes the child's argv tail; skip in-process runs.
        $phpt_skip = true;
        $phpt_skip_reason = '--ARGS-- requires --target-executable';
    } elseif ($phpt_stdin_unsupported) {
        // --STDIN-- is redirected into the child; skip in-process runs.
        $phpt_skip = true;
        $phpt_skip_reason = '--STDIN-- requires --target-executable';
    } elseif (isset($phpt_sections['skipif'])) {
        $phpt_skipif_path = $phpt_file . '.skipif';
        if (!empty($phpt_target_executable)) {
            $phpt_skip_output = run_file_with_target($phpt_target_executable, $phpt_skipif_path, $phpt_env, $phpt_ini);
            if ($phpt_skip_output === false) {
                echo "# ERROR: Failed to spawn skipif for $phpt_skipif_path\n";
                $phpt_skip_output = '';
            }
        } else {
            ob_start();
            set_error_handler('handle_error');
            include($phpt_skipif_path);
            set_error_handler(null);
            $phpt_skip_output = ob_get_clean();
        }
        chdir($phpt_curdir);
        $phpt_skip_output = trim($phpt_skip_output);
        if (!empty($phpt_skip_output)) {
            $phpt_skip = true;
            // Normalize the SKIPIF's message into a one-line TAP reason: drop
            // the conventional leading "skip" word (the directive adds its
            // own) and collapse newlines.
            $phpt_skip_reason = str_replace(array("\r", "\n"), ' ', $phpt_skip_output);
            if (strncasecmp($phpt_skip_reason, 'skip', 4) === 0) {
                $phpt_skip_reason = ltrim(substr($phpt_skip_reason, 4), " \t-:");
            }
            $phpt_skip_reason = trim($phpt_skip_reason);
        }
    }

    if ($phpt_skip) {
        if ($phpt_skip_reason === '') {
            $phpt_skip_reason = '(no reason given)';
        }
        if ($phpt_output_format == "tap") {
            echo "ok $phpt_count - $phpt_test_name # skip $phpt_skip_reason\n";
        } else {
            echo "S";
        }
        if (!isset($phpt_skip_reasons[$phpt_skip_reason])) {
            $phpt_skip_reasons[$phpt_skip_reason] = 0;
        }
        $phpt_skip_reasons[$phpt_skip_reason]++;
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
                $phpt_failures[] = array(
                    'count' => $phpt_count,
                    'name' => $phpt_test_name,
                    'error' => 'Unsupported phpt sections: ' . implode(', ', array_map('strtoupper', array_filter($phpt_not_implemented, function($section) use ($phpt_sections) {
                        return isset($phpt_sections[$section]) && !empty(trim($phpt_sections[$section]));
                    })))
                );
            }
            $phpt_nimp++;
        } else {
            // Test execution
            $phpt_file_path = $phpt_file . '.file';
            // --EXPECT_STDERR-- asserts the child's stderr separately; only the
            // subprocess (target-executable) tier can capture a distinct stderr.
            $phpt_want_stderr = isset($phpt_sections['expect_stderr']);
            $phpt_stderr_captured = '';
            if (!empty($phpt_target_executable)) {
                $phpt_output = run_file_with_target($phpt_target_executable, $phpt_file_path, $phpt_env, $phpt_ini, $phpt_argv_tail, $phpt_stdin, $phpt_want_stderr, $phpt_stderr_captured);
                if ($phpt_output === false) {
                    echo "# ERROR: Failed to spawn test for $phpt_file_path\n";
                    $phpt_output = "";
                }
            } else {
                ob_start();
                set_error_handler('handle_error');
                // NOT @include: the '@' applied to the whole body put every
                // in-process test inside the silence operator, so
                // error_reporting() read 4437 throughout and a test could not
                // observe '@' at all -- handle_error printed suppressed
                // diagnostics, while the same test under --target-executable
                // suppressed them correctly. Any diagnostic the include itself
                // raises is part of what the test asserts.
                include($phpt_file_path);
                set_error_handler(null);
                $phpt_output = ob_get_clean();
            }
            $phpt_output = str_replace("\r\n", "\n", $phpt_output);
            $phpt_output = str_replace("\r", "", $phpt_output);
            $phpt_stderr_captured = str_replace("\r\n", "\n", $phpt_stderr_captured);
            $phpt_stderr_captured = str_replace("\r", "", $phpt_stderr_captured);
            chdir($phpt_curdir);
            $phpt_output = trim($phpt_output);
            $phpt_stderr_captured = trim($phpt_stderr_captured);

            $phpt_expected = isset($phpt_sections['expect']) ? trim($phpt_sections['expect']) : '';
            $phpt_expectedf = isset($phpt_sections['expectf']) ? trim($phpt_sections['expectf']) : '';
            $phpt_expectedr = isset($phpt_sections['expectregex']) ? trim($phpt_sections['expectregex']) : '';

            $phpt_matches = false;
            if ($phpt_expectedr !== '') {
                // --EXPECTREGEX--: the section body is the pattern WITHOUT delimiters
                // (php run-tests wraps it), matched against the whole output. '/' is
                // used as the delimiter, so any literal '/' in the body is escaped.
                $phpt_regex = '/' . str_replace('/', '\\/', $phpt_expectedr) . '/s';
                $phpt_matches = (preg_match($phpt_regex, $phpt_output) === 1);
            } elseif (!empty($phpt_expectedf)) {
                // Use EXPECTF with pattern matching for %d and %s
                $phpt_matches = match_expectf_pattern($phpt_expectedf, $phpt_output);
            } elseif ($phpt_output === $phpt_expected) {
                $phpt_matches = true;
            }

            // --EXPECT_STDERR--: the child's stderr must additionally match. Matched
            // with the EXPECTF engine so %s (file path) / %d (line) wildcards work.
            // Only meaningful under a target-executable; in-process runs can't
            // capture a subprocess stderr, so such a test must live in integration.
            if ($phpt_want_stderr) {
                $phpt_expected_stderr = trim($phpt_sections['expect_stderr']);
                $phpt_matches = ($phpt_matches === true)
                    && match_expectf_pattern($phpt_expected_stderr, $phpt_stderr_captured);
            }

            if ($phpt_matches === true) {
                if ($phpt_output_format == "tap") {
                    echo "ok $phpt_count - $phpt_test_name\n";
                } else {
                    echo ".";
                }
                $phpt_passed++;
            } else {
                if ($phpt_output_format == "dot") {
                    echo "F";
                    // Store failure details for summary at end
                    $phpt_failures[] = array(
                        'count' => $phpt_count,
                        'name' => $phpt_test_name,
                        'expected' => $phpt_expected,
                        'expectedf' => $phpt_expectedf,
                        'expectedr' => $phpt_expectedr,
                        'output' => $phpt_output
                    );
                } else {
                    echo "not ok $phpt_count - $phpt_test_name\n";
                    if ($phpt_expectedr !== '') {
                        echo "# Expected regex: '$phpt_expectedr'\n";
                    } elseif (!empty($phpt_expectedf)) {
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
        if (!empty($phpt_target_executable)) {
            $phpt_clean_output = run_file_with_target($phpt_target_executable, $phpt_clean_path, $phpt_env, $phpt_ini);
            if ($phpt_clean_output === false) {
                echo "# ERROR: Failed to spawn clean for $phpt_clean_path\n";
            }
        } else {
            ob_start();
            include($phpt_clean_path);
            ob_end_clean();
        }
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

// The loop completed normally; tell the abort guard not to fire.
$phpt_run_complete = true;

if ($phpt_output_format == "tap") {
    echo "# Incomplete Tests\n";
    echo "# ----------------\n";
    echo "#     ok: $phpt_skipped\t(# skip)\n";
    echo "# not ok: $phpt_nimp\t(# TODO)\n";
    echo "# ----------------\n";
    echo "#  Total: " . ($phpt_skipped + $phpt_nimp) . " incomplete\n";
    echo "\n";
    if (!empty($phpt_skip_reasons)) {
        echo "# Skips by reason\n";
        echo "# ----------------\n";
        arsort($phpt_skip_reasons);
        foreach ($phpt_skip_reasons as $phpt_reason => $phpt_reason_count) {
            echo "# " . $phpt_reason_count . "\t" . $phpt_reason . "\n";
        }
        echo "\n";
    }
    echo "# Test Summary\n";
    echo "# ----------------\n";
    echo "#     ok: " . ($phpt_passed + $phpt_skipped) . "\n";
    echo "# not ok: " . ($phpt_failed + $phpt_nimp) . "\n";
    echo "# ----------------\n";
    echo "#  Total: $phpt_total tests\n";
} else {
    echo " (" . ($phpt_passed + $phpt_skipped) . "/$phpt_total)\n";
    // Display failure details for dot format
    if (!empty($phpt_failures)) {
        echo "\nFailures\n";
        echo "--------\n";
        foreach ($phpt_failures as $failure) {
            if (isset($failure['error'])) {
                echo "\nnot ok " . $failure['count'] . " - " . $failure['name'] . "\n";
                echo "# ERROR: " . $failure['error'] . "\n";
                continue;
            }
            echo "\nnot ok " . $failure['count'] . " - " . $failure['name'] . "\n";
            if (!empty($failure['expectedr'])) {
                echo "# Expected regex: '" . $failure['expectedr'] . "'\n";
            } elseif (!empty($failure['expectedf'])) {
                echo "# Expected pattern: '" . $failure['expectedf'] . "'\n";
            } else {
                echo "# Expected: '" . $failure['expected'] . "'\n";
            }
            echo "# Actual: '" . $failure['output'] . "'\n";
        }
        echo "\n";
    }
}

if ($phpt_total != ($phpt_passed + $phpt_failed + $phpt_skipped + $phpt_nimp)) {
    echo "# WARNING: Test count mismatch in directory '$phpt_target_dir'.\n";
    exit(1);
}

if ($phpt_failed > 0) {
    exit(1);
}