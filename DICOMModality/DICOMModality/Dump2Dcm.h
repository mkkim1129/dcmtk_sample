#pragma once
class Dump2Dcm
{
public:
	Dump2Dcm(const CString& str);
	~Dump2Dcm();

	void Save (const CString& filename);
private:
	CString m_dump_contents;
};

