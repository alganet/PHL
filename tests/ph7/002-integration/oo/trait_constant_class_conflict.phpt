--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The composing class's OWN constant conflicts with a trait's the same way
--FILE--
<?php
trait TcdT { const K = 't'; }
class TcdC { use TcdT; const K = 'c'; }
echo "unreachable\n";
?>
--EXPECTF--
%ATcdC and TcdT define the same constant (K) in the composition of TcdC. However, the definition differs and is considered incompatible. Class was composed%A
--CLEAN--
<?php
