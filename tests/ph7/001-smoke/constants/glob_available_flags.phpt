--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: GLOB_AVAILABLE_FLAGS constant and glob()'s no-match silence
--FILE--
<?php
echo "GLOB_AVAILABLE_FLAGS=", GLOB_AVAILABLE_FLAGS, "\n";
var_dump(GLOB_AVAILABLE_FLAGS ===
    (GLOB_BRACE | GLOB_MARK | GLOB_NOSORT | GLOB_NOCHECK | GLOB_NOESCAPE | GLOB_ERR | GLOB_ONLYDIR));
// The whole mask is accepted; one bit past it is the warning + FALSE.
set_error_handler(function ($no, $str) {
    // Handlers see '@'-suppressed diagnostics too; consult the mask like php asks.
    if (!(error_reporting() & $no)) { return true; }
    echo "warn: ", $str, "\n";
    return true;
});
var_dump(is_array(glob("/nonexistent-dir-xyz/*", GLOB_AVAILABLE_FLAGS & ~GLOB_NOCHECK)));
var_dump(glob("*", GLOB_AVAILABLE_FLAGS + 1));
restore_error_handler();
// A directory that cannot be opened is zero matches, in SILENCE — not FALSE
// (GLOB_ERR included: that flag is about errors during the walk, not the path).
var_dump(glob("/nonexistent-dir-xyz/*"));
var_dump(glob("/nonexistent-dir-xyz/*", GLOB_ERR));
var_dump(glob("/nonexistent-dir-xyz/*", GLOB_NOCHECK));
?>
--EXPECT--
GLOB_AVAILABLE_FLAGS=1073746108
bool(true)
bool(true)
warn: glob(): At least one of the passed flags is invalid or not supported on this platform
bool(false)
array(0) {
}
array(0) {
}
array(1) {
  [0]=>
  string(22) "/nonexistent-dir-xyz/*"
}
--CLEAN--
<?php
