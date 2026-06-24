MYSQL_CONNECTOR = E:/zhongzhuan2/mysql_connector/install_mingw

mysql_test:main.cc
	g++ -o $@ $^ -std=c++17 \
		-I"$(MYSQL_CONNECTOR)/include" \
		-L"$(MYSQL_CONNECTOR)/lib64/debug" \
		-lmysqlcppconn \
		-Wl,-rpath,"$(MYSQL_CONNECTOR)/lib64/debug"
.PHONY:clean
clean: 
	-del mysql_test