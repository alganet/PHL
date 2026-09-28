--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An interface method may not be final
--DESCRIPTION--
An interface method is a signature every implementor must provide, so `final` --
and a written-out `abstract`, which it already is -- are both refused, each
naming the method. PHL used to refuse the whole member with
"Expecting method signature or constant declaration".
--FILE--
<?php
interface ImfI { final public function f(); }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Interface method ImfI::f() must not be final %s
--CLEAN--
<?php
