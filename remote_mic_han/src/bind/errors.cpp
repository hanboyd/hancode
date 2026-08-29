#include "remotemic/bind/errors.hpp"

namespace remotemic {

const char* ErrorCategory::name() const noexcept {
    return "remotemic";
}

std::string ErrorCategory::message(int ev) const {
    switch (static_cast<ErrorCode>(ev)) {
    case ErrorCode::None:
        return "no error";
    case ErrorCode::InvalidArgument:
        return "invalid argument";
    case ErrorCode::NotFound:
        return "not found";
    case ErrorCode::Timeout:
        return "operation timed out";
    case ErrorCode::Internal:
        return "internal error";
    }
    return "unknown remotemic error";
}

const std::error_category& error_category() noexcept {
    static ErrorCategory instance;
    return instance;
}

Error::Error(ErrorCode code, std::string what)
    : std::system_error(static_cast<int>(code), error_category(), std::move(what)) {}

ErrorCode Error::code() const noexcept {
    return static_cast<ErrorCode>(std::system_error::code().value());
}

} // namespace remotemic