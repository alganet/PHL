--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Four shapes real vendor code writes: an arrow function as a ternary branch, a class named Integer, a private abstract trait method, and a comment-only php chunk inside a block
--FILE--
<?php
// Four shapes real vendor code writes that this compiler refused.

// 1. An arrow function as a TERNARY branch: php's grammar ends the body at the
//    ':' that closes the enclosing '?', so the false branch is its own function.
$vcx_c = true;
$vcx_f = static fn ($v) => $v * 2;
$vcx_a = $vcx_c ? static fn ($v) => !$vcx_f($v) : static fn ($v) => false === $v;
var_dump($vcx_a(0), $vcx_a(1));
$vcx_b = false ? fn ($v) => 'T' : fn ($v) => 'F';
var_dump($vcx_b(1));
// ...and a ternary opened INSIDE the body still owns its own colon.
$vcx_d = fn ($v) => $v ? 'y' : 'n';
var_dump($vcx_d(1), $vcx_d(0));
$vcx_e = true ? fn ($v) => $v ? 1 : 2 : fn ($v) => 3;
var_dump($vcx_e(1), $vcx_e(0));
// '?:', '??' and '?->' are not ternary opens.
var_dump((fn ($v) => $v ?: 'd')(0), (fn ($v) => $v ?? 'd')(null));
$vcx_o = new stdClass();
$vcx_o->p = 'P';
var_dump((fn ($q) => $q?->p)($vcx_o));
var_dump(array_map(fn ($v) => $v ? 't' : 'f', [1, 0]));

// 2. Which words may NAME a class. php's scanner and its compiler disagree about
//    where the refusal comes from, and PHL's keyword set is not php's.
class Integer { public int $v = 1; }
class Boolean { public bool $v = true; }
class Double { public float $v = 1.5; }
var_dump((new Integer)->v, (new Boolean)->v, (new Double)->v);
// The words php REFUSES to name a class with are a compile-time fatal, so one
// per process: see class_name_reserved_word.phpt for that half.

// 3. A private abstract method is legal in a TRAIT (php 8.0) and nowhere else.
trait VcxT { abstract private function step(array $j): void; public function run() { $this->step([]); } }
class VcxC { use VcxT; private function step(array $j): void { echo "step\n"; } }
(new VcxC)->run();

// 4. A PHP chunk that holds only a COMMENT is still a chunk, and it must not end
//    the block it sits in -- every php template writes this.
?>
<?php if (true) { ?>
BRANCH-A
<?php } else { ?>
<?php // why this branch exists ?>
BRANCH-B
<?php } ?>
<?php if (false) { ?>
NOPE
<?php } else { ?>
<?php /* block comment only */ ?>
ELSE-TAKEN
<?php } ?>
<?php echo "end\n";
--EXPECT--
bool(true)
bool(false)
string(1) "F"
string(1) "y"
string(1) "n"
int(1)
int(2)
string(1) "d"
string(1) "d"
string(1) "P"
array(2) {
  [0]=>
  string(1) "t"
  [1]=>
  string(1) "f"
}
int(1)
bool(true)
float(1.5)
step
BRANCH-A
ELSE-TAKEN
end
