--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure whose body names $this refuses to be unbound; one that never names it drops $this
--FILE--
<?php
class A {
  public $p = 1;
  function m() { return 1; }
  function all() {
    return [
      'read' => function() { return $this; },
      'prop' => function() { return $this->p; },
      'nullsafe' => function() { return $this?->p; },
      'call' => function() { return $this->m(); },
      'isset' => function() { return isset($this); },
      'arg' => function() { return get_class($this); },
      'interpolated' => function() { return "$this->p"; },
      'deadcode' => function() { if (0) { return $this; } return 1; },
      'arrow' => fn() => $this,
      'innerClosure' => function() { return function() { return $this; }; },
      'innerArrow' => function() { return fn() => $this; },
      'staticCall' => function() { return static::class; },
      'varVar' => function() { $n = 'this'; return $$n; },
      'eval' => function() { return eval('return $this;'); },
      'none' => function() { return 1; },
    ];
  }
  static function fromStatic() { return function() { return isset($this) ? 'this' : 'none'; }; }
}
set_error_handler(function($n, $s) { echo "[warning: $s] "; return true; });
foreach ((new A)->all() as $k => $cl) {
  echo str_pad($k, 13);
  $b = Closure::bind($cl, null, 'static');
  echo $b === null ? "null" : "closure, this " . var_export((new ReflectionFunction($b))->getClosureThis(), true), "
";
}
echo "bindTo: "; var_dump((new A)->all()['read']->bindTo(null));
$g = function() { return isset($this) ? $this->p : 'none'; };
echo "global: "; var_dump(Closure::bind($g, null, null)());
$bound = Closure::bind($g, new A, 'A');
echo "rebound to null: "; var_dump(Closure::bind($bound, null, 'A'));
echo "rebound to object: "; var_dump(Closure::bind($bound, new A, 'A')());
echo "static method: "; var_dump(Closure::bind(A::fromStatic(), null, 'A')());
--EXPECT--
read         [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
prop         [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
nullsafe     [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
call         [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
isset        [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
arg          [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
interpolated [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
deadcode     [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
arrow        [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] null
innerClosure closure, this NULL
innerArrow   closure, this NULL
staticCall   closure, this NULL
varVar       closure, this NULL
eval         closure, this NULL
none         closure, this NULL
bindTo: [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] NULL
global: string(4) "none"
rebound to null: [warning: Cannot unbind $this of closure using $this, this will be an error in PHP 9] NULL
rebound to object: int(1)
static method: string(4) "none"
