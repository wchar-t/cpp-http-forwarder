out:
	g++ *.cpp -lcurl -lpthread -std=c++2a -o out

clean:
	rm out