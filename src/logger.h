#pragma once

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/json.hpp>
#include <boost/date_time.hpp>

namespace logging = boost::log;
namespace json = boost::json;
namespace expr = boost::log::expressions;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value);

void InitBoostLog();
void LogFormatJSON(logging::record_view const& rec, 
	logging::formatting_ostream& strm);

