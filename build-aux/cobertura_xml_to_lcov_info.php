<?php
/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */

$xml_file = isset($argv[0]) ? $argv[0] : null;
if (!isset($xml_file)) {
    echo "Usage: phl cobertura_xml_to_lcov_info.php <cobertura.xml> > coverage.info\n";
    exit(1);
}

$xml = file_get_contents($xml_file);

$parser = xml_parser_create();
xml_parser_set_option($parser, XML_OPTION_CASE_FOLDING, 0);
xml_parser_set_option($parser, XML_OPTION_SKIP_WHITE, 1);

$coverage_data = array();
$current_filename = null;
$current_data = null;

function startElement($parser, $name, $attrs) {
    global $current_filename, $current_data;
    if ($name == 'class') {
        $current_filename = isset($attrs['filename']) ? $attrs['filename'] : '';
        // Fix path
        $current_filename = str_replace('\\', '/', $current_filename);
        $pos = strpos($current_filename, 'PHL-build/');
        if ($pos !== false) {
            $current_filename = substr($current_filename, $pos + strlen('PHL-build/'));
        }
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
        $coverage_data[$current_filename] = $current_data;
        $current_data = null;
        $current_filename = null;
    }
}

xml_set_element_handler($parser, 'startElement', 'endElement');

if (!xml_parse($parser, $xml, true)) {
    die("XML error: " . xml_error_string(xml_get_error_code($parser)) . " at line " . xml_get_current_line_number($parser));
}

xml_parser_free($parser);

// Now output lcov
foreach ($coverage_data as $filename => $data) {
    echo "SF:$filename\n";
    $functions = isset($data['functions']) ? $data['functions'] : array();
    foreach ($functions as $func) {
        echo "FN:{$func['line']},{$func['name']}\n";
    }
    $fnf = count($functions);
    $fnh = 0;
    foreach ($functions as $func) {
        if ($func['hits'] > 0) $fnh++;
    }
    echo "FNF:$fnf\n";
    echo "FNH:$fnh\n";
    echo "BRF:0\n";
    echo "BRH:0\n";
    $lines = isset($data['lines']) ? $data['lines'] : array();
    $found = count($lines);
    $hit = 0;
    foreach ($lines as $line) {
        echo "DA:{$line['number']},{$line['hits']}\n";
        if ($line['hits'] > 0) $hit++;
    }
    echo "LF:$found\n";
    echo "LH:$hit\n";
    echo "end_of_record\n";
}

?>