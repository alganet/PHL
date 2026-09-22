--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Group use declarations (php 7.0): use A\{Cee, Dee as D2, function f, const K}
--FILE--
<?php
namespace A\Sub;

class Eee { const K = 'A\Sub\Eee'; public function id() { return static::K; } }
interface Iface {}

namespace A;

class Cee { const K = 'A\Cee'; }
class Dee { const K = 'A\Dee'; }
function eff() { return 'A\eff'; }
const KAY = 'A\KAY';

namespace B;

// One prefix, several members: plain, aliased, multi-segment, trailing comma.
use A\{
    Cee,
    Dee as D2,
    Sub\Eee as E,
};
use A\Sub\{Iface};
// A typed group applies its kind to every member...
use function A\{eff, eff as eff2};
use const A\{KAY, KAY as KAY2};

echo Cee::K, '|', D2::K, '|', E::K, "\n";
echo eff(), '|', eff2(), '|', KAY, '|', KAY2, "\n";

class Impl extends E implements Iface {}
echo (new Impl())->id(), "\n";
$anon = new class extends E {};   // deferred chunk: the imports must reach it
echo get_parent_class($anon), "\n";

// ...and a group member resolves case-insensitively, like any other import.
echo CEE::K, '|', EFF(), "\n";

namespace C;

// An UNTYPED group may qualify individual members instead.
use A\{Cee, function eff, const KAY};

echo Cee::K, '|', eff(), '|', KAY, "\n";
?>
--EXPECT--
A\Cee|A\Dee|A\Sub\Eee
A\eff|A\eff|A\KAY|A\KAY
A\Sub\Eee
A\Sub\Eee
A\Cee|A\eff
A\Cee|A\eff|A\KAY
--CLEAN--
<?php
