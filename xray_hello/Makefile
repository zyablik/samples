all: unix-sock-client unix-sock-server xray-sample

unix-sock-client: unix_sock_hello_client.cpp Makefile
	clang++ unix_sock_hello_client.cpp -fxray-instrument -fxray-instruction-threshold=1 -o unix-sock-client

unix-sock-server: unix_sock_hello_server.cpp Makefile
	clang++ unix_sock_hello_server.cpp -fxray-instrument -fxray-instruction-threshold=1 -o unix-sock-server

xray-sample: xray_sample.cpp Makefile
	clang++ xray_sample.cpp -g3 -O0 -fxray-instrument -fxray-instruction-threshold=1 -fxray-attr-list=./xray-attr-list.txt -o xray-sample

clean:
	rm -rf ./unix-sock-client ./unix-sock-server ./xray-sample ./xray-log.*
