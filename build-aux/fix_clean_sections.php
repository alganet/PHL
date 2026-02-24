<?php

function scan_test_dir($dir) {
    foreach (scandir($dir) as $entry) {
        if ($entry === '.' || $entry === '..') {
            continue;
        }
        $path = $dir . DIRECTORY_SEPARATOR . $entry;
        if (is_dir($path)) {
            scan_test_dir($path);
        } elseif (is_file($path) && substr($entry, -5) === '.phpt') {
            lint_test_file($path);
        }
    }
}

function lint_test_file($file) {
    $content = file_get_contents($file);

    $lines = explode("\n", $content);
    $variables = array();
    foreach ($lines as $line_num => $line) {
        if (strpos($line, '$') !== false && strpos($line, '=') !== false) {
            $parts = explode('=', $line, 2);
            $var_part = trim($parts[0]);
            // clean += etc
            $var_part = rtrim($var_part, '+-*/%&|^<>');
            // clean array access (without preg_replace for simplicity)
            $bracket_pos = strpos($var_part, '[');
            if ($bracket_pos !== false) {
                $var_part = substr($var_part, 0, $bracket_pos);
            }
            // clean object access
            $arrow_pos = strpos($var_part, '->');
            if ($arrow_pos !== false) {
                $var_part = substr($var_part, 0, $arrow_pos);
            }
            // clean static access
            $static_pos = strpos($var_part, '::');
            if ($static_pos !== false) {
                $var_part = substr($var_part, 0, $static_pos);
            }
            // clean function call
            $paren_pos = strpos($var_part, '(');
            if ($paren_pos !== false) {
                $var_part = substr($var_part, 0, $paren_pos);
            }
            // clean variable variables $$
            if (substr($var_part, 0, 2) === '$$') {
                $var_part = substr($var_part, 1);
            }
            // clean $this
            if (substr($var_part, 0, 5) === '$this') {
                continue;
            }
            if (substr($var_part, 0, 1) === '$') {
                $var_name = trim(substr($var_part, 1));
                if ($var_name !== '') {
                    $variables[$var_name] = true;
                }
            }
        }
    }

    // get file without --CLEAN-- section
    $clean_pos = strpos($content, "--CLEAN--");
    $no_clean_content = $clean_pos !== false ? substr($content, 0, $clean_pos) : $content;

    // iterate --CLEAN-- section
    $new_clean_section = '--CLEAN--' . "\n" . '<' . '?php' . "\n";
    if ($clean_pos !== false) {
        $clean_section = substr($content, $clean_pos);
        $clean_lines = explode("\n", $clean_section);
        foreach ($clean_lines as $line) {
            // discard opening --CLEAN-- line
            if (trim($line) === '--CLEAN--') {
                continue;
            }
            // discard closing php tag
            if (strpos($line, '?'.'>') !== false) {
                continue;
            }
            // discard opening php tag
            if (strpos($line, '<'.'?php') !== false) {
                continue;
            }
            // discard empty lines
            if (trim($line) == '') {
                continue;
            }
            // keep lines that don't have an unset
            if (strpos($line, 'unset(') === false) {
                $new_clean_section .= $line . "\n";
            }
        }
    }

    $no_clean_content = trim($no_clean_content, "\n");

    // add the found variables to the new clean section
    if (!empty($variables)) {
        $unset_vars = 'unset(';
        foreach ($variables as $var_name => $_) {
            $unset_vars .= '$' . $var_name . ', ';
        }
        $unset_vars = rtrim($unset_vars, ', ') . ');';
        $new_clean_section .= $unset_vars;
    }

    // rewrite the file with updated clean section
    $new_content = $no_clean_content . "\n" . $new_clean_section . "\n";
    file_put_contents($file, $new_content);
}

scan_test_dir(__DIR__ . '/../tests');
