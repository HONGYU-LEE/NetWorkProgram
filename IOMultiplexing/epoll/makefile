all:tcp_cli epoll_lt_srv epoll_et_srv

tcp_cli:tcp_cli.cc
	g++ -std=c++11 $^ -o $@
epoll_lt_srv:epoll_lt_srv.cc
	g++ -std=c++11 $^ -o $@
epoll_et_srv:epoll_et_srv.cc
	g++ -std=c++11 $^ -o $@
