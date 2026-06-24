all:test_redis1 test_redis2
test_redis1:test_redis.cc
	g++ -o $@ $^ -std=c++17 -I"E:/YingYongRuanJian/mingw64/x86_64-w64-mingw32/include" -L"E:/YingYongRuanJian/mingw64/x86_64-w64-mingw32/lib" -lhiredis
test_redis2:test_redis_2.cc ../RedisMgr.cc
	g++ -o $@ $^ -std=c++17 -I"E:/YingYongRuanJian/mingw64/x86_64-w64-mingw32/include" -L"E:/YingYongRuanJian/mingw64/x86_64-w64-mingw32/lib" -lhiredis
.PHONY:clean
clean:
	-del test_redis1 test_redis2