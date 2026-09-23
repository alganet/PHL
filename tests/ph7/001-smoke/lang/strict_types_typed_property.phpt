--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strict_types refuses a coerced value stored into a typed property
--FILE--
<?php
declare(strict_types=1);
class StpBag {
    public int $i = 0;
    public float $f = 0.0;
    public string $s = '';
    public bool $b = false;
    public ?int $ni = null;
    public int|string $u = 0;
    public mixed $m = null;
    public array $a = [];
    public static int $st = 0;
    public function inside() { $this->i = "7"; }
}
function stpTry(callable $f) {
    try { $r = $f(); echo "ok "; var_dump($r); }
    catch (\TypeError $e) { echo $e->getMessage(), "\n"; }
}
$p = new StpBag;

// no coercion into a scalar property
stpTry(function () use ($p) { $p->i = "5"; return $p->i; });
stpTry(function () use ($p) { $p->i = 5.0; return $p->i; });
stpTry(function () use ($p) { $p->i = true; return $p->i; });
stpTry(function () use ($p) { $p->f = "5.5"; return $p->f; });
stpTry(function () use ($p) { $p->s = 5; return $p->s; });
stpTry(function () use ($p) { $p->b = 1; return $p->b; });
stpTry(function () use ($p) { $p->ni = "5"; return $p->ni; });
stpTry(function () use ($p) { $p->a = "x"; return $p->a; });

// unions take the same rule
stpTry(function () use ($p) { $p->u = 1.5; return $p->u; });
stpTry(function () use ($p) { $p->u = true; return $p->u; });

// what strict mode still accepts: the exact type, int -> float, null, mixed
stpTry(function () use ($p) { $p->i = 5; return $p->i; });
stpTry(function () use ($p) { $p->f = 5; return $p->f; });
stpTry(function () use ($p) { $p->s = "x"; return $p->s; });
stpTry(function () use ($p) { $p->b = true; return $p->b; });
stpTry(function () use ($p) { $p->ni = null; return $p->ni; });
stpTry(function () use ($p) { $p->u = 3; return $p->u; });
stpTry(function () use ($p) { $p->m = "anything"; return $p->m; });

// the rule follows the ASSIGNMENT, wherever it is written
stpTry(function () use ($p) { $p->inside(); return $p->i; });
stpTry(function () { StpBag::$st = "9"; return StpBag::$st; });
?>
--EXPECT--
Cannot assign string to property StpBag::$i of type int
Cannot assign float to property StpBag::$i of type int
Cannot assign true to property StpBag::$i of type int
Cannot assign string to property StpBag::$f of type float
Cannot assign int to property StpBag::$s of type string
Cannot assign int to property StpBag::$b of type bool
Cannot assign string to property StpBag::$ni of type ?int
Cannot assign string to property StpBag::$a of type array
Cannot assign float to property StpBag::$u of type string|int
Cannot assign true to property StpBag::$u of type string|int
ok int(5)
ok float(5)
ok string(1) "x"
ok bool(true)
ok NULL
ok int(3)
ok string(8) "anything"
Cannot assign string to property StpBag::$i of type int
Cannot assign string to property StpBag::$st of type int
