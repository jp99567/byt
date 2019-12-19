
if [[ `hostname` -eq 'ws' ]]; then
	export CROSS_ENV_ROOT=$HOME/workspace/bb-sysroot
	export PKG_CONFIG_PATH=$HOME/workspace/gpiod/lib/pkgconfig
else
	export CROSS_ENV_ROOT=$HOME/work/work2/bbb/bb-sysroot
	export BYTD_STAGING=$HOME/work/work2/bbb/staging
	export PKG_CONFIG_PATH=$HOME/work/work2/install-libgpiod141/lib/pkgconfig
fi
