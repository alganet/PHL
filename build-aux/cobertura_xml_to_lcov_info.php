<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

function usage() {
    echo "Usage: phl cobertura_xml_to_lcov_info.php <cobertura.xml> [more.xml ...] > coverage.info\n";
    echo "   or: php cobertura_xml_to_lcov_info.php <cobertura.xml> [more.xml ...] > coverage.info\n";
}

$coverage_data = array(); // filename => ['lines' => [line => hits], 'functions' => [key => ['name','line','hits']]]
$current_filename = null;
$current_data = null;

function normalize_filename($filename) {
    $filename = str_replace('\\', '/', $filename);
    $pos = strpos($filename, 'src/');
    if ($pos !== false) {
        $filename = substr($filename, $pos);
    }
    return $filename;
}

function merge_coverage_data(&$coverage_data, $filename, $data) {
    if (!isset($coverage_data[$filename])) {
        $coverage_data[$filename] = array('lines' => array(), 'functions' => array());
    }

    if (isset($data['lines'])) {
        foreach ($data['lines'] as $line) {
            $number = (int)$line['number'];
            $hits = (int)$line['hits'];
            if (!isset($coverage_data[$filename]['lines'][$number])) {
                $coverage_data[$filename]['lines'][$number] = 0;
            }
            $coverage_data[$filename]['lines'][$number] += $hits;
        }
    }

    if (isset($data['functions'])) {
        foreach ($data['functions'] as $func) {
            $line = (int)$func['line'];
            $name = (string)$func['name'];
            $hits = (int)$func['hits'];
            $key = $line . ":" . $name;
            if (!isset($coverage_data[$filename]['functions'][$key])) {
                $coverage_data[$filename]['functions'][$key] = array('name' => $name, 'line' => $line, 'hits' => 0);
            }
            $coverage_data[$filename]['functions'][$key]['hits'] += $hits;
        }
    }
}

function ends_with_ci($text, $suffix) {
    $text = (string)$text;
    $suffix = (string)$suffix;
    $text_len = strlen($text);
    $suffix_len = strlen($suffix);
    if ($suffix_len == 0) return true;
    if ($text_len < $suffix_len) return false;
    return strtolower(substr($text, $text_len - $suffix_len)) === strtolower($suffix);
}

function compare_functions($a, $b) {
    if ($a['line'] == $b['line']) {
        return strcmp($a['name'], $b['name']);
    }
    return ($a['line'] < $b['line']) ? -1 : 1;
}

function startElement($parser, $name, $attrs) {
    global $current_filename, $current_data;
    if ($name == 'class') {
        $current_filename = isset($attrs['filename']) ? normalize_filename($attrs['filename']) : '';
        $current_data = array('lines' => array(), 'functions' => array());
    } elseif ($name == 'line' && $current_data !== null) {
        if (isset($attrs['number']) && isset($attrs['hits'])) {
            $current_data['lines'][] = array('number' => (int)$attrs['number'], 'hits' => (int)$attrs['hits']);
        }
    } elseif ($name == 'method' && $current_data !== null) {
        if (isset($attrs['name']) && isset($attrs['line']) && isset($attrs['hits'])) {
            $current_data['functions'][] = array('name' => $attrs['name'], 'line' => (int)$attrs['line'], 'hits' => (int)$attrs['hits']);
        }
    }
}

function endElement($parser, $name) {
    global $current_filename, $current_data, $coverage_data;
    if ($name == 'class' && $current_filename && $current_data) {
        merge_coverage_data($coverage_data, $current_filename, $current_data);
        $current_data = null;
        $current_filename = null;
    }
}

// Collect input XML files.
$input_files = array();
if (isset($argc) && $argc >= 2) {
    for ($i = 1; $i < $argc; $i++) {
        $input_files[] = $argv[$i];
    }
} elseif (!isset($argc) && isset($argv) && is_array($argv) && count($argv) > 0) {
    // PHL/PH7 does not set $argc and does not include the script name in $argv.
    $input_files = $argv;
} elseif (isset($argv[0]) && ends_with_ci($argv[0], '.xml')) {
    // Fallback: accept a single XML path passed as $argv[0].
    $input_files[] = $argv[0];
}

if (count($input_files) == 0) {
    usage();
    exit(1);
}

// Parse each Cobertura file and merge into $coverage_data.
foreach ($input_files as $xml_file) {
    if (!is_string($xml_file) || $xml_file === '' || !is_readable($xml_file)) {
        echo "Error: cannot read input file: $xml_file\n";
        exit(1);
    }

    $xml = file_get_contents($xml_file);
    if ($xml === false) {
        echo "Error: failed to read input file: $xml_file\n";
        exit(1);
    }

    $parser = xml_parser_create();
    xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 0);
    xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);
    xml_set_element_handler($parser, 'startElement', 'endElement');

    if (!xml_parse($parser, $xml, true)) {
        $message = "XML error in $xml_file: " . xml_error_string(xml_get_error_code($parser)) . " at line " . xml_get_current_line_number($parser) . "\n";
        xml_parser_free($parser);
        echo $message;
        exit(1);
    }

    xml_parser_free($parser);
}

// Now output lcov
ksort($coverage_data);
foreach ($coverage_data as $filename => $data) {
    echo "SF:$filename\n";
    $functions = isset($data['functions']) ? $data['functions'] : array();
    // Convert to an ordered list for stable output.
    $functions_list = array_values($functions);
    usort($functions_list, 'compare_functions');
    foreach ($functions_list as $func) {
        echo "FN:{$func['line']},{$func['name']}\n";
    }
    $fnf = count($functions_list);
    $fnh = 0;
    foreach ($functions_list as $func) {
        if ($func['hits'] > 0) $fnh++;
    }
    echo "FNF:$fnf\n";
    echo "FNH:$fnh\n";
    echo "BRF:0\n";
    echo "BRH:0\n";
    $lines = isset($data['lines']) ? $data['lines'] : array();
    ksort($lines);
    $found = count($lines);
    $hit = 0;
    foreach ($lines as $number => $hits) {
        echo "DA:$number,$hits\n";
        if ($hits > 0) $hit++;
    }
    echo "LF:$found\n";
    echo "LH:$hit\n";
    echo "end_of_record\n";
}

?>
