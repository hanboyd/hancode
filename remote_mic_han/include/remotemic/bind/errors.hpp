#pragma once

#include <stdexcept>
#include <string>
#include <system_error>

namespace remotemic {

enum class ErrorCode : int {
    None = 0,
    InvalidArgument = 1,
    NotFound = 2,
    Timeout = 3,
    Internal = 4,
};

class ErrorCategory : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override;
    [[nodiscard]] std::string message(int ev) const override;
};

[[nodiscard]] const std::error_category& error_category() noexcept;

class Error : public std::system_error {
public:
    Error(ErrorCode code, std::string what);
    [[nodiscard]] ErrorCode code() const noexcept;
};

} // namespace remotemic