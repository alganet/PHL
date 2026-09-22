<?php
// No declare() here on purpose: this file binds arguments in WEAK mode, even
// when the strict file next door is what called into it.
class W {
    public $q;
    public int $p { set { $this->q = $value; } }
}
function weakDispatch($v) {
    $w = new W;
    $w->p = $v;
    var_dump($w->q);
}
