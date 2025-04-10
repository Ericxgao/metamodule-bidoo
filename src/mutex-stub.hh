#pragma once

#include <stdint.h>

namespace std
{

struct mutex {
private:
	volatile uint32_t lock_flag;

public:
	mutex() : lock_flag(0) {
	}
	
	void lock() {
		// Simple spinlock implementation 
		while (__atomic_test_and_set(&lock_flag, __ATOMIC_ACQUIRE)) {
			// Wait until lock is released
			// Optional: could add a small delay or yield here
		}
	}
	
	void unlock() {
		__atomic_clear(&lock_flag, __ATOMIC_RELEASE);
	}
};

} // namespace std