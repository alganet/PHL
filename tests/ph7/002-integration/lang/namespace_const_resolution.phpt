--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unqualified constant resolves against the namespace of its SOURCE: import, then ns, then global
--FILE--
<?php
namespace Other;

const KAY = 'Other\KAY';

namespace B;

define('KAY', 'global KAY');
define('ONLY_GLOBAL', 'global only');

const KAY = 'B\KAY';
const PHP_EOL = 'B\PHP_EOL';   // a namespaced constant shadows even a builtin name

// current namespace WINS over the global constant of the same short name…
echo KAY, '|', \KAY, "\n";
// (the global PHP_EOL's VALUE is platform-dependent — assert only that it is
// still the builtin one, not the namespaced shadow)
echo PHP_EOL, '|', \PHP_EOL === 'B\PHP_EOL' ? 'shadowed' : 'global intact', "\n";
// …and the global one is still what an unmatched name falls back to.
echo ONLY_GLOBAL, "\n";

function reader() { return KAY; }
class Holder {
    const X = KAY;
    public $p = KAY;
    public static $s = KAY;
    public function m($x = KAY) { return $x . '/' . KAY; }
}

namespace C;

const KAY = 'C\KAY';

// The namespace that counts is the one the SOURCE sits in, not the one running:
// every one of these was compiled in B.
echo \B\reader(), "\n";
echo \B\Holder::X, '|', (new \B\Holder)->p, '|', \B\Holder::$s, "\n";
echo (new \B\Holder)->m(), "\n";

// An unresolved name is named the way php names it — the name looked up FIRST.
try { echo NOPE; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { echo \NOPE2; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
try { echo A\NOPE3; } catch (\Error $e) { echo $e->getMessage(), "\n"; }

namespace D;

use const Other\KAY;
use const Other\MISSING;

// An import wins over the global constant of the same short name (declaring a
// local `const KAY` here would be php's import/declaration collision instead)…
echo KAY, "\n";
// …and it has NO global fallback: `MISSING` is an Error even though a global
// constant of that short name exists.
define('MISSING', 'global MISSING');
try { echo MISSING; } catch (\Error $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
B\KAY|global KAY
B\PHP_EOL|global intact
global only
B\KAY
B\KAY|B\KAY|B\KAY
B\KAY/B\KAY
Undefined constant "C\NOPE"
Undefined constant "NOPE2"
Undefined constant "C\A\NOPE3"
Other\KAY
Undefined constant "Other\MISSING"
--CLEAN--
<?php
