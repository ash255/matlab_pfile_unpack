#pragma once
#include <string>

std::string wstring2string(std::wstring wstr);
std::string wstring2string(wchar_t* wstr);

class pstring :public std::string
{
public:
	pstring():std::string(), m_flag(0) {}
	pstring(std::string str):std::string::basic_string(str), m_flag(0){}
	~pstring() { std::string::~basic_string();}
	bool check_flag(unsigned int mask) { return ((m_flag & mask) != 0); }
	unsigned int get_flag() { return m_flag; }
	void set_flag(unsigned int flag) { m_flag = flag; }
	pstring& operator =(std::string str) { std::string::operator=(str); return *this; }
	pstring& operator +=(std::string str) { std::string::operator+=(str); return *this; }
private:
	unsigned int m_flag;
};