--TEST--
Check for vapor layout()
--SKIPIF--
<?php if (!extension_loaded("vapor")) {
    print "skip";
}
?>
--FILE--
<?php
is_dir("/tmp/views/layout") or mkdir("/tmp/views/layout", 0777, true);

// layout file
$layout = <<<'EOF'
<div class="layout-container"><?= $this->section('content') ?></div>
EOF;
file_put_contents('/tmp/views/layout/default.php', $layout);

// template file
$index = <<<'EOF'
<?php $this->layout('layout::default') ?>
<span>The quick <?= $animal ?> jumped over the lazy dog</span>
EOF;
file_put_contents('/tmp/views/index.php', $index);

// vapor
$v1 = new Vapor('/tmp/views', 'php');
$v1->addFolder('layout', '/tmp/views/layout');
$content = $v1->render('index', ['animal' => 'brown fox']);
var_dump($content);

// cleanup
unlink("/tmp/views/layout/default.php");
unlink("/tmp/views/index.php");
rmdir("/tmp/views/layout");
rmdir("/tmp/views");
?>
--EXPECT--
string(93) "<div class="layout-container"><span>The quick brown fox jumped over the lazy dog</span></div>"
