--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: FILTER_SANITIZE_STRING/STRIPPED stay undefined, the filter itself is reachable (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php 8.1 deprecated the two CONSTANT names for filter id 513 ("use
 * htmlspecialchars() instead"); §10 rejects php's deprecated surface loudly, so
 * this engine does not define them. What php deprecated is the name, not the
 * filter, so the filter is here and answers exactly what php's does — by id,
 * and (once filter_id() exists) by name. See the zend half. */
var_dump(defined('FILTER_SANITIZE_STRING'), defined('FILTER_SANITIZE_STRIPPED'));
try {
    var_dump(FILTER_SANITIZE_STRING);
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
var_dump(filter_var("<b>a</b> 'q'", 513));
var_dump(filter_var("<b>a</b> 'q'", 513, FILTER_FLAG_NO_ENCODE_QUOTES));
?>
--EXPECT--
bool(false)
bool(false)
Error: Undefined constant "FILTER_SANITIZE_STRING"
string(13) "a &#39;q&#39;"
string(5) "a 'q'"
--CLEAN--
<?php
