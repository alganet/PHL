<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

function usage() {
    echo "Usage: lcov_to_markdown.php [--output-dir=DIR] [--source-root=DIR] <coverage.info> [more.info ...]\n";
    echo "\n";
    echo "Generates a Markdown coverage report:\n";
    echo "- One .md file per covered source file\n";
    echo "- index.md files per directory with totals\n";
}

function starts_with($text, $prefix) {
    return strpos($text, $prefix) === 0;
}

function normalize_path_slashes($path) {
    return str_replace('\\', '/', $path);
}

function normalize_sf_to_rel($sf) {
    $sf = normalize_path_slashes($sf);
    $pos = strrpos($sf, '/src/');
    if ($pos !== false) {
        return 'src/' . substr($sf, $pos + 5);
    }
    $pos = strpos($sf, 'src/');
    if ($pos !== false) {
        return substr($sf, $pos);
    }
    return $sf;
}

function ensure_dir($path) {
    if ($path == '' || $path === '.' || is_dir($path)) {
        return true;
    }
    $path = normalize_path_slashes($path);

    $is_abs = (substr($path, 0, 1) === '/');
    $trimmed = trim($path, '/');
    if ($trimmed == '') {
        return $is_abs ? is_dir('/') : true;
    }

    $parts = explode('/', $trimmed);
    $current = $is_abs ? '/' : '';

    foreach ($parts as $part) {
        if ($part == '' || $part === '.') continue;
        if ($current == '' || $current === '/') {
            $current = $current . $part;
        } else {
            $current = $current . '/' . $part;
        }

        if (!is_dir($current)) {
            if (!mkdir($current, 0777)) {
                return false;
            }
        }
    }
    return true;
}

function join_path($a, $b) {
    $a = rtrim($a, '/');
    $b = ltrim($b, '/');
    if ($a == '') return $b;
    if ($b == '') return $a;
    return $a . '/' . $b;
}

function read_text_file($path) {
    $data = file_get_contents($path);
    if ($data === false) return null;
    return $data;
}

function get_source_lines($path) {
    $data = read_text_file($path);
    if ($data === null) return null;
    $data = str_replace("\r\n", "\n", $data);
    $data = str_replace("\r", "\n", $data);
    $lines = explode("\n", $data);
    // If file ended with a newline, explode() adds a final empty entry; keep it as a real line.
    return $lines;
}

function max_backtick_run($text) {
    $max = 0;
    $run = 0;
    $len = strlen($text);
    for ($i = 0; $i < $len; $i++) {
        if ($text[$i] === '`') {
            $run++;
            if ($run > $max) $max = $run;
        } else {
            $run = 0;
        }
    }
    return $max;
}

function md_code_span($text) {
    // Prevent table delimiter issues.
    $text = str_replace('|', '\\|', $text);

    if ($text == '') {
        return '` `';
    }

    $ticks = max_backtick_run($text) + 1;
    $delim = str_repeat('`', $ticks);

    // CommonMark: if content starts/ends with backtick, pad with spaces.
    $needs_pad = false;
    if (substr($text, 0, 1) === '`') $needs_pad = true;
    if (substr($text, -1) === '`') $needs_pad = true;

    if ($needs_pad) {
        return $delim . ' ' . $text . ' ' . $delim;
    }

    return $delim . $text . $delim;
}

function percent_str($hit, $total) {
    if ($total <= 0) return '0.00%';
    return sprintf('%.2f%%', ($hit / $total) * 100.0);
}

function write_file($path, $content) {
    $dir = dirname($path);
    if (!ensure_dir($dir)) {
        echo "Error: cannot create directory: $dir\n";
        return false;
    }
    $ok = file_put_contents($path, $content);
    if ($ok === false) {
        echo "Error: cannot write file: $path\n";
        return false;
    }
    return true;
}

function rel_link_to($fromPath, $toPath) {
    // Both are relative paths inside output dir.
    $fromDir = dirname($fromPath);
    if ($fromDir === '.') $fromDir = '';

    $fromParts = $fromDir == '' ? array() : explode('/', $fromDir);
    $toParts = $toPath == '' ? array() : explode('/', $toPath);

    // Trim common prefix.
    while (count($fromParts) > 0 && count($toParts) > 0 && $fromParts[0] === $toParts[0]) {
        array_shift($fromParts);
        array_shift($toParts);
    }

    $up = '';
    for ($i = 0; $i < count($fromParts); $i++) {
        $up .= '../';
    }
    return $up . implode('/', $toParts);
}

function parse_lcov_files($info_files) {
    $files = array(); // relpath => ['lines' => [line => hits]]

    foreach ($info_files as $info_file) {
        $fh = fopen($info_file, 'r');
        if ($fh === false) {
            echo "Error: cannot open file: $info_file\n";
            exit(1);
        }

        $current = null;
        while (($line = fgets($fh)) !== false) {
            $line = rtrim($line, "\r\n");

            if (starts_with($line, 'SF:')) {
                $sf = substr($line, 3);
                $current = normalize_sf_to_rel($sf);
                if (!isset($files[$current])) {
                    $files[$current] = array('lines' => array());
                }
            } elseif ($current !== null && starts_with($line, 'DA:')) {
                // DA:<line>,<hits>[,<checksum>]
                $payload = substr($line, 3);
                $comma = strpos($payload, ',');
                if ($comma === false) continue;
                $line_no = (int)substr($payload, 0, $comma);
                $rest = substr($payload, $comma + 1);
                $comma2 = strpos($rest, ',');
                $hits_str = ($comma2 === false) ? $rest : substr($rest, 0, $comma2);
                $hits = (int)$hits_str;

                if (!isset($files[$current]['lines'][$line_no])) {
                    $files[$current]['lines'][$line_no] = 0;
                }
                $files[$current]['lines'][$line_no] += $hits;
            } elseif ($line === 'end_of_record') {
                $current = null;
            }
        }

        fclose($fh);
    }

    return $files;
}

function compute_file_totals($file_data) {
    $lines = isset($file_data['lines']) ? $file_data['lines'] : array();
    $total = 0;
    $hit = 0;
    foreach ($lines as $n => $h) {
        $total++;
        if ($h > 0) $hit++;
    }
    return array($hit, $total);
}

function add_dir_totals(&$dir_totals, $dir, $hit, $total) {
    if (!isset($dir_totals[$dir])) {
        $dir_totals[$dir] = array('hit' => 0, 'total' => 0);
    }
    $dir_totals[$dir]['hit'] += $hit;
    $dir_totals[$dir]['total'] += $total;
}

function build_tree($files, &$dir_totals) {
    $tree = array(); // dir => ['dirs'=>[child=>true], 'files'=>[file=>true]]

    foreach ($files as $rel => $data) {
        list($hit, $total) = compute_file_totals($data);

        $dir = dirname($rel);
        if ($dir === '.') $dir = '';

        // Populate tree relationships.
        if (!isset($tree[$dir])) $tree[$dir] = array('dirs' => array(), 'files' => array());
        $tree[$dir]['files'][$rel] = true;

        // Ensure parent dirs exist and record child dir.
        $child_dir = $dir;
        while (true) {
            add_dir_totals($dir_totals, $child_dir, $hit, $total);

            if ($child_dir == '') break;

            $parent = dirname($child_dir);
            if ($parent === '.') $parent = '';

            if (!isset($tree[$parent])) $tree[$parent] = array('dirs' => array(), 'files' => array());
            $tree[$parent]['dirs'][$child_dir] = true;

            if ($parent === $child_dir) break;
            $child_dir = $parent;
        }
    }

    if (!isset($tree[''])) {
        $tree[''] = array('dirs' => array(), 'files' => array());
    }

    return $tree;
}

function sort_keys($assoc) {
    $keys = array_keys($assoc);
    sort($keys);
    return $keys;
}

function generate_file_markdown($rel, $source_path, $hits_by_line, $out_rel_md) {
    $lines = get_source_lines($source_path);
    if ($lines === null) {
        return null;
    }

    // Ensure stable ordering for hits.
    ksort($hits_by_line);

    $tracked_total = 0;
    $tracked_hit = 0;
    foreach ($hits_by_line as $n => $h) {
        $tracked_total++;
        if ($h > 0) $tracked_hit++;
    }

    $rate = percent_str($tracked_hit, $tracked_total);

    $dir = dirname($rel);
    if ($dir === '.') $dir = '';

    $parent_index_rel = ($dir == '') ? 'index.md' : join_path($dir, 'index.md');
    $root_index_rel = 'index.md';

    $nav = array();
    $nav[] = '[Root index](' . rel_link_to($out_rel_md, $root_index_rel) . ')';
    $nav[] = '[Directory index](' . rel_link_to($out_rel_md, $parent_index_rel) . ')';

    $md = '';
    $md .= '# ' . $rel . "\n\n";
    $md .= '<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>' . "\n\n";
    $md .= 'Coverage: ' . $tracked_hit . '/' . $tracked_total . ' lines (' . $rate . ")\n\n";
    $md .= implode(' | ', $nav) . "\n\n";

    $line_count = count($lines);
    $line_col_width = strlen('Line');
    $line_digits = strlen((string)$line_count);
    if ($line_digits > $line_col_width) $line_col_width = $line_digits;

    $max_hits = 0;
    foreach ($hits_by_line as $n => $h) {
        if ($h > $max_hits) $max_hits = $h;
    }
    $hits_col_width = strlen('Hits');
    $hits_digits = strlen((string)$max_hits);
    if ($hits_digits > $hits_col_width) $hits_col_width = $hits_digits;
    // Account for the warning marker on uncovered tracked lines.
    $warn_zero = '! 0';
    $warn_len = strlen($warn_zero);
    if ($warn_len > $hits_col_width) $hits_col_width = $warn_len;

    // Separator cells sized to keep pipes aligned in plain text.
    // Right-aligned columns use ---: and grow with width.
    $line_sep = str_repeat('-', $line_col_width - 1) . ':';
    $hits_sep = str_repeat('-', $hits_col_width - 1) . ':';

    $md .= '| ' . str_pad('Hits', $hits_col_width, ' ', STR_PAD_LEFT) . ' | ' . str_pad('Line', $line_col_width, ' ', STR_PAD_LEFT) . " | Source |\n";
    $md .= '| ' . $hits_sep . ' | ' . $line_sep . " | :--- |\n";

    for ($i = 1; $i <= $line_count; $i++) {
        $raw = $lines[$i - 1];
        // Avoid carrying CR, keep other whitespace.
        $raw = rtrim($raw, "\r");

        $hits_cell = '-';
        if (isset($hits_by_line[$i])) {
            $hits_value = (int)$hits_by_line[$i];
            if ($hits_value === 0) {
                $hits_cell = '! 0';
            } else {
                $hits_cell = (string)$hits_value;
            }
        }

        $source_cell = '';
        if ($raw != '') {
            $source_cell = md_code_span($raw);
        }

        $md .= '| ' . str_pad($hits_cell, $hits_col_width, ' ', STR_PAD_LEFT) . ' | ' . str_pad((string)$i, $line_col_width, ' ', STR_PAD_LEFT) . ' | ' . $source_cell . " |\n";
    }

    return $md;
}

function generate_dir_index_markdown($dir, $tree, $dir_totals, $out_rel_index) {
    $hit = isset($dir_totals[$dir]) ? $dir_totals[$dir]['hit'] : 0;
    $total = isset($dir_totals[$dir]) ? $dir_totals[$dir]['total'] : 0;
    $rate = percent_str($hit, $total);

    $title = ($dir == '') ? '/' : $dir . '/';

    $md = '';
    $md .= '# ' . $title . "\n\n";
    $md .= 'Coverage: ' . $hit . '/' . $total . ' lines (' . $rate . ")\n\n";

    if ($dir != '') {
        $parent = dirname($dir);
        if ($parent === '.') $parent = '';
        $parent_index = ($parent == '') ? 'index.md' : join_path($parent, 'index.md');
        $md .= '[Up](' . rel_link_to($out_rel_index, $parent_index) . ")\n\n";
    }

    $md .= "| Name | Rate | Hit/Total |\n";
    $md .= "|:---|---:|---:|\n";

    $dirs = isset($tree[$dir]) ? $tree[$dir]['dirs'] : array();
    $files = isset($tree[$dir]) ? $tree[$dir]['files'] : array();

    foreach (sort_keys($dirs) as $child_dir) {
        $child_hit = isset($dir_totals[$child_dir]) ? $dir_totals[$child_dir]['hit'] : 0;
        $child_total = isset($dir_totals[$child_dir]) ? $dir_totals[$child_dir]['total'] : 0;
        $child_rate = percent_str($child_hit, $child_total);

        $name = basename($child_dir) . '/';
        $link = join_path(basename($child_dir), 'index.md');
        $md .= '|[' . $name . '](' . $link . ')|' . $child_rate . '|' . $child_hit . '/' . $child_total . "|\n";
    }

    foreach (sort_keys($files) as $rel) {
        $data = $GLOBALS['__lcov_files'][$rel];
        list($fh, $ft) = compute_file_totals($data);
        $fr = percent_str($fh, $ft);

        $base = basename($rel);
        $link = $base . '.md';
        $md .= '|[' . $base . '](' . $link . ')|' . $fr . '|' . $fh . '/' . $ft . "|\n";
    }

    return $md;
}

// ---- CLI ----

$output_dir = null;
$source_root = null;
$info_files = array();

// Skip $argv[0] (the script name) — PHL now matches PHP's $argv convention.
foreach (array_slice($argv, 1) as $arg) {
    if (starts_with($arg, '--output-dir=')) {
        $output_dir = substr($arg, 13);
    } elseif (starts_with($arg, '--source-root=')) {
        $source_root = substr($arg, 14);
    } elseif (starts_with($arg, '--')) {
        echo "Unknown option: $arg\n";
        usage();
        exit(1);
    } else {
        $info_files[] = $arg;
    }
}

if (count($info_files) === 0) {
    usage();
    exit(1);
}

if ($output_dir === null || $output_dir == '') {
    // Default: alongside first input file.
    $output_dir = dirname($info_files[0]);
    if ($output_dir === '.') $output_dir = '';
    $output_dir = join_path($output_dir, 'markdown');
}

if ($source_root === null) {
    $source_root = '';
}

if (!ensure_dir($output_dir)) {
    echo "Error: cannot create output directory: $output_dir\n";
    exit(1);
}

$__lcov_files = parse_lcov_files($info_files);
$GLOBALS['__lcov_files'] = $__lcov_files;

$dir_totals = array();
$tree = build_tree($__lcov_files, $dir_totals);

// Generate per-file documents.
$missing_sources = array();
foreach ($__lcov_files as $rel => $data) {
    $source_path = $rel;
    if ($source_root != '') {
        $source_path = join_path($source_root, $rel);
    }

    if (!is_file($source_path)) {
        $missing_sources[] = $rel;
        continue;
    }

    $out_rel_md = $rel . '.md';
    $out_abs_md = join_path($output_dir, $out_rel_md);

    $hits = isset($data['lines']) ? $data['lines'] : array();
    $md = generate_file_markdown($rel, $source_path, $hits, $out_rel_md);
    if ($md === null) {
        $missing_sources[] = $rel;
        continue;
    }

    if (!write_file($out_abs_md, $md)) {
        exit(1);
    }
}

// Generate directory indexes.
foreach (array_keys($tree) as $dir) {
    $out_rel_index = ($dir == '') ? 'index.md' : join_path($dir, 'index.md');
    $out_abs_index = join_path($output_dir, $out_rel_index);

    $md = generate_dir_index_markdown($dir, $tree, $dir_totals, $out_rel_index);

    if ($dir == '' && count($missing_sources) > 0) {
        sort($missing_sources);
        $md .= "\n\n## Missing sources\n\n";
        foreach ($missing_sources as $m) {
            $md .= '- ' . $m . "\n";
        }
    }

    if (!write_file($out_abs_index, $md)) {
        exit(1);
    }
}

echo "Wrote markdown coverage report to: $output_dir\n";
?>
