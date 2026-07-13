--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
#[Deprecated] attribute drives isDeprecated()
--FILE--
<?php
#[Deprecated("use other", "1.2")]
function reflDepFn() {}
class ReflDepC { #[Deprecated] const OLD = 1; #[Deprecated] public function om() {} public function nm() {} }
echo (new ReflectionFunction('reflDepFn'))->isDeprecated() ? 'y' : 'n', "\n";
echo (new ReflectionClassConstant('ReflDepC', 'OLD'))->isDeprecated() ? 'y' : 'n', "\n";
echo (new ReflectionMethod('ReflDepC', 'om'))->isDeprecated() ? 'y' : 'n', "\n";
echo (new ReflectionMethod('ReflDepC', 'nm'))->isDeprecated() ? 'y' : 'n', "\n";
$i = (new ReflectionFunction('reflDepFn'))->getAttributes()[0]->newInstance();
echo get_class($i), ':', $i->message, ':', $i->since, "\n";
echo (new ReflectionFunction('strlen'))->isDeprecated() ? 'y' : 'n', "\n";
--EXPECT--
y
y
y
n
Deprecated:use other:1.2
n
