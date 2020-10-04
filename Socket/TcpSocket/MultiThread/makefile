all:tcp_cli tcp_srv

tcp_cli:tcp_cli.cc
	g++ -std=c++11 $^ -o $@ 
tcp_srv:tcp_srv_thread.cc
	g++ -std=c++11 $^ -o $@ -lpthread
