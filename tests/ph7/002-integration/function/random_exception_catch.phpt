--TEST--
Random\RandomException is catchable as \Exception and by FQN
--SKIPIF--
<?php if (function_exists('xdebug_info')) echo "skip xdebug annotates thrown exceptions with a dynamic \$xdebug_message property that Random\\RandomException forbids"; ?>
--FILE--
<?php
try { throw new \Random\RandomException('boom'); }
catch (\Exception $x) { echo "caught-as-exception: ", $x->getMessage(), "\n"; }
try { throw new \Random\RandomException('boom2'); }
catch (\Random\RandomException $x) { echo "caught-as-fqn: ", $x->getMessage(), "\n"; }
--EXPECT--
caught-as-exception: boom
caught-as-fqn: boom2
