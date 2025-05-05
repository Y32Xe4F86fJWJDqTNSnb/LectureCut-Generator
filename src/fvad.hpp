#pragma once

#include "fvad.h"

#include <optional>

#include <stdexcept>

namespace VAD
{
    enum class Mode : unsigned int
    {
        quality = 0, 
        low_bitrate,
        aggressive,
        very_aggressive
    };

    enum class SampleRate : unsigned int
    {
        sr8000hz = 0,
        sr16000hz,
        sr32000hz,
        sr48000hz,
    };

    class VoiceActivityDetection
    {
    public:
        static std::optional<VoiceActivityDetection> create(Mode mode, SampleRate sampleRate)
        {
            auto fvad = fvad_new();
            if(!fvad)
                return std::nullopt;

            auto optVad = std::make_optional<VoiceActivityDetection>(fvad);
            auto & vad = optVad.value();

            auto const aggressiveness = static_cast<int>(mode);
            if(aggressiveness > 3)
                return std::nullopt;
            fvad_set_mode(vad, static_cast<int>(mode));

            auto intSampleRate = 
                [&sampleRate] () {
                    switch(sampleRate)
                    {
                        case SampleRate::sr8000hz: return 8000;
                        case SampleRate::sr16000hz: return 16000;
                        case SampleRate::sr32000hz: return 32000;
                        case SampleRate::sr48000hz: return 48000;
                        default: return -1;
                    }
                }();
            if(intSampleRate == -1)
                return std::nullopt;
            fvad_set_sample_rate(vad, intSampleRate);

            return optVad;
        }

        VoiceActivityDetection(VoiceActivityDetection const & other) = delete;
        VoiceActivityDetection & operator = (VoiceActivityDetection const & other) = delete;

        VoiceActivityDetection(VoiceActivityDetection && other) noexcept
            : VoiceActivityDetection(std::exchange(other.m_fvad, nullptr))
        {
        }

        VoiceActivityDetection & operator = (VoiceActivityDetection && other) 
        {
            if(this != &other) 
            {
                if(m_fvad)
                    fvad_free(m_fvad);
                m_fvad = std::exchange(other.m_fvad, nullptr);
            }
            return *this;
        }

        ~VoiceActivityDetection()
        {
            if(m_fvad)
                fvad_free(m_fvad);
        }

        bool process(const int16_t * frame, std::size_t length)
        {   
            auto result = fvad_process(m_fvad, frame, length);

            if(result == -1)
                throw std::runtime_error("Invalid frame length");
            else
                return result;
        }
    
        operator Fvad * ()
        {
        return m_fvad; 
        }

        operator Fvad const * () const
        {
        return m_fvad; 
        }

        Fvad * operator -> ()
        {
            return m_fvad;
        }

        Fvad const * operator -> () const
        {
            return m_fvad;
        }

    private:
        VoiceActivityDetection(Fvad * fvad) noexcept
            : m_fvad(fvad)
        {
        }

    private:
        Fvad * m_fvad;
    };
}