--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property of abstract-class type: accepts a subclass, rejects an unrelated class (TypeError)
--FILE--
<?php
abstract class IabBase {}
class IabChild extends IabBase {}
class IabOther {}
class IabHolder { public IabBase $a; }
$h = new IabHolder();
$h->a = new IabChild();
echo "ok ", get_class($h->a), "\n";
try { $h->a = new IabOther(); }
catch (TypeError $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "after\n";
?>
--EXPECT--
ok IabChild
caught: Cannot assign IabOther to property IabHolder::$a of type IabBase
after
--CLEAN--
<?php
unset($h);
