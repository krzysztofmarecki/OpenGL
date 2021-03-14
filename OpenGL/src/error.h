#pragma once
#include <iostream>

inline void PrintErrorAndAbort(std::string msg) {
	std::cout << msg;
	std::cin.get(); // wait for ENTER
	std::abort();
}