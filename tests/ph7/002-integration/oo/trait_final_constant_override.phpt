--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait's `final const` may not be overridden, and the refusal names the COMPOSING class
--FILE--
<?php
trait TfcT { final const K = 't'; }
class TfcC { use TfcT; }
class TfcD extends TfcC { const K = 'd'; }
echo "unreachable\n";
?>
--EXPECTF--
%ATfcD::K cannot override final constant TfcC::K%A
--CLEAN--
<?php
