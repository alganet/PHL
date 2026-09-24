--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var_export() writes nothing when a property hook throws part-way through
--FILE--
<?php
class VehThrower {
    public $before = 1;
    public int $boom { get => throw new LogicException('boom'); }
    public $after = 2;
}

/* the object being exported is the one whose hook throws */
try { var_export(new VehThrower); echo "NOT REACHED\n"; }
catch (Throwable $e) { echo 'caught ', $e->getMessage(), "\n"; }

/* the $return = true form answers nothing either */
try { $s = var_export(new VehThrower, true); echo 'NOT REACHED: ', $s, "\n"; }
catch (Throwable $e) { echo 'caught ', $e->getMessage(), "\n"; }

/* nested inside an ARRAY */
try { var_export([1, new VehThrower, 'tail']); echo "NOT REACHED\n"; }
catch (Throwable $e) { echo 'caught ', $e->getMessage(), "\n"; }

/* a hook that does NOT throw still exports */
class VehFine { public int $v = 3 { get => $this->v * 2; } }
var_export(new VehFine);
echo "\n";
?>
--EXPECT--
caught boom
caught boom
caught boom
\VehFine::__set_state(array(
   'v' => 6,
))
--CLEAN--
<?php
