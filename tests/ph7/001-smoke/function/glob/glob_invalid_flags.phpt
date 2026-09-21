--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
glob() rejects a flag outside GLOB_AVAILABLE_FLAGS with a warning and false
--DESCRIPTION--
The handler reports the message BODY only: PHL raises this from the embedded prelude
via trigger_error, so its errno is E_USER_WARNING (512) where php's is E_WARNING (2) —
the shared embedded-builtin errno residual, not a glob() property.
--FILE--
<?php
$gif_dir = sys_get_temp_dir() . '/phl_glob_flags_' . getmypid();
mkdir($gif_dir);
touch($gif_dir . '/a.txt');
mkdir($gif_dir . '/sub');

set_error_handler(function ($gif_no, $gif_msg) { echo "WARN: $gif_msg\n"; return true; });

// supported flags keep working
var_dump(glob($gif_dir . '/*', GLOB_ONLYDIR));
var_dump(count(glob($gif_dir . '/*')));
var_dump(glob($gif_dir . '/nothing*', GLOB_NOCHECK));

// a bit outside the available set is refused before anything is read. 1/2/64 are
// exactly the values a script written against PHL's old private GLOB_* ladder
// would pass.
foreach ([1, 2, 64, 1024, -1] as $gif_bad) {
    var_dump(glob($gif_dir . '/*', $gif_bad));
}
restore_error_handler();
unlink($gif_dir . '/a.txt');
rmdir($gif_dir . '/sub');
rmdir($gif_dir);
?>
--EXPECTF--
array(1) {
  [0]=>
  string(%d) "%s/sub"
}
int(2)
array(1) {
  [0]=>
  string(%d) "%s/nothing*"
}
WARN: glob(): At least one of the passed flags is invalid or not supported on this platform
bool(false)
WARN: glob(): At least one of the passed flags is invalid or not supported on this platform
bool(false)
WARN: glob(): At least one of the passed flags is invalid or not supported on this platform
bool(false)
WARN: glob(): At least one of the passed flags is invalid or not supported on this platform
bool(false)
WARN: glob(): At least one of the passed flags is invalid or not supported on this platform
bool(false)
--CLEAN--
<?php
