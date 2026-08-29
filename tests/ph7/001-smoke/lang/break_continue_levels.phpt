--TEST--
Break and continue with levels

--FILE--
<?php
$i = 0;
$j = 0;
while ($i < 3) {
    $i++;
    while ($j < 3) {
        $j++;
        if ($j == 2) {
            break 2;
        }
        echo "inner: $j ";
    }
    echo "outer: $i ";
}
echo "final: $i,$j";
?>
--EXPECT--
inner: 1 final: 1,2
--CLEAN--
<?php
unset($i, $j);
