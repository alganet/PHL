--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: object subject uses strict identity (===) comparison
--FILE--
<?php
class MatchObjSubjectTag {
    public $name;
    public function __construct($n) { $this->name = $n; }
}
$a = new MatchObjSubjectTag('x');
$b = new MatchObjSubjectTag('y');
$a2 = new MatchObjSubjectTag('x');
echo match ($a) { $b => 'is b', $a => 'is a', $a2 => 'is a2' }, "\n";
echo match ($a2) { $a => 'is a', $a2 => 'is a2', $b => 'is b' }, "\n";
echo match ($b) { $a => 'is a', $a2 => 'is a2', $b => 'is b' }, "\n";
?>
--EXPECT--
is a
is a2
is b
--CLEAN--
<?php
