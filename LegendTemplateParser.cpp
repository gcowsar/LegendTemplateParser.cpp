// LegendTemplateParser.cpp : Defines the entry point for the console application.
//

/*
Parse this form:
Page id=Truck type=AddOrEdit cid=FMDT01
		addTitle="Add Truck" editTitle="Edit Truck"
	Section title=Truck mode=add
	Form title="Add Truck"

this is one statement with two substatements 
each substatement can have 0, 1 or many substatements

<letter> ::= "A-Za-z"
<number> ::= "0-9"
<operator> ::= "*+-/_=,"
<function> ::= <identifier> '(' <delimited-expression> ')'
<expression> ::= <expression-element> { <operator> <expression-element> }
<idchar> ::= <letter> | <number>
<identifier> ::= <letter> { <idchar> }
<reference> ::= '@' <identifier>
<value> ::= <numericValue> | <quoted-string> | <reference>
<assignment> ::= <identifier>=<value>
<assignments> ::= <assignment> { <assignment> }
<statement> ::= <identifier> { <assignments> }

<primary-identifier> ::= <identifier>
<primary-identifier> list of <identifier>=<value>

<primary-statement> ::= <assignment> { <assignment> }

*/

#include "stdafx.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <deque>
#include <unordered_set>

#include <vector>
#include <list>
#include <map>
#include <Shlobj.h>  // need to include definitions of constants
#include <algorithm>
#include <regex>

// .....

using namespace std;

ifstream myFile("C:\\Users\\George Cowsar\\stuff.txt"); // one statement


class Statement;

typedef map<string, string> VariableMap;
typedef map<string, Statement*> StatementMap;
typedef vector<Statement*> StatementList;
typedef vector<string> StringList;

#include <codecvt>

const int _SpacesPerTab = 4;

string startCommentWeb = "<%--";
string endCommentWeb = "--%>";

string startCommentCode = "/* ";
string endCommentCode = " */";

string startComment;
string endComment;
string endOfLine;

string endOfLineCode = "";
string endOfLineWeb = "\n";

string defFileName;

class MyException : public std::exception {
private:
	std::string message_;
public:
	explicit MyException(const std::string& message);
	virtual const char* what() const throw() {
		return message_.c_str();
	}
};


MyException::MyException(const std::string& message) : message_(message) {

}

string GetUserPath()
{
	WCHAR path[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path))) {
		wstring userPath(path);
		wstring_convert<codecvt_utf8<wchar_t>> myconv;
		return myconv.to_bytes(userPath);
	}
	return "";
}

string ResolvePath(string path)
{
	size_t hasTilde = path.find("~");
	if (hasTilde != string::npos) {
		string myUserDir = GetUserPath();
		return path.replace(hasTilde, 1, myUserDir);
	}
	return path;
}

string trim(const string& str,
	const string& whitespace = " \t")
{
	const size_t strBegin = str.find_first_not_of(whitespace);
	if (strBegin == string::npos)
		return ""; // no content

	const size_t strEnd = str.find_last_not_of(whitespace);
	const size_t strRange = strEnd - strBegin + 1;

	return str.substr(strBegin, strRange);
}

size_t findMatchingDelim(string s, string sd, string ed)
{
	string v = sd;
	size_t delimLen = sd.length();
	size_t count = 0;
	size_t nesting = 0;
	do {
		s = s.substr(delimLen);
		count += delimLen;
		size_t nextEnd = s.find(ed);
		size_t nextStart = s.find(sd);
		if (nextEnd == string::npos)
			return string::npos;
		if (nextEnd < nextStart) {
			if (nesting == 0)
				return count + nextEnd + delimLen;
			nesting--;
			count += nextEnd;
			s = s.substr(nextEnd);
		}
		else { // nextEnd > nextStart
			nesting++;
			count += nextStart;
			s = s.substr(nextStart);
		}
	} while (true);
}

StringList split(string s, string delimiter)
{
	StringList stringList;
	do {
		size_t found, delimLength = delimiter.size();
		string elem;
		if (s[0] == '<' && s.find("<#") == 0) {
			found = findMatchingDelim(s, "<#", "#>");
			int delimLength = 2;
			if (found == string::npos)
			{
				elem = s.substr(delimLength); // rest of the string, error
			}
			else {
				elem = s.substr(delimLength, found - delimLength - delimLength);
				if (found == s.length())
					found = string::npos;
				else {
					found = s.find(delimiter, found);
				}
			}
			elem = trim(elem);
		}
		else if (s[0] == '"') {
			found = s.find('"',1);
			if (found != string::npos) {
				elem = trim(s.substr(0, found + 1));
				found = s.find(delimiter, found + 1);
			}
			else
				elem = trim(s.substr(1)); // close quote missing, error
		}
		else {
			found = s.find(delimiter);
			elem = trim(s.substr(0, found));
		}
		stringList.push_back(elem);
		if (found == string::npos)
			break;
		s = s.substr(found + delimLength);
	} while (true);
	return stringList;
}

StringList splitParenList(string s, string delimiter)
{
	// s = "<# foo, bar #>,<# 1,2,3 <# 4,5,6 #> #>";
	// StringList test = split(s, ",");
	// test = split("<# foo #>,\"bar\",foobar", ",");
	s = trim(s, "()[]");
	return split(s, delimiter);
}

bool nonAlphaNumeric(char c)
{
	return !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ');
}

string removeNonAlpha(string str)
{
	str.erase(remove_if(str.begin(), str.end(), nonAlphaNumeric), str.end());
	string result;
	bool foundSpace = false;
	size_t len = str.length();
	for (size_t ndx = 0; ndx < len; ndx++) {
		int c = str[ndx];
		if (str[ndx] == ' ') {
			foundSpace = true;
		}
		else if (foundSpace) {
			foundSpace = false;
			result += toupper(c);
		}
		else {
			result += c;
		}
	}

	return result;
}

string removeSpaces(string str) 
{
	str.erase(remove_if(str.begin(), str.end(), ::isspace), str.end());
	return str;
}

template <typename K, typename V>
ostream& operator<<(ostream& os, const map<K, V>& m)
{
	os << "{ ";
	for (typename map<K, V>::const_iterator i = m.begin(); i != m.end(); ++i)
	{
		if (i != m.begin()) os << " ";
		os << i->first << "=\"" << i->second << "\"";
	}
	return os << " }";
}

void print(string v)
{
	cout << v;
}

string doubleToString(double dv)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%g", dv);
	return buffer;
}

double stringToDouble(string sv)
{
	return atof(sv.c_str());
}

bool isDouble(string ds)
{
	char* endptr = 0;
	const char* s = ds.c_str();
	strtod(s, &endptr);

	if (*endptr != '\0' || endptr == s)
		return false;
	return true;
}

bool replace(string& str, const string& from, const string& to) {
	size_t start_pos = str.find(from);
	if (start_pos == string::npos)
		return false;
	str.replace(start_pos, from.length(), to);
	return true;
}

string replaceAll(string str, string replaceStr, string withStr)
{
	while (replace(str, replaceStr, withStr));
	return str;
}

string replaceIndent(size_t indent, string& str)
{
	string whitespace = " \t";
	const size_t strBegin = str.find_first_not_of(whitespace);
	if (strBegin == string::npos)
		return ""; // no content

	str = str.substr(strBegin);

	int numToAdd = strBegin - indent * 2 - 4;
	string addSpaces = "";
	if (numToAdd > 0) {
		while (numToAdd--)
			addSpaces += " ";
		str = addSpaces + str;
	}

	return str;
}

// end of utilities

class Statement {
	Statement* parent = NULL;
	Statement* referringStatement;
	string name;
	VariableMap variables;
	StatementList statements;
	StatementMap statementMap;
	bool lastValue;
	vector<bool> ifstack;
public:
	Statement(string _name)
	{
		name = _name;
		parent = NULL;
	}

	Statement() 
	{
		name = "Root";
		parent = NULL;
	}

	Statement(const Statement& src)
	{
		*this = src;
	}

	Statement& operator=(const Statement& src)
	{
		name = src.name;
		variables = src.variables;
		parent = src.parent;
		for (StatementList::const_iterator it = src.statements.begin(); it != src.statements.end(); it++)
		{
			Statement* statement = new Statement(**it);
			statement->SetParent(this);
			statements.push_back(statement);
		}
		return *this;
	}

	void OutputError(string errorText)
	{
		cout << startComment + " **  " << errorText << endComment << " in file: " << defFileName <<  endl;
	}

	const StatementList& GetStatements() const
	{
		return statements; 
	}

	Statement* GetRoot()
	{
		Statement* root = this;
		while (root->parent != NULL) {
			root = root->parent;
		}
		return root;
	}

	void GetStatementsLike(string key, StatementList& results)
	{
		for (StatementList::iterator it = statements.begin(); it != statements.end(); it++)
		{
			Statement* statement = *it;
			if (statement->GetKey().find(key) == 0)
				results.push_back(statement);
			statement->GetStatementsLike(key, results);
		}
	}

	string GetKey()
	{
		// the key is name:type where name is this statement name, or its name property
		string keyName;
		if (!GetValue("name", keyName))
			keyName = name;
		string value;
		GetValue("type", value, true);
		return keyName + ":" + value;
	}

	Statement* GetStatement(string key)
	{
		StatementMap::iterator found = statementMap.find(key);
		Statement* statement = (found != statementMap.end()) ? found->second : (Statement*)NULL;
		if (!statement)
			for (StatementMap::iterator it = statementMap.begin(); it != statementMap.end(); it++) {
				string statementKey = it->first;
				if (key.find(statementKey) == 0) {
					// statement is a generic type for the requested key
					statement = it->second;
					break;
				}
			}
		if (!statement)
			for (StatementMap::iterator it = statementMap.begin(); it != statementMap.end(); it++) {
				string statementKey = it->first;
				if (statementKey == "*:") {
					// statement is a generic type for the requested key
					statement = it->second;
					break;
				}
			}
			// statementKey == "*" ||
		return statement;
	}

	void AppendAssignment(string variable, string value)
	{
		variables[variable] += value;
	}

	void AddAssignment(string variable, string value, bool escapeQuotes = false)
	{
		variables[variable] = value;
	}

	VariableMap& GetVariables()
	{
		return variables;
	}

	StatementList& GetStatements()
	{
		return statements;
	}

	Statement* Parent()
	{
		if (!parent)
			return this;
		return parent;
	}

	void SetParent(Statement* _parent)
	{
		parent = _parent;
	}

	Statement* AddStatement(string name) {
		Statement* newStatement = new Statement(name);
		statements.push_back(newStatement);
		newStatement->SetParent(this);
		return newStatement;
	}

	void AddStatementsToMap()
	{
		for (StatementList::iterator& it = statements.begin(); it != statements.end(); it++) {
			Statement* statement = *it;
			string key = statement->GetKey();
			statementMap[key] = statement;
		}
	}

	void Print(ostream& out)
	{
		out << "{ " << name << " " << variables << endl;
		for (StatementList::iterator& it = statements.begin(); it != statements.end(); it++) {
			(*it)->Print(out);
		}
		out << "} " << endl;
	}

	bool getLastValue()
	{
		if (ifstack.size() > 0) {
			lastValue = ifstack.back();
			ifstack.pop_back();
		}
		return lastValue;
	}

	// @(id,key)
	bool EvaluateExpression(string expression, string& result)
	{
		if (expression[0] != '(' && expression[0] != '[') {
			return GetValue(expression, result);
		}
		StringList arguments = splitParenList(expression, ",");
		string op = arguments[0];
		size_t numArgs = arguments.size();
		size_t lastItem = numArgs - 1;
		if (op == "id") {
			if (GetValue(arguments[1], result)) {
				result = removeNonAlpha(result);
				return true;
			}
		}
		else if (op == "format") {
			// a1=@(format,a,2,@action_);
			if (numArgs < 4) {
				OutputError(" expression requires at least 3 arguments");
				return false;
			}
			string formatSpec = arguments[lastItem];
			string formatResolved;
			GetValue(formatSpec, formatResolved);
			for (int i = 1; i < numArgs-1; i += 2) {
				string varName = arguments[i];
				string varValue = arguments[i + 1];
				result = replaceAll(formatResolved, "`" + varName + "`", varValue);
				return true;
			}
		}
		else if (op == "*" || op == "+") {
			if (numArgs != 3) {
				OutputError(" expression requires 2 arguments");
				return false;
			}
			string s1 = arguments[1], s2 = arguments[2];
			double nan = std::numeric_limits<double>::quiet_NaN();
			double d1 = isDouble(s1) || GetValue(arguments[1], s1) ? stringToDouble(s1) : nan;
			double d2 = isDouble(s2) || GetValue(arguments[2], s2) ? stringToDouble(s2) : nan;
			try {
				double d = op == "*" ? d1 * d2 : d1 + d2;
				result = doubleToString(d);
				return result != "nan";
			}
			catch (...) {
			}
		}
		else if (op == "ifeq" || op == "ifne") {
			if (numArgs < 4) {
				OutputError(" ifeq/ifne: expression requires 2 arguments + the conditional text");
				return false;
			}
			string varName = arguments[1];
			string value = arguments[2];
			string varValue;
			if (GetValue(varName, varValue))
			{
				bool opValue = (varValue == value) ^ (op == "ifne");
				if (opValue) {
					ifstack.push_back(opValue);
					result = arguments[3];
					return true;
				}
			}
			ifstack.push_back(false);
			result = "";
			return true;
		}
		else if (op == "ifnn_null" || op == "ifnn" || op == "ifn" || op == "if") {
			bool senseNN = op == "ifnn_null" || op == "ifnn" || op == "if";
			if (numArgs < 3) {
				OutputError(" ifnn: expression requires at least 2 arguments where the last one is the conditional text");
				return false;
			} // "c:\Users\George Cowsar\Projects\TankMateGen2\PageDefs" Truck Trailer FMCustomer FMStore Terminal Supplier Driver Order
			for (int ndx = 1; ndx < lastItem; ndx++) {
				string v, s = arguments[ndx];
				bool hasValue = GetValue(s, v) && v.length() > 0;
				if (hasValue && op == "if")
					hasValue = v != "false";
				//if (s == "lengthish")
					//DebugBreak();
				if (senseNN ? !hasValue : hasValue)
				{
					ifstack.push_back(false);
					result = op == "ifnn_null" ? "null" : "";
					return true;
				}
			}
			result = arguments[lastItem];
			ifstack.push_back(true);
			return true;
		}
		else if (op == "elif")
		{
			if (op == "elif") {
				int x = 1;
				x++;
				lastValue = x == 1;
			}
			if (numArgs < 3)
			{
				OutputError(" elif: expression requires at least 2 arguments");
				return false;
			}
			result = "";
			if (getLastValue()) {
				return true;
			}
			for (int ndx = 1; ndx < lastItem; ndx++) {
				string v, s = arguments[ndx];
				bool hasValue = GetValue(s, v) && v.length() > 0;
				if (!hasValue || (hasValue && op == "elif" && v == "false"))
				{
					ifstack.push_back(false);
					return true;
				}
			}
			result = arguments[lastItem];
			ifstack.push_back(true);
			return true;
		}
		else if (op == "elifnn" || op == "elifnn_null" || op == "elif")
		{
			if (op == "elif") {
				int x = 1;
				x++;
				lastValue = x == 1;
			}
			if (numArgs < 3)
			{
				OutputError(" elifnn: expression requires at least 2 arguments");
				return false;
			}
			result = "";
			if (getLastValue()) {
				return true;
			}
			for (int ndx = 1; ndx < lastItem; ndx++) {
				string v, s = arguments[ndx];
				bool hasValue = GetValue(s, v) && v.length() > 0;
				if (!hasValue || (hasValue && op == "elif" && v == "false"))
				{
					ifstack.push_back(false);
					if (op == "elifnn_null")
						result = "null";
					return true;
				}
			}
			result = arguments[lastItem];
			ifstack.push_back(true);
			return true;
		}
		else if (op == "else")
		{
			if (numArgs != 2)
			{
				OutputError(" else: expression requires 1 argument");
				return false;
			}
			result = "";
			if (getLastValue()) {
				return true;
			}
			result = arguments[1];
			return true;
		}
		else if (op == "foreach_template") {
			// @(foreach_template,template,type,<# statement *>)
			if (numArgs < 4)
			{
				OutputError(" foreach_template: expression requires 3 args, arg1:template name, arg2: type, arg3: statement");
				return false;
			}
			string name, type;
			if (!GetValue(arguments[1], name) || !GetValue(arguments[2], type)) {
				OutputError(" foreach_template: could not get name or type");
				return false;
			}
			StringList types = split(type, ",");
			StatementList matchingStatements;
			Statement* root = referringStatement->GetRoot();
			for (StringList::iterator it = types.begin(); it != types.end(); it++) {
				string type = *it;
				root->GetStatementsLike(name + ":" + type, matchingStatements);
			}
			//Statement* theTemplate = statementMap[name + ":" + type];
			// for each matchingStatements apply the variables to this template
			for (StatementList::iterator it = matchingStatements.begin(); it != matchingStatements.end(); it++)
			{
				string text = arguments[numArgs - 1];
				size_t numResolved = 0;
				bool resolved = false;
				Statement& statement = **it;
				Statement theTemplate = *this;
				theTemplate.ApplyVariables(statement);
				bool modified = theTemplate.ResolveReferences(text, numResolved, resolved);
				if (!resolved) {
					OutputError(" foreach_template: variable references could not be resolved");
				}
				result += text;
			}
			return true;
		}
		else if (op == "foreach" || op == "foreach_ref") {
			if (numArgs < 3) {
				OutputError(" foreach: expression requires 2 arguments, arg1 is the control variable and arg2 is the generated text");
				return false;
			}
			string controlVar, controlVarName = arguments[1];
			bool ref = op == "foreach_ref";
			if (!GetValue(controlVarName, controlVar)) {
				OutputError(" foreach: could not get control variable value: " + controlVarName);
				return false;
			}
			StringList varList = split(controlVar, ",");
			for (StringList::iterator it = varList.begin(); it != varList.end(); it++)
			{
				string text = arguments[numArgs - 1];
				size_t numResolved = 0;
				bool resolved = false;
				string s = *it;
				variables[controlVarName] = ref ? "@" + s + "_" : s;
				bool modified = ResolveReferences(text, numResolved, resolved);
				result += text;
			}
			return true;
		}
		else if (op == "split") {
			if (numArgs != 4) {
				OutputError(" split expression requires 3 arguments");
				return false;
			}
			string delimeter = arguments[1];
			string reference = arguments[2];
			string value;
			if (!GetValue(reference, value)) {
				OutputError(" split variable reference not resolved: " + reference);
				return false;
			}
			try {
				int index = stringToDouble(arguments[3]);
				StringList values = split(value, delimeter);
				if (values.size() == 1) {
					result = value;
				}
				else {
					result = values[index];
				}
				return true;
			}
			catch (...) {
				OutputError(" split error converting arguments");
				return false;
			}
		}
		else if (op == "1+" || op == "inc") {
			bool isSilent = op == "inc";
			if (numArgs != 2) {
				OutputError(" " + op + " expression requires 1 argument");
				return false;
			}
			string v, vName = arguments[1];
			if (!GetValue(vName, v)) {
				OutputError(" " + op + " requires a variable name argument, found: " + vName);
				return false;
			}
			try {
				double d1 = stringToDouble(v);
				if (!isnan(d1)) {
					d1 += 1;
					SetValue(vName, result);
					result = isSilent ? "" : doubleToString(d1);
					return true;
				}
				else throw "bad argument";
			}
			catch (...) {
				OutputError(" 1+ requires a numeric value, found: " + vName + " = " + v);
				return false;
			}
		}
		result = "";
		// cout << "<!-- ** Error in evaluating expression: " << expression << "*** -->" << endl;
		return false;
	}

	bool SetValue(string key, string value)
	{
		VariableMap::iterator it = variables.find(key);
		if (it != variables.end()) {
			variables[key] = value;
			return true;
		}
		else if (parent) {
			return parent->SetValue(key, value);
		}
		return false;
	}

	bool GetValue(string key, string& value, bool oneLevel=false)
	{
		VariableMap::iterator it = variables.find(key);
		if (it != variables.end()) {
			value = it->second;
			bool resolved = false;
			size_t numResolved = 0;
			ResolveReferences(value, numResolved, resolved);
			return true;
		}
		else if (!oneLevel && parent) {
			return parent->GetValue(key, value, oneLevel);
		}
		return false;
	}

	/*
	map the template with name=Page type=ViewStuff with key "Page:ViewStuff" 
	to the statement Page type=ViewStuff

	templates:
	template name=Page type=ViewStuff v1="default v1" v2="default v2" 
				params=id,type,label,v1,v2
	[[
		Stuff that I want to view: @v1_-ish stuff and @v2_-ish other stuff
		ID: @id
		##foreach Field##
		More stuff
	]]
	template name=X1 typeof=Field x1="default x1" x2="default x2"
	[[ X1 values: @x1_ and @x2_ ]]
	template name=X2 typeof=Field x1="default f2x1" x2="default f2x2"
	[[ X2 values: @x1_ and @x2_ ]]
	template name=Y typeof=Field y1="default y1" y2="default y2"
	[[ Y values: @y1_ and @y2_ ]]

	statements:
	Page type=ViewStuff id=MyPage v2="Hurray for v2"
		X1 x2="Override x2"
		X2 x1="Override x1"
		Y y2="Override y2"

	*/

	// used to apply variables parent to child (inheritance)
	// or controlling statement to corresponding template
	void ApplyVariables(Statement& fromStatement)
	{
		for (VariableMap::iterator it = fromStatement.variables.begin(); it != fromStatement.variables.end(); it++)
		{
			string key = it->first;
			string value = it->second;
			variables[key] = value;
		}
	}

	bool checkSlots(const StringList& slots, string slotName) {
		return find(begin(slots), end(slots), slotName) != end(slots);
	}

	bool hasChildren() 
	{
		return statements.size() != 0;
	}

	string RemoveUnusedTemplate(string body)
	{
		size_t nextTemplate;
		string output = "";
		bool foundOrphanTemplate = false;
		while ((nextTemplate = body.find("##")) != string::npos) {
			output += body.substr(0, nextTemplate);
			size_t tokenEOL = body.find("\n", nextTemplate);
			body = body.substr(tokenEOL + 1);
			foundOrphanTemplate = true;
		}
		return output + body;
	}

	bool EvaluateTemplates(Statement& templates, stringstream& output)
	{
		string body;
		string key = GetKey();
		Statement* templatePtr = templates.GetStatement(key);
		if (!templatePtr) {
			cout << startComment + " **  error no corresponding template for statement, key = " << key << endComment << endl;
			return false; // error no corresponding template
		}

		Statement theTemplate = *templatePtr;
		theTemplate.ApplyVariables(*this);
		//if (theTemplate.parent)
			//theTemplate.ApplyVariables(*theTemplate.parent);
		theTemplate.Evaluate(this);
		theTemplate.GetValue("body", body, true);
		string templateComment = startComment + " Template: " + theTemplate.GetKey() + " " + endComment + "\n";
		output << templateComment;

		if (!hasChildren()) {
			size_t nextTemplate;
			bool foundOrphanTemplate = false;
			while ((nextTemplate = body.find("##")) != string::npos) {
				string bodyOutput = body.substr(0, nextTemplate);
				output << bodyOutput;
				size_t tokenEOL = body.find("\n", nextTemplate);
				body = body.substr(tokenEOL + 1);
				foundOrphanTemplate = true;
			}
			if (foundOrphanTemplate)
				output << "\n";
			output << body;
			return true;
		}

		StringList tokens;
		for (StatementList::iterator it = statements.begin(); it != statements.end(); )
		{
			// find lists or elements ##
			Statement& statement = **it;
			while (!checkSlots(tokens, statement.name)) {
				size_t nextSlotPos = body.find("##");
				if (nextSlotPos == string::npos)
					break;
				string bodyOutput = body.substr(0, nextSlotPos);
				output << bodyOutput;
				body = body.substr(nextSlotPos + 2); // the rest of the body
				size_t tokenEOL = body.find("\n");
				string tokenTypes = body.substr(0, tokenEOL);
				tokens = split(tokenTypes, ",");
				body = body.substr(tokenEOL + 1);
			}
			statement.EvaluateTemplates(templates, output);
			it++;
		}
		if (body.length())
			output << RemoveUnusedTemplate(body);
		return true;
	}

	bool ResolveReferences(string& val, size_t& numResolved, bool& resolved)
	{
		int len;
		size_t pos = 0;
		bool modified = false;
		resolved = true;
		while ((pos = val.find_first_of('@', pos)) != string::npos)
		{
			pos++;
			string reference = val.substr(pos);
			size_t end, len;
			string add;
			if (reference.find("(") == 0) {
				end = val.find(")_", pos);
				add = (end != string::npos) ? ")_" : "";
				len = end != string::npos ? end - pos : val.length() - pos;
			}
			else if (reference.find("[") == 0) {
				end = val.find("]", pos);
				add = (end != string::npos) ? "]" : "";
				len = end != string::npos ? end - pos : val.length() - pos;
			}
			else if (reference[0] == '@') {
				pos++;
				continue;
			}
			else {
				end = val.find_first_of(" \t_@", pos);
				add = (end != string::npos && val[end] == '_') ? val.substr(end, 1) : "";
				len = end != string::npos ? end - pos : val.length() - pos;
				if (len == 0)
					continue;
			}
			reference = val.substr(pos, len);
			string newValue = "";
			bool evaluated = false;
			if (EvaluateExpression(reference, newValue)) {
				bool circularReference = false;
				if (newValue == val) {
					OutputError("Circular reference: " + val);
					newValue = "*** Circular reference: " + reference + "***";
					circularReference = true;
				}
				numResolved++;
				replace(val, string("@") + reference + add, newValue);
				len = newValue.length();
				modified = evaluated = true;
				if (!circularReference)
					unresolvedRefs.erase(reference);
			}
			else {
				unresolvedRefs.emplace(reference);
				resolved = false;
			}
			pos += evaluated ? -1 : len + add.length();
		}
		return modified;
	}

	unordered_set<string> unresolvedRefs;

	void Evaluate(Statement* _referringStatement)
	{
		referringStatement = _referringStatement;
		size_t numResolved = 0;
		bool continueResolving = true;
		string empty;
		deque<string> varsToResolve;
		unresolvedRefs.empty();
		for (VariableMap::iterator it = variables.begin(); it != variables.end(); it++)
			varsToResolve.push_back(it->first);
		do {
			numResolved = 0;
			size_t numToResolve = varsToResolve.size();
			while (numToResolve--)
			{
				string key = varsToResolve.front(); varsToResolve.pop_front();
				string val = variables[key];
				bool resolved = false;
				bool modified = ResolveReferences(val, numResolved, resolved);
				if (modified) {
					variables[key] = val;
					if (!resolved) {
						varsToResolve.push_back(key);
					}
				}
			}
			continueResolving = numResolved > 0 && varsToResolve.size() > 0;
		} while (continueResolving);

		#ifdef _DEBUG
		if (unresolvedRefs.size()) {
			cout << startComment + " **  Template: " << GetKey() << " has unresolved references " << endComment << endl;
			for (unordered_set<string>::iterator it = unresolvedRefs.begin(); it != unresolvedRefs.end(); it++ ) {
				cout << "<!--    Reference not resolved: " << *it << endl;
			}
		}
		#endif
		// resolve child statements
		for (StatementList::iterator it = statements.begin(); it != statements.end(); it++) {
			Statement& child = **it;
			child.Evaluate(_referringStatement);
		}
	}
};

enum states { start, assignment, assignRHS, assignBlockString, body, comment };

class StatementParser {
	ifstream file;
	size_t pos = 0;
	size_t indent = 0;
	size_t lastIndent = 0;
	string line;
	bool foundEquals = false;
	bool foundQuoted = false;
	int parsingState = states::assignment;
	int lastParsingState = states::start;
	int pushParsingState = states::start;
	Statement rootStatement;
	string pushToken;
	
	public:
	StatementParser(string fileName)
	{
		file.open(fileName);
		if (!file.is_open()) {
			cout << "Could not open file: " << fileName << endl;
			throw new exception("Could not open file");
		}
	}

	public:
	void ParseStatements() 
	{ 
		Statement* currentStatement = &rootStatement;
		int lineCount = 0;
		string body;
		string lastToken;
		while (getline(file, line)) {
			lineCount++;
			string token;
			int tokenCount = 0;
			pos = 0;
			while (GetNextToken(token))
			{
				tokenCount++;

				if (token == "/*") {
					pushParsingState = parsingState;
					parsingState = states::comment;
				}
				else if (token == "<#") {
					pushParsingState = parsingState; // was assignRHS
					parsingState = states::assignBlockString;
				}
				else if (token == "//") {
					pos = string::npos; // throw away the rest of the line
				}
				else 
				switch (parsingState) {
				case states::start:
					if (lineCount == 1 || indent > lastIndent) {
						currentStatement = currentStatement->AddStatement(token);
						lastIndent = indent;
					}
					else if (lastIndent == indent) {
						currentStatement = currentStatement->Parent();
						currentStatement = currentStatement->AddStatement(token);
					}
					else // must be less than
					{
						size_t levels = lastIndent - indent  + 1; // always one deeper than root
						while (levels-- > 0)
							currentStatement = currentStatement->Parent();
						currentStatement = currentStatement->AddStatement(token);
					}
					lastIndent = indent;
					parsingState = states::assignment;
					break;

				case states::assignRHS:
					currentStatement->AddAssignment(lastToken, token, true);
					lastParsingState = states::assignRHS;
					parsingState = states::assignment;
					break;

				case states::comment:
					if ((pos = token.find("*/")) != string::npos) {
						pos += 2;
						parsingState = pushParsingState;
					}
					break;
				
				case states::assignBlockString: {
						size_t tokenPos = token.find("#>");
						bool foundEnd = tokenPos != string::npos;
						if (foundEnd)
							token = token.substr(0, tokenPos);
						if (token.length() != 0) {
							// token = replaceAll(token, "\"", "&quot;");
							// %% need a way to quote -> &quot; with quote-> backslash, 
							// depends on whether html content or not 
							replaceIndent(indent, token);
							currentStatement->AppendAssignment(lastToken, token + endOfLine);
						}
						if (foundEnd) {
							pos = string::npos;
							parsingState = parsingState = states::assignment;
						}
					}
					break;

				case states::body:
					if (token.find("]]") != string::npos)
					{
						currentStatement->AddAssignment("body", body);
						parsingState = states::start;
					}
					else
					{
						body += token + "\n";
					}
					break;

				case states::assignment:
					if (foundEquals) { // lhs
						lastToken = token;
						lastParsingState = states::start;
						parsingState = states::assignRHS;
						break;
					}
					else if (token == "[[") {
						body = "";
						parsingState = states::body;
						lastParsingState = states::start;
						int levels = lastIndent - indent; // always one deeper than root
						while (levels--)
							currentStatement = currentStatement->Parent();
						break;
						// make sure its on a line by itself
					}
					else if (foundQuoted) {
						if (lastParsingState == states::assignRHS) {
							currentStatement->AppendAssignment(lastToken, token);
							parsingState = states::assignment;
							break;
						}
						break;
					}
					else if (tokenCount > 1) {
						// error and go get next line
						break;
					}
					// no assignment found, so it is first token of a new statement
					pushToken = token;
					parsingState = states::start;
					break;
				}
				if (lastParsingState != states::assignRHS)
					lastParsingState = states::start;
			}
		}
		rootStatement.AddStatementsToMap();
	};

	void Print(ostream& out)
	{
		// rootStatement.Print(out);
	}

	Statement& GetRootStatement()
	{
		return rootStatement;
	}

	private:
	void SkipSpaces()
	{
		if (pos == string::npos)
			return;
		size_t newPos = line.find_first_not_of(" \t", pos);
		if (pos == 0 && newPos != string::npos) {
			indent = 0;
			for (int ndx = pos; ndx < newPos; ndx++) {
				if (line[ndx] == ' ')
					indent++;
				else indent += _SpacesPerTab;
			}
			indent = indent / _SpacesPerTab;
		}
		pos = newPos;
	}

	string GetQuotedString()
	{
		char quote = line[pos];
		pos++; // skip the quote
		size_t newPos = line.find_first_of(quote, pos);
		if (newPos == string::npos) {
			// handle syntax error
			return "";
		}
		string quotedString = line.substr(pos, newPos - pos);
		pos = newPos + 1;
		SkipSpaces();
		return quotedString;
	}

	string::value_type Peek()
	{
		if (pos == string::npos)
			return 0;
		return line[pos];
	}

	bool GetNextToken(string& token)
	{
		if (!pushToken.empty()) {
			token = pushToken;
			pushToken.clear();
			return true;
		}
		token = "";
		if (parsingState == states::body || parsingState == states::comment 
			|| parsingState == states::assignBlockString)
		{
			if (pos == string::npos) {
				return false; // error eof without completing
			}
			token = line.substr(pos);
			pos = string::npos;
			return true;
		}
		SkipSpaces();
		if (pos == string::npos) {
			return false;
		}
		size_t newPos = line.find_first_of(" =\t\"'", pos);
		foundEquals = false;
		foundQuoted = false;
		if (newPos != string::npos) {
			token = line.substr(pos, newPos - pos);
			foundEquals = line[newPos] == '=';
			foundQuoted = line[newPos] == '"' || line[newPos] == '\'';
			if (foundQuoted) {
				token = GetQuotedString();
			}
			else {
				pos = newPos + 1;
				SkipSpaces();
			}
		}
		else {
			token = line.substr(pos);
			pos = string::npos;
		}
		return true;
	}

};

class Generator
{
	StatementParser& statements;
public:
	Generator(StatementParser& _statements)
		: statements(_statements)
	{

	}

	void replaceAts(stringstream& ss)
	{
		std::string s = ss.str();
		size_t len = s.length();
		bool replaced = false;
		while (replace(s, "@@", "@"))
			replaced = true;
		if (replaced)
			ss.str(s);
	}

	bool Generate(stringstream& output, string templateType)
	{ 
		Statement& rootStatement = statements.GetRootStatement();
		string templatesFile;
		if (rootStatement.GetValue(templateType, templatesFile)) {
			rootStatement.Evaluate(NULL);
			templatesFile = ResolvePath(templatesFile);
			StatementParser templateParser(templatesFile);
			templateParser.ParseStatements();
			templateParser.Print(cout);
			Statement& rootTemplate = templateParser.GetRootStatement();
			rootTemplate.ApplyVariables(rootStatement);
			bool result = rootStatement.EvaluateTemplates(rootTemplate, output);
			replaceAts(output);
			return result;
		}
		return false;
	}
};


int main(int argc, char *argv[])
{
	// "c:\Users\George Cowsar\Projects\TankMateGen2\PageDefs\" Truck.def Truck.aspx Truck.aspx.cs
	if (argc < 3) {
		cout << "usage: " << argv[0] << " <path to .def file>\n";
		return -1;
	}

	// "c:\Users\George Cowsar\Projects\TankMateGen2\PageDefs\" Truck Truck2
		// Truck.def Truck.aspx.templ 
		// Truck.aspx.cs.templ
	string path = argv[1];
	path += "\\";
	for (int ndx = 2; ndx < argc; ndx++) {
		string defFile = argv[ndx];
		string statementsFile = path + defFile + ".def";

		defFileName = defFile + ".def";

		cout << "File: " << defFileName << endl;

		fstream fs;

		cout << "Parse Statements from def file" << endl;
		StatementParser statementParser(statementsFile);
		statementParser.ParseStatements();
		statementParser.GetRootStatement().Evaluate(NULL);
		statementParser.Print(cout);

		cout << "Get aspx and cs file names" << endl;
		string pageClass, outputPath;
		statementParser.GetRootStatement().GetValue("pageClass", pageClass);
		statementParser.GetRootStatement().GetValue("outputPath", outputPath);
		string classPrefix = ResolvePath(outputPath) + pageClass;
		string aspxFile = classPrefix + ".aspx"; // path + pageClass + ".aspx.cs"
		string csFile = classPrefix + ".aspx.cs";

		cout << "Generate aspx file" << endl;
		Generator generator(statementParser);
		stringstream output;
		startComment = startCommentWeb; endComment = endCommentWeb; endOfLine = endOfLineWeb;
		generator.Generate(output, "templates");
		fs.open(aspxFile, fstream::trunc | fstream::out);
		fs << output.str();
		fs.close();
		//cout << output.str();

		cout << "Generate cs file" << endl;
		Generator datamapGenerator(statementParser);
		stringstream datamapOutput;
		startComment = startCommentCode; endComment = endCommentCode; endOfLine = endOfLineCode;
		datamapGenerator.Generate(datamapOutput, "datamap");
		fs.open(csFile, fstream::trunc | fstream::out);
		fs << datamapOutput.str();
		fs.close();
	}
	//cout << datamapOutput.str();

    return 0;
}