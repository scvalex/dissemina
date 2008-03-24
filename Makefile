fakefile: fakefile.c
	gcc -Wall fakefile.c -o fakefile

listen: listen.cpp
	g++ -Wall listen.cpp -o listen

selectserver: selectserver.cpp
	g++ -Wall selectserver.cpp -o selectserver
select: select.cpp
	g++ -Wall select.cpp -o select

server: server.cpp
	g++ -Wall server.cpp -o server

getip: getip.cpp
	g++ -Wall getip.cpp -o getip

