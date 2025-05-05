#pragma once

#include <format>
#include <span>
#include <string_view>
#include <charconv>

template <
  std::convertible_to<std::string_view> ... Stringlike, 
  typename ... ArithmeticValue
>
requires (
  std::conjunction_v<
    std::disjunction<
      std::is_same<ArithmeticValue, bool>,
      std::is_same<ArithmeticValue, int>, 
      std::is_same<ArithmeticValue, float>
    > ...
  >
)
static void updateParamsFromArgs(
  ArgumentResultList args, 
  ErrorCallback * errorCallback,
  std::tuple<Stringlike const &, ArithmeticValue &> ... parameters
){
  for(auto const & arg : std::span(args.args, args.num_args))
  {
    std::string_view const
      argName(arg.name), 
      argVal(arg.value);

    bool const match = (
      [&] (auto && param)
      {
        if(
          auto const & [paramName, paramVarRef] = param;
          argName == static_cast<std::string_view>(paramName)
        ){
          if constexpr(std::is_same_v<std::remove_cvref_t<decltype(paramVarRef)>, bool>)
          {
            paramVarRef = true;
          }
          else if(std::from_chars(argVal.data(), argVal.data() + argVal.size(), paramVarRef).ec != std::errc())
          {
            auto const errorMessage = std::format(
              "Invalid argument value \"{:s}\" supplied to renderer for parameter \"{:s}\"", 
              argVal, paramName
            );
            errorCallback(errorMessage.c_str());
          }
          return true;
        }
        return false;
      }(parameters)
      || ...
    );
    if(!match)
    {
      auto const errorMessage = std::format("Invalid argument value \"{:s}\" supplied to renderer", argVal);
      errorCallback(errorMessage.c_str());
    }
  }
}