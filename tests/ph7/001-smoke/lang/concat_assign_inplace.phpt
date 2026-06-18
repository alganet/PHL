--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
.= appends in place (amortized O(1)) while preserving exact concat semantics
--DESCRIPTION--
OP_CAT_STORE gained an in-place fast path that appends directly to the lvalue's
owned buffer (turning $s .= ... from O(n^2) into amortized O(1)) for plain
variable / array-element / object-property lvalues, falling back to the copy
path for typed properties and self-aliasing RHS. This pins the observable
semantics so the optimization stays byte-for-byte identical to PHP. Uses unique
class names + a closure printer because the compat harness @includes every FILE
into one PHP process (top-level names must not collide).
--FILE--
<?php
$p = function($label,$v){ echo $label,'=',$v,"\n"; };

$s="ab"; $s.="cd";                 $p('basic',$s);
$x=5; $x.="a";                     $p('int_lvalue',$x);      // non-string lvalue stringifies
$a=["k"=>"x"]; $a["k"].="y";       $p('array_elem',$a["k"]);

class ConcatAssignInplaceA { public $prop="a"; }
$o=new ConcatAssignInplaceA; $o->prop.="b"; $p('obj_prop',$o->prop);

$s="a"; $y=($s.="b");              $p('result_value',$y.'|'.$s);
$s="a"; echo 'echo_result=' . ($s .= "bc") . "\n";

$s="a"; $s.=$s; $s.=$s; $s.=$s;    $p('self_append',$s.'|'.strlen($s));  // slow-path fallback
$s="ab"; $s.="X".$s;               $p('self_in_concat',$s);              // computed RHS, fast path
$s="a"; $cc=($s.="b").($s.="c");   $p('chained_results',$cc.'|'.$s);     // two .= results co-live: must be owned copies, not dangling aliases

$ra="x"; $rb=&$ra; $ra.=$rb;       $p('ref_self',$ra);                   // slow-path fallback
$rc="x"; $rd=&$rc; $rc.="y";       $p('ref_propagate',$rd);              // shared slot visible via ref
$va="x"; $vb=$va; $vb.="y";        $p('value_copy',$va.'|'.$vb);         // copy isolated from $va

$s="ab"; $s.="";                   $p('empty_rhs',$s.'|'.strlen($s));    // no-op

$s=""; for($i=0;$i<3;$i++) $s.="[".$i."]"; $p('chain',$s);

class ConcatAssignInplaceTyped { public string $sp="a"; public int $ip=1; }
$t=new ConcatAssignInplaceTyped; $t->sp.="b"; $p('typed_str',$t->sp);   // typed slot -> slow path
$t2=new ConcatAssignInplaceTyped;
// echo (not the $p closure) inside catch: invoking a closure as the first call
// in a catch block trips a separate pre-existing VM bug, unrelated to concat.
try { $t2->ip.="x"; echo "typed_int=NO_THROW\n"; }
catch(TypeError $e){ echo "typed_int=TypeError\n"; }                    // type enforced, no corruption
?>
--EXPECT--
basic=abcd
int_lvalue=5a
array_elem=xy
obj_prop=ab
result_value=ab|ab
echo_result=abc
self_append=aaaaaaaa|8
self_in_concat=abXab
chained_results=ababc|abc
ref_self=xx
ref_propagate=xy
value_copy=x|xy
empty_rhs=ab|2
chain=[0][1][2]
typed_str=ab
typed_int=TypeError
--CLEAN--
<?php
unset($p, $s, $x, $a, $o, $y, $cc, $i, $t, $t2, $ra, $rb, $rc, $rd, $va, $vb);
