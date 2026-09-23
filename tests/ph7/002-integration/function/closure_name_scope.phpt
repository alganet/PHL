--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A closure's php name words the ENCLOSING scope, not just the file
--FILE--
<?php
// php 8.4 names a closure `{closure:SCOPE:LINE}` where SCOPE is the enclosing
// function — the FILE only at top level. It is a COMPILE-time fact, so
// __FUNCTION__ inside the body reports the very same text, and __METHOD__
// reports it unqualified (the name already carries the declaring class).
namespace ClosureScope;

class Host
{
	public function method()
	{
		return function () { return [__FUNCTION__, __METHOD__]; };
	}
	public static function stat()
	{
		return fn() => __FUNCTION__;
	}
	public function nested()
	{
		return function () { return function () { return __FUNCTION__; }; };
	}
}
trait Mixin
{
	public function fromTrait()
	{
		return function () { return __FUNCTION__; };
	}
}
class User { use Mixin; }
function plain()
{
	return function () { return __FUNCTION__; };
}

print_r(((new Host)->method())());
echo (Host::stat())(), "\n";
echo (namespace\plain())(), "\n";
echo ((new User)->fromTrait())(), "\n";
// An enclosing CLOSURE contributes its whole name, with no parens and no class.
echo (((new Host)->nested())())(), "\n";
// Top level: the file, and every reader agrees on it.
$top = function () { return __FUNCTION__; };
echo $top(), "\n";
echo (new \ReflectionFunction($top))->getName(), "\n";
$scoped = (new Host)->method();
echo (new \ReflectionFunction($scoped))->getName(), "\n";
\is_callable($scoped, false, $named);
echo $named, "\n";
?>
--EXPECTF--
Array
(
    [0] => {closure:ClosureScope\Host::method():12}
    [1] => {closure:ClosureScope\Host::method():12}
)
{closure:ClosureScope\Host::stat():16}
{closure:ClosureScope\plain():33}
{closure:ClosureScope\Mixin::fromTrait():27}
{closure:{closure:ClosureScope\Host::nested():20}:20}
{closure:%s:43}
{closure:%s:43}
{closure:ClosureScope\Host::method():12}
{closure:ClosureScope\Host::method():12}
--CLEAN--
<?php
