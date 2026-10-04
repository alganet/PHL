<?php
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Cuts src/ph7/vm_dom_dbcs.h -- the HTML document's plain double-byte indexes,
 * EUC-KR today -- from php's own HTML decoder and encoder.
 *
 *   XDEBUG_MODE=off php build-aux/gen_domenc_dbcs.php > src/ph7/vm_dom_dbcs.h
 *
 * The oracle is `Dom\HTMLDocument` and nothing else: mbstring and iconv carry
 * tables of their own that answer differently, and the HTML document's two
 * DIRECTIONS do not agree either, so each face is swept from the door it
 * serves and neither is inverted out of the other.
 *
 * Nothing about the framing is assumed. A cell is measured per (lead,trail)
 * byte pair rather than through the standard's pointer arithmetic, which one
 * encoding of this shape has already been caught not following; which bytes
 * open a pair is measured too, with the pair (b,0xA1): a lead eats the second
 * byte and answers one character, a non-lead answers two.
 *
 * DECODE is swept one lead per document, the cells comma-separated -- no trail
 * byte here is 0x2C, and a broken pair re-reads only an ASCII byte, so the
 * separator survives every answer. ENCODE is swept through a COMMENT node,
 * which saveHtml() writes without escaping, so the bytes are the encoder's.
 */
const T0 = 0x40, T1 = 0xFE;             /* the trail window swept, all encodings */
const NCOL = T1 - T0 + 1;
const LEAD0 = 0x81, LEAD1 = 0xFE;
const NROW = LEAD1 - LEAD0 + 1;
$ENC = ['EUC-KR' => 'EUCKR'];
/* Only the encodings that are ONE table of two-byte cells belong here, and
 * EUC-KR is the only CJK name of php's that is one. GBK is not: its decoder is
 * gb18030's, so a lead followed by a DIGIT opens a four-byte sequence rather
 * than naming a cell, and no two-byte table can answer that. Big5's cells
 * reach outside the BMP and four of them are spelled with two code points, so
 * it is cut by gen_domenc_big5.php instead; EUC-JP has a second code set
 * behind its 0x8F lead; ISO-2022-JP has shift states. Each of those is another shape, and a table cut here for one of them
 * would be quietly wrong rather than obviously missing. */

function cps(string $s): array {
	$o = [];
	for( $i = 0 ; $i < mb_strlen($s,'UTF-8') ; $i++ ){
		$o[] = mb_ord(mb_substr($s,$i,1,'UTF-8'),'UTF-8');
	}
	return $o;
}
function parts(string $label,string $src,int $want): array {
	$d = Dom\HTMLDocument::createFromString("<!doctype html><p>$src</p>",0,$label);
	$p = explode(',', $d->getElementsByTagName('p')[0]->textContent);
	if( count($p) - 1 !== $want ){
		fwrite(STDERR,"$label: ".(count($p)-1)." parts for $want cells\n");
		exit(1);
	}
	return $p;
}
$out = [];
foreach( $ENC as $label => $tag ){
	/* --- which bytes open a pair, and what the others answer alone ------- */
	$src = ''; $bs = [];
	for( $b = 0x80 ; $b <= 0xFF ; $b++ ){ $src .= chr($b).chr(0xA1).','; $bs[] = $b; }
	$p = parts($label,$src,count($bs));
	$byte = [];                            /* 0xFFFF = a lead, 0 = U+FFFD */
	foreach( $bs as $i => $b ){
		$c = cps($p[$i]);
		if( count($c) === 1 ){
			$byte[$b] = 0xFFFF;            /* the 0xA1 was eaten with it */
		}elseif( count($c) === 2 ){
			$byte[$b] = $c[0] === 0xFFFD ? 0 : $c[0];
			if( $byte[$b] > 0xFFFF ){ fwrite(STDERR,"$label: byte $b is outside the BMP\n"); exit(1); }
		}else{
			fwrite(STDERR,sprintf("%s: byte %02X answered %d characters\n",$label,$b,count($c)));
			exit(1);
		}
	}
	/* --- the cells ------------------------------------------------------ */
	$cell = array_fill(0,NROW * NCOL,0);
	$pair = [];                            /* cells php spells with TWO code points */
	for( $L = LEAD0 ; $L <= LEAD1 ; $L++ ){
		if( $byte[$L] !== 0xFFFF ) continue;
		$src = ''; $ts = [];
		for( $t = T0 ; $t <= T1 ; $t++ ){ $src .= chr($L).chr($t).','; $ts[] = $t; }
		$p = parts($label,$src,count($ts));
		foreach( $ts as $i => $t ){
			$c = cps($p[$i]);
			if( count($c) === 0 || $c[0] === 0xFFFD ) continue;   /* no character here */
			if( $c[0] > 0xFFFF ){ fwrite(STDERR,"$label: cell outside the BMP\n"); exit(1); }
			$k = ($L - LEAD0) * NCOL + $t - T0;
			$cell[$k] = $c[0];
			if( count($c) > 1 ){
				/* A cell spelled with more than one code point needs a shape
				 * this table does not have; neither encoding cut here has one. */
				fwrite(STDERR,sprintf("%s: cell %02X%02X answered %d characters\n",$label,$L,$t,count($c)));
				exit(1);
			}
		}
	}
	/* --- the encoder, swept over the BMP -------------------------------- */
	$d = Dom\HTMLDocument::createEmpty($label);
	$cpl = []; $txt = '';
	for( $cp = 0x80 ; $cp <= 0xFFFF ; $cp++ ){
		if( $cp >= 0xD800 && $cp <= 0xDFFF ) continue;
		$cpl[] = $cp; $txt .= mb_chr($cp,'UTF-8').',';
	}
	$p = explode(',', substr($d->saveHtml($d->createComment($txt)),4,-3));
	if( count($p) - 1 !== count($cpl) ){ fwrite(STDERR,"$label: encode sweep lost cells\n"); exit(1); }
	$eCp = []; $eB = [];
	foreach( $cpl as $i => $cp ){
		$g = $p[$i];
		if( $g === '?' ) continue;                        /* php cannot spell it */
		if( strlen($g) === 1 ){ $eCp[] = $cp; $eB[] = ord($g); continue; }
		if( strlen($g) !== 2 ){
			fwrite(STDERR,sprintf("%s: U+%04X encoded to %d bytes\n",$label,$cp,strlen($g)));
			exit(1);
		}
		$eCp[] = $cp; $eB[] = (ord($g[0]) << 8) | ord($g[1]);
	}
	$out[$tag] = ['label'=>$label,'byte'=>$byte,'cell'=>$cell,'pair'=>$pair,'eCp'=>$eCp,'eB'=>$eB];
	fwrite(STDERR,sprintf("%-7s %5d cells, %d pair cells, %5d encoded\n",
		$label,count(array_filter($cell)),count($pair),count($eCp)));
}
/* --- emit --------------------------------------------------------------- */
$w = function(array $a){
	$o = ''; $a = array_values($a); $n = count($a);
	for( $i = 0 ; $i < $n ; $i++ ){
		$o .= ($i % 12 === 0 ? "\t" : '').sprintf('0x%04X',$a[$i]).($i + 1 === $n ? '' : ',');
		if( $i % 12 === 11 || $i + 1 === $n ) $o .= "\n";
	}
	return $o;
};
echo <<<'TXT'
/**
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The HTML document's plain double-byte indexes. GENERATED -- do not
 * hand-edit; re-cut with `XDEBUG_MODE=off php build-aux/gen_domenc_dbcs.php`.
 *
 * These are NOT mbstring's or iconv's tables of the same names, and they are
 * not each other's: every array here is swept from `Dom\HTMLDocument` itself,
 * one face at a time, because php's decoder and encoder do not hold the same
 * characters.
 *
 * Per encoding:
 *   aDom<E>Byte  what a byte 0x80..0xFF answers ALONE -- a code point, 0 where
 *                the decoder answers U+FFFD, and 0xFFFF where the byte opens a
 *                two-byte pair instead.
 *   aDom<E>Cell  the pair table, row (lead - 0x81), column (trail - 0x40),
 *                holding the code point or 0 where there is no character. It
 *                is indexed by the BYTES rather than by the standard's pointer
 *                because php's framing is not always the standard's.
 *   aDom<E>EncCp the ENCODER, ordered by code point for a binary search, with
 *   aDom<E>EncB  the byte (below 0x100) or the lead<<8|trail pair it writes.
 *
 * Every cell here is one BMP code point and every character is one cell or
 * one byte; the generator refuses to emit a table where that stops being
 * true, which is why only EUC-KR of php's CJK names is in it.
 */

TXT;
foreach( $out as $tag => $e ){
	printf("/* %s */\n",$e['label']);
	printf("#define PH7_DOM_%s_ENC %d\n",$tag,count($e['eCp']));
	printf("static const sxu16 aDom%sByte[128] = {\n%s};\n",ucfirst(strtolower($tag)),$w($e['byte']));
	printf("static const sxu16 aDom%sCell[%d * %d] = {\n%s};\n",ucfirst(strtolower($tag)),NROW,NCOL,$w($e['cell']));
	printf("static const sxu16 aDom%sEncCp[PH7_DOM_%s_ENC] = {\n%s};\n",ucfirst(strtolower($tag)),$tag,$w($e['eCp']));
	printf("static const sxu16 aDom%sEncB[PH7_DOM_%s_ENC] = {\n%s};\n\n",ucfirst(strtolower($tag)),$tag,$w($e['eB']));
}
