--TEST--
Random\RandomException is registered (PHP 8.2+)
--FILE--
<?php
echo class_exists('Random\\RandomException') ? "exists\n" : "missing\n";
echo get_parent_class('Random\\RandomException'), "\n";      // Exception
$e = new \Random\RandomException('x');
echo ($e instanceof \Exception) ? "is-exception\n" : "no\n";
--EXPECT--
exists
Exception
is-exception
