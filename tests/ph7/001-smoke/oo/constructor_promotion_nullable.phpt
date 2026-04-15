--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: nullable typed promoted property
--FILE--
<?php
class CppNull {
    public function __construct(public ?string $label) {}
}
$a = new CppNull(null);
echo is_null($a->label) ? "null" : $a->label, "\n";
$b = new CppNull("hi");
echo $b->label, "\n";
$b->label = null;
echo is_null($b->label) ? "null" : $b->label, "\n";
?>
--EXPECT--
null
hi
null
--CLEAN--
<?php
unset($a, $b);
