#include <mutex>
#include <iostream>

using namespace std;


typedef unique_lock<mutex> mutex_lock;
class channel {
	list<int> buffer;
	mutex buffer_mutex; // controls access to buffer
	semaphore sem(0); // count is number of items yet to read
	void write(DataType data) {
		mutex_lock lock(buffer_mutex);
		buffer.push_back(data);
		sem.signal(); // one more item now available
	}
	DataType read() {
		sem.wait(); // block until count > 0, then reduce it
		mutex_lock lock(buffer_mutex);
		DataType item = buffer.front();
		buffer.pop_front();
		return item;
	}
};
