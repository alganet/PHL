--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test compilation error with long string to cover uncovered lines in compile.c

--FILE--
<?php
// Long string with many variables to trigger allocation failure in GenStateNewStrObj
$a = "test";
$b = "test";
$c = "test";
$d = "test";
$e = "test";
$f = "test";
$g = "test";
$h = "test";
$i = "test";
$j = "test";
$k = "test";
$l = "test";
$m = "test";
$n = "test";
$o = "test";
$p = "test";
$q = "test";
$r = "test";
$s = "test";
$t = "test";
$u = "test";
$v = "test";
$w = "test";
$x = "test";
$y = "test";
$z = "test";
$aa = "test";
$bb = "test";
$cc = "test";
$dd = "test";
$ee = "test";
$ff = "test";
$gg = "test";
$hh = "test";
$ii = "test";
$jj = "test";
$kk = "test";
$ll = "test";
$mm = "test";
$nn = "test";
$oo = "test";
$pp = "test";
$qq = "test";
$rr = "test";
$ss = "test";
$tt = "test";
$uu = "test";
$vv = "test";
$ww = "test";
$xx = "test";
$yy = "test";
$zz = "test";
echo "$a $b $c $d $e $f $g $h $i $j $k $l $m $n $o $p $q $r $s $t $u $v $w $x $y $z $aa $bb $cc $dd $ee $ff $gg $hh $ii $jj $kk $ll $mm $nn $oo $pp $qq $rr $ss $tt $uu $vv $ww $xx $yy $zz";
?>
--EXPECT--
test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test test
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, $n, $o, $p, $q, $r, $s, $t, $u, $v, $w, $x, $y, $z, $aa, $bb, $cc, $dd, $ee, $ff, $gg, $hh, $ii, $jj, $kk, $ll, $mm, $nn, $oo, $pp, $qq, $rr, $ss, $tt, $uu, $vv, $ww, $xx, $yy, $zz);
