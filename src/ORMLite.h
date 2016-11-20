
// ORM Lite
// An ORM for SQLite in C++11
// https://github.com/BOT-Man-JL/ORM-Lite
// BOT Man, 2016

#ifndef BOT_ORM_H
#define BOT_ORM_H

#include <functional>
#include <vector>
#include <list>
#include <string>
#include <cctype>
#include <thread>
#include <sstream>
#include <type_traits>
#include <cstddef>

#include "sqlite3.h"

// Public Macro

#define ORMAP(_MY_CLASS_, ...)                            \
private:                                                  \
friend class BOT_ORM::ORMapper;                           \
template <typename VISITOR, typename FN>                  \
void __Accept (const VISITOR &visitor, FN fn)             \
{                                                         \
    visitor.Visit (fn, __VA_ARGS__);                      \
}                                                         \
template <typename VISITOR, typename FN>                  \
void __Accept (const VISITOR &visitor, FN fn) const       \
{                                                         \
    visitor.Visit (fn, __VA_ARGS__);                      \
}                                                         \
constexpr static const char *__ClassName = #_MY_CLASS_;   \
constexpr static const char *__FieldNames = #__VA_ARGS__; \

// Nullable Template
// http://stackoverflow.com/questions/2537942/nullable-values-in-c/28811646#28811646

namespace BOT_ORM
{
	template <typename T>
	class Nullable final
	{
		template <typename T2>
		friend bool operator== (const Nullable<T2> &op1,
								const Nullable<T2> &op2);
		template <typename T2>
		friend bool operator== (const Nullable<T2> &op,
								const T2 &value);
		template <typename T2>
		friend bool operator== (const T2 &value,
								const Nullable<T2> &op);
		template <typename T2>
		friend bool operator== (const Nullable<T2> &op,
								nullptr_t);
		template <typename T2>
		friend bool operator== (nullptr_t,
								const Nullable<T2> &op);
	public:
		// Default or Null Construction
		Nullable ()
			: m_hasValue (false), m_value (T ()) {}
		Nullable (nullptr_t)
			: Nullable () {}

		// Null Assignment
		const Nullable<T> & operator= (nullptr_t)
		{
			m_hasValue = false;
			m_value = T ();
			return *this;
		}

		// Value Construction
		Nullable (const T &value)
			: m_hasValue (true), m_value (value) {}

		// Value Assignment
		const Nullable<T> & operator= (const T &value)
		{
			m_hasValue = true;
			m_value = value;
			return *this;
		}

	private:
		bool m_hasValue;
		T m_value;

	public:
		const T &Value () const
		{ return m_value; }
	};

	template <typename T2>
	inline bool operator== (const Nullable<T2> &op1,
							const Nullable<T2> &op2)
	{
		return op1.m_hasValue == op2.m_hasValue &&
			(!op1.m_hasValue || op1.m_value == op2.m_value);
	}

	template <typename T2>
	inline bool operator== (const Nullable<T2> &op,
							const T2 &value)
	{
		return op.m_hasValue && op.m_value == value;
	}

	template <typename T2>
	inline bool operator== (const T2 &value,
							const Nullable<T2> &op)
	{
		return op.m_hasValue && op.m_value == value;
	}

	template <typename T2>
	inline bool operator== (const Nullable<T2> &op,
							nullptr_t)
	{
		return !op.m_hasValue;
	}

	template <typename T2>
	inline bool operator== (nullptr_t,
							const Nullable<T2> &op)
	{
		return !op.m_hasValue;
	}
}

namespace BOT_ORM_Impl
{
	class SQLConnector
	{
	public:
		SQLConnector (const std::string &fileName)
		{
			auto rc = sqlite3_open (fileName.c_str (), &db);
			if (rc)
			{
				sqlite3_close (db);
				throw std::runtime_error (
					std::string ("Can't open database: %s\n")
					+ sqlite3_errmsg (db));
			}
		}

		~SQLConnector ()
		{
			sqlite3_close (db);
		}

		// Don't throw in callback
		void Execute (const std::string &cmd,
					  std::function<void (int argc, char **argv,
										  char **azColName) noexcept>
					  callback = _callback)
		{
			const size_t MAX_TRIAL = 16;
			char *zErrMsg = 0;
			int rc;

			auto callbackWrapper = [] (void *fn, int argc, char **argv,
									   char **azColName)
			{
				static_cast<std::function
					<void (int argc,
						   char **argv,
						   char **azColName) noexcept> *>
						   (fn)->operator()(argc, argv, azColName);
				return 0;
			};

			for (size_t iTry = 0; iTry < MAX_TRIAL; iTry++)
			{
				rc = sqlite3_exec (db, cmd.c_str (), callbackWrapper,
								   &callback, &zErrMsg);
				if (rc != SQLITE_BUSY)
					break;

				std::this_thread::sleep_for (std::chrono::microseconds (20));
			}

			if (rc != SQLITE_OK)
			{
				auto errStr = std::string ("SQL error: ") + zErrMsg;
				sqlite3_free (zErrMsg);
				throw std::runtime_error (errStr);
			}
		}

	private:
		sqlite3 *db;
		static void _callback (int argc, char **argv, char **azColName)
		{ return; }
	};

	// Helper - Serialize
	template <typename T>
	inline std::ostream &SerializeValue (
		std::ostream &os,
		const T &value)
	{
		return os << value;
	}

	template <>
	inline std::ostream &SerializeValue <std::string> (
		std::ostream &os,
		const std::string &value)
	{
		return os << "'" << value << "'";
	}

	template <typename T>
	inline std::ostream &SerializeValue (
		std::ostream &os,
		const BOT_ORM::Nullable<T> &value)
	{
		if (value == nullptr)
			return os << "null";
		return SerializeValue (os, value.Value ());
	}

	// Helper - Deserialize
	template <typename T>
	inline void DeserializeValue (
		T &property,
		const BOT_ORM::Nullable<std::string> &value)
	{
		if (value == nullptr)
			throw std::runtime_error ("Null to Deserialze");
		std::stringstream ostr (value.Value ());
		ostr >> property;
	}

	template <>
	inline void DeserializeValue <std::string> (
		std::string &property,
		const BOT_ORM::Nullable<std::string> &value)
	{
		if (value == nullptr)
			throw std::runtime_error ("Null to Deserialze");
		property = std::move (value.Value ());
	}

	template <typename T>
	inline void DeserializeValue (
		BOT_ORM::Nullable<T> &property,
		const BOT_ORM::Nullable<std::string> &value)
	{
		if (value == nullptr)
			property = nullptr;
		else
		{
			T val;
			DeserializeValue (val, value);
			property = val;
		}
	}

	// Helper - Get TypeString
	template <typename T>
	const char *TypeString (const T &)
	{
		constexpr static const char *typeStr =
			(std::is_integral<T>::value &&
			 !std::is_same<std::remove_cv_t<T>, char>::value &&
			 !std::is_same<std::remove_cv_t<T>, wchar_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char16_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char32_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, unsigned char>::value)
			? "integer not null"
			: (std::is_floating_point<T>::value)
			? "real not null"
			: (std::is_same<std::remove_cv_t<T>, std::string>::value)
			? "text not null"
			: nullptr;

		static_assert (
			typeStr != nullptr,
			"Only Support Integral, Floating Point and std::string :-)");

		return typeStr;;
	}

	template <typename T>
	const char *TypeString (const BOT_ORM::Nullable<T> &)
	{
		constexpr static const char *typeStr =
			(std::is_integral<T>::value &&
			 !std::is_same<std::remove_cv_t<T>, char>::value &&
			 !std::is_same<std::remove_cv_t<T>, wchar_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char16_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, char32_t>::value &&
			 !std::is_same<std::remove_cv_t<T>, unsigned char>::value)
			? "integer"
			: (std::is_floating_point<T>::value)
			? "real"
			: (std::is_same<std::remove_cv_t<T>, std::string>::value)
			? "text"
			: nullptr;

		static_assert (
			typeStr != nullptr,
			"Only Support Integral, Floating Point and std::string :-)");

		return typeStr;;
	}

	// Visitor
	class FnVisitor
	{
	public:
		template <typename Fn, typename... Args>
		inline void Visit (Fn fn, Args & ... args) const
		{
			_Visit (fn, args...);
		}

	protected:
		template <typename Fn, typename T, typename... Args>
		inline void _Visit (Fn fn, T &property, Args & ... args) const
		{
			if (_Visit (fn, property))
				_Visit (fn, args...);
		}

		template <typename Fn, typename T>
		inline bool _Visit (Fn fn, T &property) const
		{
			return fn (property);
		}
	};
}

namespace BOT_ORM
{
	class ORMapper
	{
	public:
		ORMapper (const std::string &connectionString)
			: _connector (connectionString) {}

		template <typename Fn>
		void Transaction (Fn fn)
		{
			try
			{
				_connector.Execute ("begin transaction;");
				fn ();
				_connector.Execute ("commit transaction;");
			}
			catch (...)
			{
				_connector.Execute ("rollback transaction;");
				throw;
			}
		}

		template <typename C>
		void CreateTbl (const C &entity)
		{
			const auto &fieldNames = ORMapper::_FieldNames<C> ();
			std::vector<std::string> strTypes (fieldNames.size ());
			size_t index = 0;

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&strTypes, &index] (auto &val)
			{
				strTypes[index++] = BOT_ORM_Impl::TypeString (val);
				return true;
			});

			auto strFmt = fieldNames[0] + " " + strTypes[0] +
				" primary key,";
			for (size_t i = 1; i < fieldNames.size (); i++)
				strFmt += fieldNames[i] + " " + strTypes[i] + ",";
			strFmt.pop_back ();

			_connector.Execute ("create table " +
								std::string (C::__ClassName) +
								"(" + strFmt + ");");
		}

		template <typename C>
		void DropTbl (const C &)
		{
			_connector.Execute ("drop table " +
								std::string (C::__ClassName) +
								";");
		}

		template <typename C>
		void Insert (const C &entity)
		{
			std::ostringstream os;
			size_t index = 0;
			auto fieldCount = ORMapper::_FieldNames<C> ().size ();

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&os, &index, fieldCount] (auto &val)
			{
				if (++index != fieldCount)
					BOT_ORM_Impl::SerializeValue (os, val) << ',';
				else
					BOT_ORM_Impl::SerializeValue (os, val);
				return true;
			});

			_connector.Execute ("insert into " +
								std::string (C::__ClassName) +
								" values (" + os.str () + ");");
		}

		template <typename In>
		void InsertRange (const In &entities)
		{
			using C = typename In::value_type;

			std::ostringstream os;
			size_t count = 0;
			auto fieldCount = ORMapper::_FieldNames<C> ().size ();

			for (const auto &entity : entities)
			{
				os << "(";

				size_t index = 0;
				entity.__Accept (BOT_ORM_Impl::FnVisitor (),
								 [&os, &index, fieldCount] (auto &val)
				{
					if (++index != fieldCount)
						BOT_ORM_Impl::SerializeValue (os, val) << ',';
					else
						BOT_ORM_Impl::SerializeValue (os, val);
					return true;
				});

				if (++count != entities.size ())
					os << "),";
				else
					os << ")";
			}

			_connector.Execute ("insert into " +
								std::string (C::__ClassName) +
								" values " + os.str () + ";");
		}

		template <typename C>
		void Update (const C &entity)
		{
			std::stringstream os, osKey;
			size_t index = 0;
			const auto &fieldNames = ORMapper::_FieldNames<C> ();

			os << "set ";
			osKey << "where ";

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&os, &osKey, &index, &fieldNames] (auto &val)
			{
				if (index == 0)
				{
					osKey << fieldNames[index++] << "=";
					BOT_ORM_Impl::SerializeValue (osKey, val);
				}
				else
				{
					os << fieldNames[index++] << "=";
					if (index != fieldNames.size ())
						BOT_ORM_Impl::SerializeValue (os, val) << ',';
					else
						BOT_ORM_Impl::SerializeValue (os, val);
				}
				return true;
			});

			_Update (entity, os.str (), osKey.str ());
		}

		template <typename In>
		void UpdateRange (const In &entities)
		{
			using C = typename In::value_type;

			std::stringstream os;
			const auto &fieldNames = ORMapper::_FieldNames<C> ();

			for (const auto &entity : entities)
			{
				os << "update "
					<< C::__ClassName
					<< " set ";

				size_t index = 0;
				std::stringstream osKey;

				entity.__Accept (
					BOT_ORM_Impl::FnVisitor (),
					[&os, &osKey, &index, &fieldNames] (auto &val)
				{
					if (index == 0)
					{
						osKey << fieldNames[index++] << "=";
						BOT_ORM_Impl::SerializeValue (osKey, val);
					}
					else
					{
						os << fieldNames[index++] << "=";
						if (index != fieldNames.size ())
							BOT_ORM_Impl::SerializeValue (os, val) << ',';
						else
							BOT_ORM_Impl::SerializeValue (os, val);
					}
					return true;
				});
				os << " where " + osKey.str () + ";";
			}
			_connector.Execute (os.str ());
		}

		template <typename C>
		void Delete (const C &entity)
		{
			std::stringstream os;
			os << "where ";

			entity.__Accept (BOT_ORM_Impl::FnVisitor (),
							 [&os] (auto &val)
			{
				os << ORMapper::_FieldNames<C> ()[0] << "=";
				BOT_ORM_Impl::SerializeValue (os, val);
				return false;
			});

			_Delete (entity, os.str ());
		}

		struct Expr
		{
			std::vector<std::pair<const void *, std::string>> expr;

			template <typename T>
			Expr (const T &property,
				  const std::string &relOp = "=")
				: Expr (property, relOp, property)
			{}

			template <typename T>
			Expr (const T &property,
				  const std::string &relOp,
				  const T &value)
				: expr { std::make_pair (&property, relOp) }
			{
				std::ostringstream os;
				BOT_ORM_Impl::SerializeValue (os, value);
				expr.front ().second.append (os.str ());
			}

			template <typename T>
			Expr (const Nullable<T> &property,
				  bool isNull)
				: expr { std::make_pair (
					&property, isNull ? " is null" : " is not null") }
			{}

			template <typename T>
			struct Expr_Field
			{
				const T& _property;
				Expr_Field (const T &property)
					: _property (property) {}

				inline Expr operator == (T value)
				{ return Expr { _property, "=", std::move (value) }; }
				inline Expr operator != (T value)
				{ return Expr { _property, "!=", std::move (value) }; }

				inline Expr operator == (nullptr_t)
				{ return Expr { _property, true }; }
				inline Expr operator != (nullptr_t)
				{ return Expr { _property, false }; }

				inline Expr operator > (T value)
				{ return Expr { _property, ">", std::move (value) }; }
				inline Expr operator >= (T value)
				{ return Expr { _property, ">=", std::move (value) }; }

				inline Expr operator < (T value)
				{ return Expr { _property, "<", std::move (value) }; }
				inline Expr operator <= (T value)
				{ return Expr { _property, "<=", std::move (value) }; }
			};

			inline Expr operator && (const Expr &right)
			{
				return And_Or (right, " and ");
			}

			inline Expr operator || (const Expr &right)
			{
				return And_Or (right, " or ");
			}

		private:
			Expr And_Or (const Expr &right, std::string logOp)
			{
				constexpr void *ptr = nullptr;

				expr.emplace (
					expr.begin (),
					std::make_pair (ptr, std::string ("("))
				);
				expr.emplace_back (
					std::make_pair (ptr, std::move (logOp))
				);
				for (const auto exprPair : right.expr)
					expr.emplace_back (exprPair);
				expr.emplace_back (
					std::make_pair (ptr, std::string (")"))
				);

				return *this;
			}
		};

		template <typename C>
		class ORQuery
		{
		public:
			ORQuery (const C &queryHelper, ORMapper *pMapper)
				: _queryHelper (queryHelper), _pMapper (pMapper) {}

			// Where
			inline ORQuery &Where (const Expr &expr)
			{
				_sqlWhere.clear ();
				for (const auto exprStr : expr.expr)
				{
					if (exprStr.first != nullptr)
						_sqlWhere += _GetFieldName (exprStr.first);
					_sqlWhere += exprStr.second;
				}
				return *this;
			}

			// Order By
			template <typename T>
			inline ORQuery &OrderBy (const T &property)
			{
				_sqlOrderBy =
					" order by " +
					_GetFieldName (&property);
				return *this;
			}

			template <typename T>
			inline ORQuery &OrderByDescending (const T &property)
			{
				_sqlOrderBy =
					" order by " +
					_GetFieldName (&property) +
					" desc";
				return *this;
			}

			// Limit and Offset
			inline ORQuery &Take (size_t count)
			{
				_sqlLimit = " limit " + std::to_string (count);
				return *this;
			}

			inline ORQuery &Skip (size_t count)
			{
				// Error Syntax
				if (_sqlLimit.empty ()) Take (0);

				_sqlOffset = " offset " + std::to_string (count);
				return *this;
			}

			// Retrieve Select Result
			std::vector<C> ToVector ()
			{
				static_assert (std::is_copy_constructible<C>::value,
							   "It must be Copy Constructible");

				std::vector<C> ret;
				_pMapper->_Select (_queryHelper, ret, _GetCondSql ());
				return std::move (ret);
			}

			std::list<C> ToList ()
			{
				static_assert (std::is_copy_constructible<C>::value,
							   "It must be Copy Constructible");

				std::list<C> ret;
				_pMapper->_Select (_queryHelper, ret, _GetCondSql ());
				return std::move (ret);
			}

			// Update Values
			template <typename... Args>
			inline void Update (const Args & ... fields)
			{
				std::stringstream setSql;
				setSql << "set ";
				_Set (setSql, fields...);
				_pMapper->_Update (_queryHelper,
								   setSql.str (),
								   _GetCondSql<true> ());
			}

			// Delete Values
			inline void Delete ()
			{
				_pMapper->_Delete (_queryHelper,
								   _GetCondSql<true> ());
			}

			// Aggregate
			inline size_t Count ()
			{
				return (size_t) std::stoull (
					_pMapper->_Aggregate (_queryHelper, "count",
										  "*", _GetCondSql ())
				);
			}

			template <typename T>
			T Sum (const T &property)
			{
				T ret;
				BOT_ORM_Impl::DeserializeValue (
					ret,
					_pMapper->_Aggregate (_queryHelper, "sum",
										  _GetFieldName (&property),
										  _GetCondSql ())
				);
				return ret;
			}

			template <typename T>
			T Avg (const T &property)
			{
				T ret;
				BOT_ORM_Impl::DeserializeValue (
					ret,
					_pMapper->_Aggregate (_queryHelper, "avg",
										  _GetFieldName (&property),
										  _GetCondSql ())
				);
				return ret;
			}

			template <typename T>
			T Max (const T &property)
			{
				T ret;
				BOT_ORM_Impl::DeserializeValue (
					ret,
					_pMapper->_Aggregate (_queryHelper, "max",
										  _GetFieldName (&property),
										  _GetCondSql ())
				);
				return ret;
			}

			template <typename T>
			T Min (const T &property)
			{
				T ret;
				BOT_ORM_Impl::DeserializeValue (
					ret,
					_pMapper->_Aggregate (_queryHelper, "min",
										  _GetFieldName (&property),
										  _GetCondSql ())
				);
				return ret;
			}

		protected:
			const C &_queryHelper;
			ORMapper *_pMapper;

			std::string _sqlWhere;
			std::string _sqlOrderBy;
			std::string _sqlLimit, _sqlOffset;

			std::string _GetFieldName (const void *property)
			{
				size_t index = 0;

				_queryHelper.__Accept (BOT_ORM_Impl::FnVisitor (),
									   [&property, &index] (auto &val)
				{
					if (property == &val)
						return false;

					index++;
					return true;
				});

				const auto &fieldNames =
					_pMapper->ORMapper::_FieldNames<C> ();
				if (index == fieldNames.size ())
					throw std::runtime_error ("No such Field in the Table");
				return fieldNames[index];
			}

			template <bool onlyWhere = false>
			std::string _GetCondSql () const
			{
				if (onlyWhere)
				{
					if (!_sqlWhere.empty ())
						return " where (" + _sqlWhere + ")";
					else
						return "";
				}

				if (!_sqlWhere.empty ())
					return " where (" + _sqlWhere + ")" +
					_sqlOrderBy + _sqlLimit + _sqlOffset;
				else
					return _sqlOrderBy + _sqlLimit + _sqlOffset;
			}

			template <typename T, typename... Args>
			inline void _Set (std::ostream &os,
							  const T &arg,
							  const Args & ... args)
			{
				_Set (os, arg);
				os << ",";
				_Set (os, args...);
			}

			template <typename T>
			inline void _Set (std::ostream &os,
							  const T &arg)
			{
				os << _GetFieldName (&arg) << "=";
				BOT_ORM_Impl::SerializeValue (os, arg);
			}
		};

		template <typename C>
		ORQuery<C> Query (const C &queryHelper)
		{
			return ORQuery<C> (queryHelper, this);
		}

	private:
		BOT_ORM_Impl::SQLConnector _connector;

		template <typename C, typename Out>
		void _Select (C copy, Out &out,
					  const std::string &sqlCond)
		{
			auto fieldCount = ORMapper::_FieldNames<C> ().size ();

			_connector.Execute ("select * from " +
								std::string (C::__ClassName) +
								" " + sqlCond + ";",
								[&] (int argc, char **argv, char **)
			{
				if (argc != fieldCount)
					throw std::runtime_error (
						"Inter ORM Error: Select argc != Field Count");

				size_t index = 0;
				copy.__Accept (
					BOT_ORM_Impl::FnVisitor (),
					[&argv, &index] (auto &val)
				{
					if (argv[index])
						BOT_ORM_Impl::DeserializeValue (
							val, BOT_ORM::Nullable<std::string> (argv[index]));
					else
						BOT_ORM_Impl::DeserializeValue (
							val, BOT_ORM::Nullable<std::string> (nullptr));
					index++;
					return true;
				});
				out.emplace_back (std::move (copy));
			});
		}

		template <typename C>
		std::string _Aggregate (const C &,
								const std::string &func,
								const std::string &field,
								const std::string &sqlCond)
		{
			std::string ret;

			_connector.Execute ("select " + func +
								" (" + field + ") as agg from " +
								std::string (C::__ClassName) +
								" " + sqlCond + ";",
								[&] (int argc, char **argv, char **)
			{
				if (argv[0]) ret = argv[0];
			});

			if (ret.empty ())
				throw std::runtime_error ("SQL error: Return null at"
										  " 'select " + func +
										  " (" + field + ") as agg from " +
										  std::string (C::__ClassName) +
										  " " + sqlCond + ";'");
			return ret;
		}

		template <typename C>
		void _Update (const C &,
					  const std::string &sqlSet,
					  const std::string &sqlCond)
		{
			_connector.Execute ("update " +
								std::string (C::__ClassName) +
								" " + sqlSet +
								" " + sqlCond + ";");
		}

		template <typename C>
		void _Delete (const C &,
					  const std::string &sqlCond)
		{
			_connector.Execute ("delete from " +
								std::string (C::__ClassName) +
								" " + sqlCond + ";");
		}

		template <typename C>
		static const std::vector<std::string> &_FieldNames ()
		{
			auto _ExtractFieldName = [] ()
			{
				std::string rawStr (C::__FieldNames), tmpStr;
				rawStr.push_back (',');
				tmpStr.reserve (rawStr.size ());

				std::vector<std::string> ret;
				for (const auto &ch : rawStr)
				{
					switch (ch)
					{
					case ',':
						ret.push_back (tmpStr);
						tmpStr.clear ();
						break;

					default:
						if (isalnum (ch) || ch == '_')
							tmpStr += ch;
						break;
					}
				}
				return std::move (ret);
			};

			static const std::vector<std::string> _fieldNames (
				_ExtractFieldName ());
			return _fieldNames;
		}
	};

	template <typename T>
	ORMapper::Expr::Expr_Field<T> Field (T &property)
	{
		return ORMapper::Expr::Expr_Field<T> { property };
	}
}

#endif // !BOT_ORM_H