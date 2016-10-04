
// A Sample of ORM Lite
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#include <string>
#include <iostream>

#include "src/ORMLite.h"
using namespace BOT_ORM;

class MyClass
{
	// Inject ORM-Lite into this Class
	ORMAP (MyClass, id, score, name)
public:
	long id;
	double score;
	std::string name;
};

int main ()
{
	// Store the Data in "test.db"
	ORMapper<MyClass> mapper ("test.db");

	// Create a table for "MyClass"
	mapper.CreateTbl ();

	// Insert Values into the table
	std::vector<MyClass> initObjs =
	{
		{ 0, 0.2, "John" },
		{ 1, 0.4, "Jack" },
		{ 2, 0.6, "Jess" }
	};
	for (auto obj : initObjs)
		mapper.Insert (obj);

	// Update Entry by KEY (id)
	initObjs[1].score = 1.0;
	mapper.Update (initObjs[1]);

	// Delete Entry by KEY (id)
	mapper.Delete (initObjs[2]);

	// Select All to Vector
	auto query0 = mapper.Query (MyClass ()).ToVector ();
	// query0 = [{ 0, 0.2, "John"},
	//           { 1, 1.0, "Jack"}]

	// If 'Insert' Failed, Print the latest Error Message
	if (!mapper.Insert (MyClass { 1, 0, "Joke" }))
		auto err = mapper.ErrMsg ();
	// err = "SQL error: UNIQUE constraint failed: MyClass.id"

	// ReSeed data :-)
	for (long i = 50; i < 100; i++)
		mapper.Insert (MyClass { i, i * 0.2, "July" });

	// Define a Query Helper Object
	MyClass _mc;

	// Select by Query
	auto query1 = mapper.Query (_mc)    // Link '_mc' to its fields
		.Where (_mc.name, "=", "July")
		.WhereAnd ()
		.WhereBracket (true)
			.Where (_mc.id, "<=", 90)
			.WhereAnd ()
			.Where (_mc.id, ">=", 60)
		.WhereBracket (false)
		.OrderBy (_mc.id, true)
		.Limit (3, 10)
		.ToVector ();

	// Select by SQL
	std::vector<MyClass> query2;
	mapper.Select (query2,
				   "where (name='July' and (id<=90 and id>=50))"
				   " order by id desc"
				   " limit 3 offset 10");

	// Note that: query1 = query2 =
	// [{ 80, 16.0, "July"}, { 79, 15.8, "July"}, { 78, 15.6, "July"}]

	// Count by Query
	auto count1 = mapper.Query (_mc)    // Link '_mc' to its fields
		.Where (_mc.name, "=", "July")
		.Count ();

	// Count by SQL
	auto count2 = mapper.Count ("where (name='July')");

	// Note that:
	// count1 = count2 = 50

	// Delete by Query
	mapper.Query (_mc)                  // Link '_mc' to its fields
		.Where (_mc.name, "=", "July")
		.Delete ();

	// Delete by SQL
	mapper.Delete ("where (name='July')");

	// Drop the table "MyClass"
	mapper.DropTbl ();

	return 0;
}
