--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A strict_types TypeError on a constructor argument unwinds the NEW
--FILE--
<?php
declare(strict_types=1);
class ExcStrictCtor_T {
    public function __construct(int $n) {}
}
$o = null;
try {
    $o = new ExcStrictCtor_T("not-an-int");
    echo "after\n"; // must NOT run
} catch (TypeError $e) {
    echo "type error caught\n";
}
echo ($o === null ? "o-is-null" : "o-set"), "\n";
?>
--EXPECT--
type error caught
o-is-null
--CLEAN--
<?php
unset($o);
