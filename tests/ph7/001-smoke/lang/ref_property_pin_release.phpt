--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reference-bound property gives its hold back when it goes
--FILE--
<?php
class RefPinCell { public $p; }

// Two properties on one slot: the value stands until the last of them goes.
$rpy = 1;
$rpa = new RefPinCell;
$rpb = new RefPinCell;
$rpa->p = &$rpy;
$rpb->p = &$rpy;
unset($rpy);
var_dump($rpa->p, $rpb->p);
unset($rpa);
var_dump($rpb->p);
$rpb->p = 7;
var_dump($rpb->p);

// A clone shares the slot, and holds it in its own right.
$rpx = 1;
$rpo = new RefPinCell;
$rpo->p = &$rpx;
$rpc = clone $rpo;
$rpx = 5;
var_dump($rpo->p, $rpc->p);
unset($rpo, $rpx);
var_dump($rpc->p);

// Re-binding a bound property lets the first slot go.
$rpm = 1;
$rpn = 2;
$rpd = new RefPinCell;
$rpd->p = &$rpm;
$rpd->p = &$rpn;
$rpd->p = 8;
var_dump($rpm, $rpn);

// A property bound to an element keeps it after the array is gone.
$rparr = [1];
$rpe = new RefPinCell;
$rpe->p = &$rparr[0];
unset($rparr);
var_dump($rpe->p);
$rpe->p = 4;
var_dump($rpe->p);
?>
--EXPECT--
int(1)
int(1)
int(1)
int(7)
int(5)
int(5)
int(5)
int(1)
int(8)
int(1)
int(4)
--CLEAN--
<?php
unset($rpb, $rpc, $rpd, $rpm, $rpn, $rpe);
