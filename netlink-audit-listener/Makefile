nlaudit-listener: netlink-audit-listener.cpp Makefile
	g++ netlink-audit-listener.cpp -g3 -O0 -o nlaudit-listener

rpm:
	mb2 -X build
	rpmsign-external sign -k ${HOME}/AuroraPlatformSDK/cert/regular_key.pem -c ${HOME}/AuroraPlatformSDK/cert/regular_cert.pem ./RPMS/ru.avroid.nlaudit-listener-1.0-1.armv7hl.rpm

.PHONY: rpm
