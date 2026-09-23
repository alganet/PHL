--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Re-assigning $this is refused where it is written
--FILE--
<?php
// php decides this at COMPILE time, so the refusal arrives even for a method
// nobody calls. PHL used to perform it: $this became an int for the rest of the
// call and every later $this->x failed somewhere else entirely.
class ThisTgtA { function r() { $this = 5; } }
?>
--EXPECTF--
%ACannot re-assign $this%A
