--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Heredoc with complex interpolation patterns and edge cases
--FILE--
<?php
$name = 'World';
$numbers = array(1, 2, 3);
$value = 'test_value';

// Test heredoc with various interpolation patterns
echo <<<EOT
Simple: Hello {$name}!
Array access: {$numbers[1]}
Variable value: {$value}
Complex nesting: {$numbers[0]}{$numbers[1]}{$numbers[2]}
EOT;

// Test heredoc with empty braces (should not interpolate)
echo <<<EOT2
Empty braces: {}
Single dollar: $
EOT2;

// Test heredoc with function call in braces (should not interpolate)
echo <<<EOT3
Function call: {strtoupper('hello')}
EOT3;

?>
--EXPECT--
Simple: Hello World!
Array access: 2
Variable value: test_value
Complex nesting: 123Empty braces: {}
Single dollar: $Function call: {strtoupper('hello')}
--CLEAN--
<?php
unset($name, $numbers, $value);
