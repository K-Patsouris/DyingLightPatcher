#include "libzippp.h"

#include "logger.h"
#include "StringParser.h"
#include "ConsoleHandler.h"


#include <fstream>
#include <sstream>
#include <iostream>
using std::cout;


using namespace libzippp;


int main(int argc, char** argv) {

	/*ZipArchive z1("data0.pak");
	z1.open(ZipArchive::Write);
	string name{ "asd.temp" };
	z1.addFile("scripts/asd.temp", name);
	if (z1.hasEntry("asd.temp"))
		cout << "Has entry!";
	else
		cout << "Does not have entry";
	system("pause");
	return 0;*/

	logger.Info("Logger init? {}"sv, logger.Init());
	//ConsoleHandler::GetSingleton().CharTest(); return 0;
	ConsoleHandler::GetSingleton().Start();
	logger.Close();
	return 0;


	// Mock code follows

	cout << '\n';

	string baseName{ "dw_weather_def.scr" };
	cout << "Parsing <" << baseName << "> with <" << baseName << ".txt> ...\n";

	//std::ifstream fs(string{ baseName + ".txt" });
	std::ifstream fs(baseName + ".txt");
	if (!fs.is_open()) {
		std::cout << "\nFailed to open " << baseName << ".txt";
		system("pause");
		return -1;
	}
	std::stringstream ss{};
	ss << fs.rdbuf();
	const string difftext = ss.str();
	fs.close();

	//logger.Info("\n\n\tINPUT DIFF: <\n{}\n>"sv, difftext);

	StringParser::Parser parser{};
	if (!parser.SetDiff(difftext))
		std::cout << "\nparser.SetDiff() failed!\n";
	else
		std::cout << "\nparser.SetDiff() succeeded!\n";

	
	std::ifstream fs2(baseName);
	if (!fs2.is_open()) {
		std::cout << "\nFailed to open " << baseName;
		std::cout << "\n\nPress any key...";
		system("pause");
		return -1;
	}
	std::stringstream ss2{};
	ss2 << fs2.rdbuf();
	const string targettext = ss2.str();
	fs2.close();
	
	//logger.Info("\n\nINPUT TARGET: <\n{}\n>"sv, targettext);

	if (!parser.SetTarget(targettext))
		std::cout << "\nparser.SetTarget() failed!\n";
	else
		std::cout << "\nparser.SetTarget() succeeded!\n";


	parser.PrintTrees();


	string result{};
	if (!parser.Parse(result))
		std::cout << "\nparser.Parse() failed!\n";
	else
		std::cout << "\nparser.Parse() succeeded!\n";
	
	std::ofstream fs3("parsed.scr");
	fs3 << result;
	fs3.close();





	system("pause");
	return 0;
}




