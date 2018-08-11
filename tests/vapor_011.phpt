--TEST--
Check for vapor Engine::escape()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
$v1 = new Vapor\Engine('/tmp');
$fp = tmpfile();
fwrite($fp, '<h1>The quick <?= $this->e($fox) ?> jumped over the <?= $this->e($dog) ?>.</h1>');
$fname = basename(stream_get_meta_data($fp)['uri']);
var_dump($v1->render($fname, [
    'fox' => 'brown \'fox\'',
    'dog' => 'lazy "dog"'
]));
fclose($fp);
?>
--EXPECT--
string(78) "<h1>The quick brown &#039;fox&#039; jumped over the lazy &quot;dog&quot;.</h1>"
