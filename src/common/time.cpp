#include "time.hpp"
#include <sstream>
#include <ctime>
#include <iomanip>

std::string ws::date_string() {
	auto t = std::time(nullptr);
	auto tm = *std::localtime(&t);
	std::stringstream ss;
	ss << std::put_time(&tm, "%m-%d-%Y_%H-%M-%S");
	return ss.str();
}