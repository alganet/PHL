--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionClass constructor errors
--FILE--
<?php
try {
    new ReflectionClass('ReflErrNoSuchClass');
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionClass(42);
} catch (ReflectionException $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionClass(array());
} catch (TypeError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
try {
    new ReflectionObject('notanobject');
} catch (TypeError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
echo ReflectionException::class, ' extends ', get_parent_class(new ReflectionException('x')), "\n";
$r = new ReflectionClass('Exception');
echo $r instanceof Reflector ? 'reflector' : 'not-reflector', "\n";
echo $r instanceof Stringable ? 'stringable' : 'not-stringable', "\n";
?>
--EXPECT--
ReflectionException: Class "ReflErrNoSuchClass" does not exist
ReflectionException: Class "42" does not exist
TypeError: ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object|string, array given
TypeError: ReflectionObject::__construct(): Argument #1 ($object) must be of type object, string given
ReflectionException extends Exception
reflector
stringable
