--TEST--
Check for vapor Engine::make()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$fp = tmpfile();
fwrite($fp, '<h1>The quick brown fox jumped over the lazy dog.</h1>');
$filename = basename(stream_get_meta_data($fp)['uri']);

$v = new Vapor\Engine('/tmp');
$tpl = $v->make($filename);

var_dump($tpl);
?>
--EXPECT--
object(Vapor\Template)#2 (0) {
}
