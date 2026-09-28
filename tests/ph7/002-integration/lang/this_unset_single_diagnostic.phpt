--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`unset($this)` reports php's fatal and nothing else
--DESCRIPTION--
php refuses `unset($this)` where it is written and stops compiling. This
generator carries on to a budget of fifteen errors, and it used to resume ON the
operand it had just refused -- re-reading the closing ')' as a statement of its
own and printing an "Unmatched ')'" under the fatal. The cursor now leaves past
the whole construct, so the one sentence php prints is the one sentence here.
--FILE--
<?php
class TusC { public function f() { unset($this); } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot unset $this %s
--CLEAN--
<?php
