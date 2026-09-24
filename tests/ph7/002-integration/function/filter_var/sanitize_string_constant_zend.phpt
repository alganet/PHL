--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: FILTER_SANITIZE_STRING/STRIPPED are defined and deprecated (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php defines both names and deprecates their USE, then applies the filter.
 * See the PHL half. */
var_dump(defined('FILTER_SANITIZE_STRING'), defined('FILTER_SANITIZE_STRIPPED'));
set_error_handler(function ($no, $str) { echo "E_DEPRECATED: $str\n"; return true; });
var_dump(FILTER_SANITIZE_STRING === 513, FILTER_SANITIZE_STRIPPED === 513);
restore_error_handler();
var_dump(filter_var("<b>a</b> 'q'", 513));
var_dump(filter_var("<b>a</b> 'q'", 513, FILTER_FLAG_NO_ENCODE_QUOTES));
?>
--EXPECT--
bool(true)
bool(true)
E_DEPRECATED: Constant FILTER_SANITIZE_STRING is deprecated since 8.1, use htmlspecialchars() instead
E_DEPRECATED: Constant FILTER_SANITIZE_STRIPPED is deprecated since 8.1, use htmlspecialchars() instead
bool(true)
bool(true)
string(13) "a &#39;q&#39;"
string(5) "a 'q'"
--CLEAN--
<?php
