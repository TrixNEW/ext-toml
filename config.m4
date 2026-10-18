PHP_ARG_ENABLE([toml],
  [whether to enable toml support],
  [AS_HELP_STRING([--enable-toml], [Enable TOML parsing support])],
  [no])

if test "$PHP_TOML" != "no"; then
  PHP_NEW_EXTENSION(toml, toml.c src/parser.c, $ext_shared,, [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -fvisibility=hidden])
  PHP_ADD_BUILD_DIR([$ext_builddir/src])
fi
