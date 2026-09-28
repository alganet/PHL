--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An inherited method that satisfies a trait's abstract must still be compatible with it
--DESCRIPTION--
php names the class that PROVIDES the method first and the trait that required
it second -- the reverse of an ordinary override refusal, because it is the
requirement that is being violated, not the parent's declaration.
--FILE--
<?php
trait TaiT { abstract public function need(): string; }
class TaiP { public function need(): int { return 1; } }
class TaiC extends TaiP { use TaiT; }
echo "unreachable\n";
?>
--EXPECTF--
%ADeclaration of TaiP::need()%Amust be compatible with TaiT::need()%A
--CLEAN--
<?php
