<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Cuts src/ph7/vm_dom_sjis.h from php's own HTML decoder.
 *
 * The oracle is `Dom\HTMLDocument`, not mb_convert_encoding() and not iconv():
 * the HTML document's Shift_JIS is lexbor's index, and it is NOT the JIS X 0208
 * table those two share. Sweeping the other faces gives a different answer on
 * 463 cells and a different set of assigned ones, so the table has to come from
 * the door it is going to serve.
 *
 *   XDEBUG_MODE=off php build-aux/gen_domenc_sjis.php > src/ph7/vm_dom_sjis.h
 *
 * DECODE is swept one lead byte per document, every reachable pointer, the
 * cells comma-separated: no Shift_JIS trail byte is 0x2C and no cell decodes to
 * one, so the separator survives the round trip and cannot be mistaken for
 * payload. ENCODE is swept through a COMMENT node, which saveHtml() writes
 * without escaping anything, so the bytes are the encoder's and nothing else's
 * -- and with no U+0000 in it, which truncates a comment and silently drops
 * every cell after it.
 */
const CELLS = 11104;
function leads(){ return array_merge(range(0x81,0x9F),range(0xE0,0xFC)); }
function ptr($L,$b){
	$off = $b < 0x7F ? 0x40 : 0x41;
	$lo  = $L < 0xA0 ? 0x81 : 0xC1;
	return ($L - $lo) * 188 + $b - $off;
}
/* --- decode ------------------------------------------------------------- */
$idx = array_fill(0,CELLS,0);
foreach( leads() as $L ){
	$src = ''; $keys = [];
	for( $b = 0x40 ; $b <= 0xFC ; $b++ ){
		if( $b == 0x7F ) continue;
		$p = ptr($L,$b);
		if( $p < 0 || $p >= CELLS ) continue;
		$src .= chr($L).chr($b).','; $keys[] = $p;
	}
	$d = Dom\HTMLDocument::createFromString("<!doctype html><p>$src</p>",0,'Shift_JIS');
	$parts = explode(',', $d->getElementsByTagName('p')[0]->textContent);
	if( count($parts) - 1 !== count($keys) ){
		fwrite(STDERR,sprintf("lead %02X: %d parts for %d cells\n",$L,count($parts)-1,count($keys)));
		exit(1);
	}
	foreach( $keys as $i => $p ){
		$v = $parts[$i];
		if( $v === '' ) continue;
		$cp = mb_ord($v,'UTF-8');
		if( $cp === 0xFFFD ) continue;      /* the cell is unassigned */
		if( $cp > 0xFFFF ){
			fwrite(STDERR,"cell $p is outside the BMP\n"); exit(1);
		}
		$idx[$p] = $cp;
	}
}
/* --- encode ------------------------------------------------------------- */
/* The encoder is swept SEPARATELY over the whole BMP rather than inverted out
 * of the decode index, because php's two faces do not agree: its decoder
 * answers nothing for the leads 0xE1..0xFB, while its encoder writes those
 * cells perfectly well. Inverting the index would silently drop ~2200 code
 * points that php spells, so each direction is cut from the face that serves
 * it. Single-byte answers belong to the framing, not to the table, and are
 * asserted here rather than emitted. */
$d = Dom\HTMLDocument::createEmpty('Shift_JIS');
$cps = []; $txt = '';
for( $cp = 0x80 ; $cp <= 0xFFFF ; $cp++ ){
	if( $cp >= 0xD800 && $cp <= 0xDFFF ) continue;   /* not a character */
	$cps[] = $cp; $txt .= mb_chr($cp,'UTF-8').',';
}
$parts = explode(',', substr($d->saveHtml($d->createComment($txt)),4,-3));
if( count($parts) - 1 !== count($cps) ){
	fwrite(STDERR,"encode sweep lost cells\n"); exit(1);
}
$encCp = []; $encPtr = [];
foreach( $cps as $i => $cp ){
	$got = $parts[$i];
	if( $got === '?' ) continue;                     /* php cannot spell it */
	if( strlen($got) === 1 ){
		$b = ord($got);
		$ok = ($cp === 0x80 && $b === 0x80)
		   || ($cp === 0x00A5 && $b === 0x5C)
		   || ($cp === 0x203E && $b === 0x7E)
		   || ($cp >= 0xFF61 && $cp <= 0xFF9F && $b === 0xA1 + $cp - 0xFF61);
		if( !$ok ){
			fwrite(STDERR,sprintf("U+%04X encodes to the single byte %02X, which is no rule of the framing\n",$cp,$b));
			exit(1);
		}
		continue;
	}
	if( strlen($got) !== 2 ){
		fwrite(STDERR,sprintf("U+%04X encoded to %d bytes\n",$cp,strlen($got))); exit(1);
	}
	$p = ptr(ord($got[0]),ord($got[1]));
	if( $p < 0 || $p >= CELLS ){
		fwrite(STDERR,sprintf("U+%04X encodes outside the index\n",$cp)); exit(1);
	}
	$encCp[] = $cp; $encPtr[] = $p;
}
/* --- emit --------------------------------------------------------------- */
$w = function(array $a) {
	$o = ''; $n = count($a);
	for( $i = 0 ; $i < $n ; $i++ ){
		$o .= ($i % 12 === 0 ? "\t" : '') . sprintf('0x%04X',$a[$i]) . ($i + 1 === $n ? '' : ',');
		if( $i % 12 === 11 || $i + 1 === $n ) $o .= "\n";
	}
	return $o;
};
$h = <<<'TXT'
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HTML document's Shift_JIS index. GENERATED -- do not hand-edit; re-cut it
 * with `XDEBUG_MODE=off php build-aux/gen_domenc_sjis.php`.
 *
 * This is NOT the JIS X 0208 table builtin_jis.h carries. That one is iconv's
 * and mbstring's, which agree cell for cell; this one is the HTML document's,
 * it differs from them on 463 cells, and the two cannot be shared.
 *
 * The two directions are two tables because php's own two directions disagree.
 * aDomSjisUni is the DECODER, indexed by the pointer the framing computes,
 * holding the code point a cell stands for or 0 where the decoder answers
 * U+FFFD -- and it answers U+FFFD for every cell of the leads 0xE1..0xFB, so
 * most of the second-level kanji is absent from it. aDomSjisEncCp and
 * aDomSjisEncPtr are the ENCODER, a code point and the cell it is written to,
 * ordered by code point for a binary search; that face has the kanji the
 * decoder is missing, which is why it is swept rather than inverted.
 *
 * Neither table carries a single-byte answer: ASCII, U+0080, the yen sign, the
 * overline and the halfwidth katakana belong to the framing, which is asserted
 * against the oracle when the table is cut rather than emitted into it.
 */
TXT;
echo $h,"\n";
printf("#define PH7_DOM_SJIS_CELLS %d\n",CELLS);
printf("#define PH7_DOM_SJIS_ENC   %d\n\n",count($encCp));
printf("static const sxu16 aDomSjisUni[PH7_DOM_SJIS_CELLS] = {\n%s};\n\n",$w($idx));
printf("static const sxu16 aDomSjisEncCp[PH7_DOM_SJIS_ENC] = {\n%s};\n\n",$w($encCp));
printf("static const sxu16 aDomSjisEncPtr[PH7_DOM_SJIS_ENC] = {\n%s};\n",$w($encPtr));
