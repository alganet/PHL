--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sizeof() names itself in its ValueError, and json_validate() screens its $flags
--FILE--
<?php
function zppMiscCase(string $label, callable $fn): void {
    try {
        $out = var_export($fn(), true);
    } catch (\Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo $label, ' => ', $out, "\n";
}
// php words a diagnostic with the name the call was WRITTEN with.
zppMiscCase('sizeof', fn() => sizeof([1], 3));
zppMiscCase('count', fn() => count([1], 3));
// json_validate() accepts exactly one flag; every other bit is a ValueError, and
// the failed call leaves json_last_error() untouched.
zppMiscCase('flags 7', fn() => json_validate('1', 512, 7));
zppMiscCase('flags 2', fn() => json_validate('1', 512, 2));
zppMiscCase('flags IGNORE', fn() => json_validate('1', 512, JSON_INVALID_UTF8_IGNORE));
zppMiscCase('flags 0', fn() => json_validate('1', 512, 0));
zppMiscCase('no flags', fn() => json_validate('1'));
echo json_last_error(), "\n";
--EXPECT--
sizeof => ValueError: sizeof(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE
count => ValueError: count(): Argument #2 ($mode) must be either COUNT_NORMAL or COUNT_RECURSIVE
flags 7 => ValueError: json_validate(): Argument #3 ($flags) must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)
flags 2 => ValueError: json_validate(): Argument #3 ($flags) must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)
flags IGNORE => true
flags 0 => true
no flags => true
0
--CLEAN--
<?php
