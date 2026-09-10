# We're going to intercept the install stage so we can hack up
# some files.

override_dh_auto_install_pre:
	dh_auto_install

override_dh_auto_install_clean:
	rm -f ./debian/fluxbox-flux/usr/bin/fluxbox-generate_menu
	perl debian/update-init.pl \
		debian/fluxbox-flux$(DEB_CONFIGURE_SYSCONFDIR)/init

override_dh_auto_install_theme:
	cd ./debian/additional-themes && make
	cd ./debian/additional-themes && make install
	cd ./debian/additional-themes && make clean

override_dh_clean:
	dh_clean
	rm -f data/menu version.h

override_dh_auto_install: \
	override_dh_auto_install_pre   \
	override_dh_auto_install_theme \
	override_dh_auto_install_clean
