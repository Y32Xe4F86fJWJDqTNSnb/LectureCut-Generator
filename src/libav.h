#pragma once

extern "C" 
{
  #include "libavcodec/avcodec.h"
  #include "libavformat/avformat.h"
  #include "libswresample/swresample.h"
}

#include <filesystem>

class Packet
{
public:
  Packet() 
    : m_packet(av_packet_alloc())
  {
  }

  ~Packet() 
  {
    if(m_packet)
      av_packet_free(&m_packet);
  }

  Packet(Packet const & other) = delete;
  Packet & operator = (Packet const & other) = delete;

  Packet(Packet && other) noexcept
    : Packet(std::exchange(other.m_packet, nullptr))
  {
  }

  Packet & operator = (Packet && other) 
  {
    if(this != &other) 
    {
      if(m_packet)
        av_packet_free(&m_packet);
      m_packet = std::exchange(other.m_packet, nullptr);
    }
    return *this;
  }

  void unref() 
  {
    av_packet_unref(m_packet);
  }

  void addFlags(std::same_as<int> auto ... flag) const
  {
    m_packet->flags |= (flag | ...); 
  }

  bool isFlaggedAs(std::same_as<int> auto ... flag) const
  {
    auto const flagCombination = (flag | ...);
    return (m_packet->flags & flagCombination) == flagCombination; 
  }

  bool isNotFlaggedAs(std::same_as<int> auto ... flag) const
  {
    return (m_packet->flags & (flag | ...)) == 0; 
  }

  int64_t & duration() const
  {
    return m_packet->duration;
  }

  int64_t & dts() const
  {
    return m_packet->dts;
  }

  int64_t & pts() const
  {
    return m_packet->pts;
  }

  int64_t getEndPts() const
  {
    return pts() + duration();
  }
  
  operator AVPacket * () noexcept
  {
    return m_packet;
  }

  operator AVPacket const * () const noexcept
  {
    return m_packet;
  }

  AVPacket * operator -> () noexcept
  {
    return m_packet;
  }

  AVPacket const * operator -> () const noexcept
  {
    return m_packet;
  }

private:
  explicit Packet(AVPacket * other) noexcept
    : m_packet(other)
  {
  }
 
private:
  AVPacket * m_packet;
};

class Frame
{
public:
  Frame() 
    : m_frame(av_frame_alloc())
  {
  }

  ~Frame() 
  {
    if(m_frame)
      av_frame_free(&m_frame);
  }

  Frame(Frame const & other) = delete;
  Frame & operator = (Frame const & other) = delete;

  Frame(Frame && other) noexcept
    : Frame(std::exchange(other.m_frame, nullptr))
  {
  }

  Frame & operator = (Frame && other) 
  {
    if(this != &other) 
    {
      if(m_frame)
        av_frame_free(&m_frame);
      m_frame = std::exchange(other.m_frame, nullptr);
    }
    return *this;
  }

  void unref() 
  {
    av_frame_unref(m_frame);
  }

  operator AVFrame * () noexcept
  {
    return m_frame;
  }

  operator AVFrame const * () const noexcept
  {
    return m_frame;
  }

  AVFrame * operator -> () noexcept
  {
    return m_frame;
  }

  AVFrame const * operator -> () const noexcept
  {
    return m_frame;
  }

private:
  explicit Frame(AVFrame * other) noexcept
    : m_frame(other)
  {
  }

private:
  AVFrame * m_frame;
};

class SwrCtx
{
public:
  SwrCtx() noexcept
    : m_ctx(nullptr)
  {
  }

  bool init(AVChannelLayout & out_ch_layout, AVSampleFormat out_sample_fmt, int out_sample_rate, AVChannelLayout & in_ch_layout, AVSampleFormat in_sample_fmt, int in_sample_rate, int log_offset = 0, void * log_ctx = nullptr)
  {
      if(swr_alloc_set_opts2(&m_ctx, &out_ch_layout, out_sample_fmt, out_sample_rate, &in_ch_layout, in_sample_fmt, in_sample_rate, log_offset, log_ctx) < 0) 
        return false;

      if(swr_init(m_ctx) < 0) 
        return false;

      return true;
  }

  std::optional<std::size_t> convert(uint8_t const ** inputs, std::size_t inputSamplesPerChannel, uint8_t ** outputs, std::size_t outputBufferSizePerChannel)
  {
    if(!m_ctx)
      return std::nullopt;
    
    if(
      auto const maxSizeT = std::numeric_limits<int>::max();
      inputSamplesPerChannel > maxSizeT || outputBufferSizePerChannel > maxSizeT
    ){
        throw std::overflow_error("Unintended overflow during conversion of std::size_t to int");
    }

    auto const 
      inputLength = static_cast<int>(inputSamplesPerChannel), 
      outputLength = static_cast<int>(outputBufferSizePerChannel);
 
    if(
      auto const upperBoundOutputSamplesPerChannel = swr_get_out_samples(m_ctx, inputLength);
      upperBoundOutputSamplesPerChannel < 0 || upperBoundOutputSamplesPerChannel > outputLength 
    ){
      return std::nullopt;
    }

    auto const outputSamplesPerChannel = swr_convert(m_ctx, outputs, outputLength, inputs, inputLength);
    if(outputSamplesPerChannel < 0)
      return std::nullopt;
    else
      return outputSamplesPerChannel;
  }

  ~SwrCtx() 
  {
    if(m_ctx) 
      swr_free(&m_ctx);
  }

  operator SwrContext * () noexcept
  {
    return m_ctx;
  }

  operator SwrContext const * () const noexcept
  {
    return m_ctx;
  }

  SwrContext * operator -> () noexcept
  {
    return m_ctx;
  }

  SwrContext const * operator -> () const noexcept
  {
    return m_ctx;
  }

private:
  SwrContext * m_ctx;
};

class FormatContext 
{
public:
  FormatContext() = delete;
  FormatContext(FormatContext const &) = delete;
  FormatContext(FormatContext &&) = delete;
  FormatContext & operator = (FormatContext const &) = delete;
  FormatContext & operator = (FormatContext &&) = delete;

   operator AVFormatContext * () noexcept
  {
    return m_ctx;
  }

  operator AVFormatContext const * () const noexcept
  {
    return m_ctx;
  }

  AVFormatContext * operator -> () noexcept
  {
    return m_ctx;
  }

  AVFormatContext const * operator -> () const noexcept
  {
    return m_ctx;
  }

protected:
  explicit FormatContext(AVFormatContext * ctx) noexcept
    : m_ctx(ctx)
  {
  }

protected:
  AVFormatContext * m_ctx;
};

class InputFormatContext : public FormatContext
{
public:
  InputFormatContext() = delete;
  InputFormatContext(InputFormatContext const &) = delete;
  InputFormatContext & operator = (InputFormatContext const &) = delete;

  InputFormatContext(InputFormatContext && other) noexcept
    : InputFormatContext(std::exchange(other.m_ctx, nullptr))
  {
  };

  InputFormatContext & operator = (InputFormatContext && other) noexcept
  {
    if(this != &other)
      m_ctx = std::exchange(other.m_ctx, nullptr);
    return *this;
  };

  std::optional<Packet> readPacket()
  {
    Packet packet;
    auto const status = av_read_frame(m_ctx, packet);
    if(status < 0)
      return std::nullopt;
    else
      return packet;
  }

  static std::optional<InputFormatContext> open(std::filesystem::path const & url)
  {
      AVFormatContext * ctx = nullptr;
      {
        auto const urlStr = url.string();
        auto const err = avformat_open_input(&ctx, urlStr.c_str(), nullptr, nullptr);
        if(err < 0)
          return std::nullopt;
      }

      auto optInputFormatContext = std::make_optional<InputFormatContext>(ctx);
      auto & inputFormatContext = optInputFormatContext.value();

      {
        auto const err = avformat_find_stream_info(inputFormatContext.m_ctx, nullptr);
        if(err < 0) 
          return std::nullopt;
      }

      return optInputFormatContext;
  }

  ~InputFormatContext()
  {
    if(m_ctx)
      avformat_close_input(&m_ctx);
  }

private:
	InputFormatContext(AVFormatContext * ctx) noexcept
	    : FormatContext(ctx)
	{
  }
};

class OutputFormatContext : public FormatContext
{
public:
  OutputFormatContext() = delete;
  OutputFormatContext(OutputFormatContext const &) = delete;
  OutputFormatContext & operator = (OutputFormatContext const &) = delete;

  OutputFormatContext(OutputFormatContext && other) noexcept
    : OutputFormatContext(std::exchange(other.m_ctx, nullptr))
  {
  };

  OutputFormatContext & operator = (OutputFormatContext && other) noexcept
  {
    if(this != &other)
      m_ctx = std::exchange(other.m_ctx, nullptr);
    return *this;
  };

  static std::optional<OutputFormatContext> open(std::filesystem::path const & url)
  {
      AVFormatContext * ctx = nullptr;
      auto const urlStr = url.string();
      {
        auto const err = avformat_alloc_output_context2(&ctx, nullptr, nullptr, urlStr.c_str());
        if(err < 0)
          return std::nullopt;
      }

      auto optOutputFormatContext = std::make_optional<OutputFormatContext>(ctx);
      auto & outputFormatContext = optOutputFormatContext.value();

      {
        auto const err = avio_open(&outputFormatContext.m_ctx->pb, urlStr.c_str(), AVIO_FLAG_WRITE);
        if(err < 0) 
          return std::nullopt;
      }

      return optOutputFormatContext;
  }

  ~OutputFormatContext()
  {
    if(m_ctx)
    {
      if(m_ctx->pb)
      {
        av_write_trailer(m_ctx);
        avio_closep(&m_ctx->pb);
      }
      avformat_free_context(m_ctx);
    }
  }

private:
	OutputFormatContext(AVFormatContext * ctx) noexcept
	    : FormatContext(ctx)
	{
  }
};

class CodecContext 
{
public:
  CodecContext() = delete;
  CodecContext(CodecContext const &) = delete;
  CodecContext & operator = (CodecContext const &) = delete;

  CodecContext(CodecContext && other) noexcept
    : CodecContext(std::exchange(other.m_ctx, nullptr))
  {
  };

  CodecContext & operator = (CodecContext && other) noexcept
  {
    if(this != &other)
      m_ctx = std::exchange(other.m_ctx, nullptr);
    return *this;
  };

  static std::optional<CodecContext> open(AVCodecParameters const & codecParameters)
  {
      AVCodecContext * ctx = avcodec_alloc_context3(nullptr);
      if(!ctx)
        return std::nullopt;

      {
        auto const err = avcodec_parameters_to_context(ctx, &codecParameters);
        if(err < 0)
        {
          avcodec_free_context(&ctx);
          return std::nullopt;
        }
      }

      auto * codec = avcodec_find_decoder(ctx->codec_id);
      if(!codec) 
      {
        avcodec_free_context(&ctx);
        return std::nullopt;
      }

      {
        auto const err = avcodec_open2(ctx, codec, nullptr);
        if(err < 0) 
        {
          avcodec_free_context(&ctx);
          return std::nullopt;
        }
      }

      return std::make_optional<CodecContext>(ctx);
  }

  template <template <typename, typename> typename C, typename A>
  void readFrames(Packet const & packet, std::back_insert_iterator<C<Frame,A>> it)
  {
    if(avcodec_send_packet(m_ctx, packet) < 0) 
    {
      return; // Error or end of stream.
    }

    auto const checkDecodingStatus = 
      [](int status)
      {
        switch(status) 
        {
          case 0:
            return true;
          case AVERROR(EAGAIN): [[fallthrough]]; // Need more data
          case AVERROR_EOF: // End of stream
            return false;
          default: // Legitimate decoding error
            throw std::runtime_error("Decoding error");
        }
      };

    for(Frame frame; checkDecodingStatus(avcodec_receive_frame(m_ctx, frame)); frame = {}) 
    {
      *it = std::move(frame);
    }
  }

  operator AVCodecContext * () noexcept
  {
    return m_ctx;
  }
  
  operator AVCodecContext const * () const noexcept
  {
    return m_ctx;
  }

  AVCodecContext * operator -> () noexcept
  {
    return m_ctx;
  }
  
  AVCodecContext const * operator -> () const noexcept
  {
    return m_ctx;
  }

  ~CodecContext()
  {
    if(m_ctx)
    {
      avcodec_close(m_ctx);
      avcodec_free_context(&m_ctx);
    }
  }

private:
  CodecContext(AVCodecContext * ctx) noexcept
  : m_ctx(ctx)
  {
  }

private:
  AVCodecContext * m_ctx;
};