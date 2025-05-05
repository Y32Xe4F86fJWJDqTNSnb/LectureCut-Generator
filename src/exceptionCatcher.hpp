#pragma once

#include <exception>
#include <optional>
#include <string>
#include <format>
#include <functional>

class ExceptionCatcher
{
  public:
    ExceptionCatcher() = default;
    ExceptionCatcher(ExceptionCatcher const & other) = delete;
    ExceptionCatcher & operator = (ExceptionCatcher const & other) = delete;
    ExceptionCatcher(ExceptionCatcher && other) = default;
    ExceptionCatcher & operator = (ExceptionCatcher && other) = default;

    void operator () (auto && callable, auto && ... args)
    {
      try 
      {
        callable(std::forward<decltype(args)>(args) ...);
      }
      catch (...) 
      {
        exceptionPointer = std::current_exception();
      }
    }

    void rethrow()
    {
      if(exceptionPointer) 
      {
        std::rethrow_exception(exceptionPointer);
      }
    }

    std::optional<std::string> getMessage()
    {
        if(exceptionPointer) 
        {
          try 
          {
              std::rethrow_exception(exceptionPointer);
          }
          catch (std::exception const & exc)
          {
              return std::format("Worker thread exception message: {:s}\n", exc.what());
          }
          catch (...)
          {
              return std::format("Worker thread exception.");
          }
        }

        return std::nullopt;
    }

  private:
    std::exception_ptr exceptionPointer;
};

class ExceptionCaughtThread
{
public:
  template <typename ... Args>
  ExceptionCaughtThread(std::function<void(char const *)> errorCallback, std::invocable<Args ...> auto && callable, Args && ... args)
  {
    m_errorCallback = std::move(errorCallback);

    m_thread = 
      std::thread(
        std::ref(m_excCatcher), 
        std::forward<decltype(callable)>(callable), 
        [](auto && arg){
          using ArgType = decltype(arg);

          if constexpr(std::is_lvalue_reference_v<ArgType>)
            if constexpr(std::is_const_v<std::remove_reference_t<ArgType>>)
              return std::cref(arg);
            else
              return std::ref(arg);
          else
            return std::forward<ArgType>(arg);
        }(args)
        ...
      );
  }

  ExceptionCaughtThread(ExceptionCaughtThread const & other) = delete;
  ExceptionCaughtThread & operator = (ExceptionCaughtThread const & other) = delete;
  ExceptionCaughtThread(ExceptionCaughtThread && other) = default;
  ExceptionCaughtThread & operator = (ExceptionCaughtThread && other) = default;

  void join()
  {
    if(!m_thread.joinable())
      throw std::invalid_argument("Thread not joinable");
    
    m_thread.join();

    if(
      auto optErrorMessage = m_excCatcher.getMessage();
      optErrorMessage.has_value()
    ){
      m_errorCallback(optErrorMessage->c_str());
    }
  }

  ~ExceptionCaughtThread()
  {
    if(m_thread.joinable())
      join();
  }

private:
  ExceptionCatcher m_excCatcher;
  std::thread m_thread;
  std::function<void(char const *)> m_errorCallback;
};