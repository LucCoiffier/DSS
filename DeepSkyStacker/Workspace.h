#ifndef __WORKSPACE_H__
#define __WORKSPACE_H__
#include <memory>
#include <QString>
#include <QVariant>

class CWorkspaceSettings;

class CWorkspaceSetting
{
private :
	QString					keyName;
	QVariant				Value;
	bool					dirty;

public :
	CWorkspaceSetting(const QString& name, const QVariant& value = QVariant())
		: keyName(name), Value(value), dirty(true)
	{
	};

	CWorkspaceSetting & operator = (const CWorkspaceSetting & rhs)
	{
		keyName = rhs.keyName;
		Value = rhs.Value;
		dirty = rhs.dirty;
		return (*this);
	};

	bool operator < (const CWorkspaceSetting & s) const
	{
		if (keyName < s.keyName)
			return true;
		else if (keyName > s.keyName)
			return false;
		else
			return keyName < s.keyName;
	};

	bool operator != (const CWorkspaceSetting & s) const
	{
		return (keyName != s.keyName);
	};

	CWorkspaceSetting &	readSetting();
	CWorkspaceSetting &	saveSetting();

	bool	isDirty(bool bClear)
	{
		bool result = dirty;

		if (bClear)
			dirty = false;

		return result;
	};

	inline QString key() const
	{
		return keyName;
	};

	CWorkspaceSetting &	setValue(const CWorkspaceSetting & ws);
	CWorkspaceSetting &	setValue(const QVariant& value);

	inline QVariant value() const
	{
		return Value;
	};
};

/* ------------------------------------------------------------------- */

typedef std::vector<CWorkspaceSetting>				WORKSPACESETTINGVECTOR;
typedef WORKSPACESETTINGVECTOR::iterator			WORKSPACESETTINGITERATOR;

class CWorkspace
{
private:
	std::shared_ptr <CWorkspaceSettings > pSettings;
public:
	CWorkspace();

	~CWorkspace()
	{
	};

	void	setValue(const QString& key, const QVariant& value);

	QVariant value(const QString& key, const QVariant& defaultValue = QVariant()) const;

	bool	isDirty();
	void	setDirty();

	void	readSettings();
	void	saveSettings();
	void	ReadFromFile(FILE * hFile);
	void	ReadFromFile(LPCTSTR szFile);
	void	SaveToFile(FILE * hFile);
	void	SaveToFile(LPCTSTR szFile);
	bool	ReadFromString(LPCTSTR szString);
	void	ResetToDefault();

	void	Push();
	void	Pop(bool bRestore = true);
};

#endif // __WORKSPACE_H__
