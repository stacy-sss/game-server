#include "logger.h"
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/expressions.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>

namespace logging = boost::log;
namespace json = boost::json;
namespace expr = boost::log::expressions;

void InitBoostLog() {

    logging::add_common_attributes();
    logging::add_console_log(
        std::clog,
        logging::keywords::format = &LogFormatJSON
    );
}

void LogFormatJSON(logging::record_view const& rec,
    logging::formatting_ostream& strm) {
    auto ts = rec[timestamp];
    std::string timestamp = boost::posix_time::to_iso_extended_string(*ts);

    auto message = rec[expr::smessage];

    json::value data;
    if (auto data_ptr = rec[additional_data]) {
        data = *data_ptr;
    }

    json::object log_obj;
    log_obj["timestamp"] = timestamp;
    log_obj["message"] = message.get<std::string>();
    log_obj["data"] = data;

    strm << json::serialize(log_obj);
}

