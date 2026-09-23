--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_ini_file() honors $scanner_mode and keeps an empty-value entry's NEXT line
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_ini_typed');
file_put_contents($fn, "a = yes\nb = 3\nc =\nd = after\n");
var_dump(parse_ini_file($fn, false, INI_SCANNER_TYPED));
var_dump(parse_ini_file($fn, false, INI_SCANNER_RAW));
set_error_handler(function ($no, $str) { echo "warn: ", $str, "\n"; return true; });
var_dump(parse_ini_file($fn, false, 9));
restore_error_handler();
unlink($fn);
?>
--EXPECT--
array(4) {
  ["a"]=>
  bool(true)
  ["b"]=>
  int(3)
  ["c"]=>
  string(0) ""
  ["d"]=>
  string(5) "after"
}
array(4) {
  ["a"]=>
  string(3) "yes"
  ["b"]=>
  string(1) "3"
  ["c"]=>
  string(0) ""
  ["d"]=>
  string(5) "after"
}
warn: Invalid scanner mode
bool(false)
--CLEAN--
<?php
unset($fn);
