--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What the import/declaration collision rule does NOT refuse (self-imports, other kinds, scopes)
--FILE--
<?php
namespace B;

// A SELF-import names the declaration itself, so it is a no-op, not a collision —
// in either order, and whatever case the import is written in.
use B\Cee;
use function B\eff;
use const B\KAY;

class Cee { const K = 'B\Cee'; }
function eff() { return 'B\eff'; }
const KAY = 'B\KAY';

echo Cee::K, '|', eff(), '|', KAY, "\n";

namespace C;

// The three kinds are independent name spaces: a CLASS import never blocks a
// function or const declaration, and a FUNCTION import never blocks a class.
use A\eff;
use function A\Cee;

class Cee { const K = 'C\Cee'; }
function eff() { return 'C\eff'; }

echo Cee::K, '|', eff(), "\n";

namespace D;

// A method, a class CONSTANT and an anonymous class are not global declarations,
// so an import of the same short name leaves them alone.
use A\Dee;

class Holder {
    const Dee = 'const Dee';
    public function Dee() { return 'method Dee'; }
}
$anon = new class { public function w() { return 'anon'; } };
echo Holder::Dee, '|', (new Holder)->Dee(), '|', $anon->w(), "\n";

namespace E;

// The rule is per COMPILE UNIT and per name: a class declared in namespace D
// above does not collide with the same short name imported here.
use A\Holder;
use A\Cee as Alias;

class Other { const K = 'E\Other'; }
echo Other::K, "\n";
?>
--EXPECT--
B\Cee|B\eff|B\KAY
C\Cee|C\eff
const Dee|method Dee|anon
E\Other
--CLEAN--
<?php
