--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Test parsing of various language constructs and keywords
--FILE--
<?php
// Test language construct parsing (covers PH7_IsLangConstruct paths)
$a = 1;
$b = 2;

// Test echo construct
echo "Test: ", $a + $b, "\n";

// Test print construct
print "Print: " . ($a * $b) . "\n";

// Test isset in expression context
$result = isset($a) && isset($b);
var_dump($result); // true

// Test empty in expression
$empty_var = null;
$result2 = empty($empty_var) || !empty($a);
var_dump($result2); // true

// Test include construct (with variable)
$filename = 'nonexistent.php';
$result3 = @include $filename; // Should be false/null
var_dump($result3 === false); // true (include returns false on failure)

// Test eval construct
$result4 = eval('return $a + $b;');
var_dump($result4); // 3
?>
--EXPECTF--
Test: 3
Print: 2
bool(TRUE)
bool(TRUE)
Warning: include(): IO error while importing: 'nonexistent.php' in %s on line %d
bool(TRUE)
int(3)
--CLEAN--
<?php
unset($a, $b, $result, $empty_var, $result2, $filename, $result3, $result4);
