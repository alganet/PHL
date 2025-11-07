--TEST--
Simple function call
--FILE--
<?php
function add($x, $y) {
    return $x + $y;
}
echo add(3, 4);
?>
--EXPECT--
7