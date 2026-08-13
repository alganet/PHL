--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constants: initializers referencing other constants (self::/parent::/cross-class), lazily evaluated
--FILE--
<?php
class CclBase {
    const A = 5;
    const B = self::A + 1;
    private const SECRET = 3;
    const FROM_SECRET = self::SECRET * 2; // own-class private access is in scope
    public $p = self::A + 10;
}
class CclChild extends CclBase {
    const C = parent::B * 2;
}
class CclForward {
    const X = CclLater::Y + 1; // cross-class reference, target declared later
}
class CclLater { const Y = 41; }
interface CclIface { const V = 9; }
class CclImpl implements CclIface { const W = self::V + 1; }

echo CclBase::B, "\n";
echo CclBase::FROM_SECRET, "\n";
echo CclChild::C, "\n";
echo CclForward::X, "\n";
echo CclImpl::W, "\n";
$o = new CclBase();
echo $o->p, "\n";
class CclSelfRef { const LOOP = self::LOOP; }
try {
    echo CclSelfRef::LOOP;
} catch (Error $e) {
    // php words the reference as written ("self::LOOP"), PHL as resolved
    // ("CclSelfRef::LOOP") — assert the shared core.
    echo get_class($e), ": ",
        str_contains($e->getMessage(), "self-referencing constant") ? "self-ref" : "?", "\n";
}
?>
--EXPECT--
6
6
12
42
10
15
Error: self-ref
--CLEAN--
<?php
unset($o);
