<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

$usage = "Usage: lcov_info_to_text.php [--content=files|summary|all] <coverage.info>\n";

$content = 'all';
$info_file = null;

foreach ($argv as $arg) {
    if (strpos($arg, '--content=') === 0) {
        $content = substr($arg, 10);
    } elseif (!$info_file) {
        $info_file = $arg;
    } else {
        // extra arg
        echo $usage;
        exit(1);
    }
}

if ($content !== 'files' && $content !== 'summary' && $content !== 'all') {
    echo "Invalid content option: $content\n";
    echo $usage;
    exit(1);
}

if (!$info_file) {
    echo $usage;
    exit(1);
}

$lines = file($info_file, FILE_IGNORE_NEW_LINES);
if ($lines === false) {
    echo "Error reading file: $info_file\n";
    exit(1);
}

if ($content === 'files' || $content === 'all') {
    echo "| Filename                       | Rate     | Hit/Total   |\n";
    echo "|--------------------------------|----------|-------------|\n";
}

$file = '';
$total = 0;
$hit = 0;
$total_lines_hit = 0;
$total_lines = 0;
$total_funcs_hit = 0;
$total_funcs = 0;

foreach ($lines as $line) {
    if (strpos($line, 'SF:') === 0) {
        $file = substr($line, 3);
        $pos = strrpos($file, '/src/');
        if ($pos !== false) {
            $file = 'src/' . substr($file, $pos + 5);
        }
    } elseif (strpos($line, 'LF:') === 0) {
        $total = (int)substr($line, 3);
        $total_lines += $total;
    } elseif (strpos($line, 'LH:') === 0) {
        $hit = (int)substr($line, 3);
        $total_lines_hit += $hit;
        if ($total > 0) {
            $rate = sprintf("%.2f%%", ($hit / $total) * 100);
        } else {
            $rate = "0.00%";
        }
        if ($content === 'files' || $content === 'all') {
            echo sprintf("| %-30s | %-8s | %-11s |\n", $file, $rate, "$hit/$total");
        }
    } elseif (strpos($line, 'FNF:') === 0) {
        $total_funcs += (int)substr($line, 4);
    } elseif (strpos($line, 'FNH:') === 0) {
        $total_funcs_hit += (int)substr($line, 4);
    }
}

if ($total_lines > 0) {
    $overall_rate = sprintf("%.2f%%", ($total_lines_hit / $total_lines) * 100);
} else {
    $overall_rate = "0.00%";
}
if ($total_funcs > 0) {
    $func_rate = sprintf("%.2f%%", ($total_funcs_hit / $total_funcs) * 100);
} else {
    $func_rate = "0.00%";
}

if ($content === 'summary' || $content === 'all') {
    echo "\n";
    echo "| Total                          | Rate     | Hit/Total   |\n";
    echo "|--------------------------------|----------|-------------|\n";

    echo sprintf("| %-30s | %-8s | %-11s |\n", "Lines", $overall_rate, "$total_lines_hit/$total_lines");
    if ($total_funcs > 0) {
        echo sprintf("| %-30s | %-8s | %-11s |\n", "Functions", $func_rate, "$total_funcs_hit/$total_funcs");
    }
}
?>